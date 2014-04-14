#pragma once

#include <cstdint>
#include <utility>
#include <tuple>
#include <unordered_map>
#include <memory>
#include <boost/lexical_cast.hpp>

#include "common.h"
#include "http_response.h"
#include "http_request.h"
#include "utility.h"

//TEST
#include <iostream>

namespace flask
{
    class BaseRule
    {
    public:
        BaseRule(std::string rule)
            : rule_(std::move(rule))
        {
        }

        virtual ~BaseRule()
        {
        }
        
        BaseRule& name(std::string name)
        {
            name_ = std::move(name);
            return *this;
        }

        virtual void validate()
        {
        }

        virtual response handle(const request&, const routing_params&)
        {
            return response(400);
        }

    protected:
        std::string rule_;
        std::string name_;
    };

    class Rule : public BaseRule
    {
    public:
        Rule(std::string rule)
            : BaseRule(std::move(rule))
        {
        }
        
        Rule& name(std::string name)
        {
            name_ = std::move(name);
            return *this;
        }

        template <typename Func>
        void operator()(Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<>>::value, 
                "Handler type is mismatched with URL paramters");
            static_assert(!std::is_same<void, decltype(f())>::value, 
                "Handler function cannot have void return type; valid return types: string, int, flask::resposne");
            handler_ = [f = std::move(f)]{
                return response(f());
            };
        }

        template <typename Func>
        void operator()(std::string name, Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<>>::value, 
                "Handler type is mismatched with URL paramters");
            name_ = std::move(name);
            handler_ = [f = std::move(f)]{
                return response(f());
            };
        }

        void validate()
        {
            if (!handler_)
            {
                throw std::runtime_error(name_ + (!name_.empty() ? ": " : "") + "no handler for url " + rule_);
            }
        }

        response handle(const request&, const routing_params&)
        {
            return handler_();
        }

    protected:
        std::function<response()> handler_;
    };

    template <typename ... Args>
    class TaggedRule : public BaseRule
    {
    private:
        template <typename F, int NInt, int NUint, int NDouble, int NString, typename S1, typename S2> struct call
        {
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
        struct call<F, NInt, NUint, NDouble, NString, black_magic::S<int64_t, Args1...>, black_magic::S<Args2...>>
        {
            response operator()(F& handler, const routing_params& params)
            {
                using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<int64_t, NInt>>;
                return call<F, NInt+1, NUint, NDouble, NString,
                       black_magic::S<Args1...>, pushed>()(handler, params);
            }
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1> 
        struct call<F, NInt, NUint, NDouble, NString, black_magic::S<>, black_magic::S<Args1...>>
        {
            response operator()(F& handler, const routing_params& params)
            {
                return handler(
                    params.get<typename Args1::type>(Args1::pos)... 
                );
                //return response(500);
            }
        };
    public:
        TaggedRule(std::string rule)
            : BaseRule(std::move(rule))
        {
        }
        
        TaggedRule<Args...>& name(std::string name)
        {
            name_ = std::move(name);
            return *this;
        }

        template <typename Func>
        void operator()(Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<Args...>>::value, 
                "Handler type is mismatched with URL paramters");
            handler_ = [f = std::move(f)](Args ... args){
                return response(f(args...));
            };
        }

        template <typename Func>
        void operator()(std::string name, Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<Args...>>::value, 
                "Handler type is mismatched with URL paramters");
            name_ = std::move(name);
            handler_ = [f = std::move(f)](Args ... args){
                return response(f(args...));
            };
        }

        response handle(const request&, const routing_params& params)
        {
            //return handler_();
            return call<decltype(handler_), 0, 0, 0, 0, black_magic::S<Args...>, black_magic::S<>>()(handler_, params);
            //return response(500);
        }

    private:
        std::function<response(Args...)> handler_;

        template <typename T, int Pos>
        struct call_pair
        {
            using type = T;
            static const int pos = Pos;
        };

    };

    class Trie
    {
    public:
        struct Node
        {
            std::unordered_map<std::string, unsigned> children;
            unsigned rule_index{};
            std::array<unsigned, (int)ParamType::MAX> param_childrens{};
            bool IsSimpleNode() const
            {
                return 
                    !rule_index &&
                    std::all_of(
                        std::begin(param_childrens), 
                        std::end(param_childrens), 
                        [](unsigned x){ return !x; });
            }
        };

        Trie() : nodes_(1)
        {
        }

private:
        void optimizeNode(Node* node)
        {
            for(auto& kv : node->children)
            {
                Node* child = &nodes_[kv.second];
                optimizeNode(child);
            }
        }

        void optimize()
        {
            optimizeNode(head());
        }

public:
        void validate()
        {
            if (!head()->IsSimpleNode())
                throw std::runtime_error("Internal error: Trie header should be simple!");
            optimize();
        }

        std::pair<unsigned, routing_params> find(const request& req, const Node* node = nullptr, unsigned pos = 0, routing_params* params = nullptr) const
        {
            routing_params empty;
            if (params == nullptr)
                params = &empty;

            unsigned found{};
            routing_params match_params;

            if (node == nullptr)
                node = head();
            if (pos == req.url.size())
                return {node->rule_index, *params};

            auto update_found = [&found, &match_params](std::pair<unsigned, routing_params>& ret)
            {
                if (ret.first && (!found || found > ret.first))
                {
                    found = ret.first;
                    match_params = ret.second;
                }
            };

            if (node->param_childrens[(int)ParamType::INT])
            {
                char c = req.url[pos];
                if ((c >= '0' && c <= '9') || c == '+' || c == '-')
                {
                    char* eptr;
                    errno = 0;
                    long long int value = strtoll(req.url.data()+pos, &eptr, 10);
                    if (errno != ERANGE && eptr != req.url.data()+pos)
                    {
                        params->int_params.push_back(value);
                        auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::INT]], eptr - req.url.data(), params);
                        update_found(ret);
                        params->int_params.pop_back();
                    }
                }
            }

            for(auto& kv : node->children)
            {
                const std::string& fragment = kv.first;
                const Node* child = &nodes_[kv.second];

                if (req.url.compare(pos, fragment.size(), fragment) == 0)
                {
                    auto ret = find(req, child, pos + fragment.size(), params);
                    update_found(ret);
                }
            }

            return {found, std::move(match_params)};
        }

        void add(const std::string& url, unsigned rule_index)
        {
            unsigned idx{0};

            for(unsigned i = 0; i < url.size(); i ++)
            {
                char c = url[i];
                if (c == '<')
                {
                    bool found = false;

                    static struct ParamTraits
                    {
                        ParamType type;
                        std::string name;
                    } paramTraits[] =
                    {
                        { ParamType::INT, "<int>" },
                        { ParamType::UINT, "<uint>" },
                        { ParamType::DOUBLE, "<float>" },
                        { ParamType::DOUBLE, "<double>" },
                        { ParamType::STRING, "<str>" },
                        { ParamType::STRING, "<string>" },
                        { ParamType::PATH, "<path>" },
                    };

                    for(auto it = std::begin(paramTraits); it != std::end(paramTraits); ++it)
                    {
                        if (url.compare(i, it->name.size(), it->name) == 0)
                        {
                            idx = nodes_[idx].param_childrens[(int)it->type] = new_node();
                            i += it->name.size();
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {
                        throw std::runtime_error("Invalid parameter type: " + url + " (" + boost::lexical_cast<std::string>(i) + ")");
                    }
                    i --;
                }
                else
                {
                    std::string piece(&c, 1);
                    if (!nodes_[idx].children.count(piece))
                    {
                        nodes_[idx].children.emplace(piece, new_node());
                    }
                    idx = nodes_[idx].children[piece];
                }
            }
            if (nodes_[idx].rule_index)
                throw std::runtime_error("handler already exists for " + url);
            nodes_[idx].rule_index = rule_index;
        }
    private:
        const Node* head() const
        {
            return &nodes_.front();
        }

        Node* head()
        {
            return &nodes_.front();
        }

        unsigned new_node()
        {
            nodes_.resize(nodes_.size()+1);
            return nodes_.size() - 1;
        }

        std::vector<Node> nodes_;
    };

    class Router
    {
    public:
        Router() : rules_(1) {}
        template <uint64_t N>
        typename black_magic::arguments<N>::type::template rebind<TaggedRule>& new_rule_tagged(const std::string& rule)
        {
            using RuleT = typename black_magic::arguments<N>::type::template rebind<TaggedRule>;
            auto ruleObject = new RuleT(rule);
            rules_.emplace_back(ruleObject);
            trie_.add(rule, rules_.size() - 1);
            return *ruleObject;
        }

        Rule& new_rule(const std::string& rule)
        {
            Rule* r(new Rule(rule));
            rules_.emplace_back(r);
            trie_.add(rule, rules_.size() - 1);
            return *r;
        }

        void validate()
        {
            trie_.validate();
            for(auto& rule:rules_)
            {
                if (rule)
                    rule->validate();
            }
        }

        response handle(const request& req)
        {
            auto found = trie_.find(req);
            unsigned rule_index = found.first;

            if (!rule_index)
                return response(404);

            if (rule_index >= rules_.size())
                throw std::runtime_error("Trie internal structure corrupted!");

            return rules_[rule_index]->handle(req, found.second);
        }
    private:
        std::vector<std::unique_ptr<BaseRule>> rules_;
        Trie trie_;
    };
}
