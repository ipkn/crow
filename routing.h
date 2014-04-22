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
        BaseRule(std::string rule) noexcept
            : rule_(std::move(rule))
        {
        }

        virtual ~BaseRule()
        {
        }
        
        BaseRule& name(std::string name) noexcept
        {
            name_ = std::move(name);
            return *this;
        }

        virtual void validate() = 0;

        virtual response handle(const request&, const routing_params&) = 0;

    protected:
        std::string rule_;
        std::string name_;

        friend class Router;
    };

    template <typename ... Args>
    class TaggedRule : public BaseRule
    {
    private:
        template <typename H1, typename H2>
        struct call_params
        {
            H1& handler;
            H2& handler_with_req;
            const routing_params& params;
            const request& req;
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename S1, typename S2> 
        struct call
        {
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
        struct call<F, NInt, NUint, NDouble, NString, black_magic::S<int64_t, Args1...>, black_magic::S<Args2...>>
        {
            response operator()(F cparams)
            {
                using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<int64_t, NInt>>;
                return call<F, NInt+1, NUint, NDouble, NString,
                       black_magic::S<Args1...>, pushed>()(cparams);
            }
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
        struct call<F, NInt, NUint, NDouble, NString, black_magic::S<uint64_t, Args1...>, black_magic::S<Args2...>>
        {
            response operator()(F cparams)
            {
                using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<uint64_t, NUint>>;
                return call<F, NInt, NUint+1, NDouble, NString,
                       black_magic::S<Args1...>, pushed>()(cparams);
            }
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
        struct call<F, NInt, NUint, NDouble, NString, black_magic::S<double, Args1...>, black_magic::S<Args2...>>
        {
            response operator()(F cparams)
            {
                using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<double, NDouble>>;
                return call<F, NInt, NUint, NDouble+1, NString,
                       black_magic::S<Args1...>, pushed>()(cparams);
            }
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
        struct call<F, NInt, NUint, NDouble, NString, black_magic::S<std::string, Args1...>, black_magic::S<Args2...>>
        {
            response operator()(F cparams)
            {
                using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<std::string, NString>>;
                return call<F, NInt, NUint, NDouble, NString+1,
                       black_magic::S<Args1...>, pushed>()(cparams);
            }
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1> 
        struct call<F, NInt, NUint, NDouble, NString, black_magic::S<>, black_magic::S<Args1...>>
        {
            response operator()(F cparams)
            {
                if (cparams.handler) 
                    return cparams.handler(
                        cparams.params.template get<typename Args1::type>(Args1::pos)... 
                    );
                if (cparams.handler_with_req)
                    return cparams.handler_with_req(
                        cparams.req,
                        cparams.params.template get<typename Args1::type>(Args1::pos)... 
                    );
#ifdef FLASK_ENABLE_LOGGING
                std::cerr << "ERROR cannot find handler" << std::endl;
#endif
                return response(500);
            }
        };
    public:
        TaggedRule(std::string rule)
            : BaseRule(std::move(rule))
        {
        }
        
        TaggedRule<Args...>& name(std::string name) noexcept
        {
            name_ = std::move(name);
            return *this;
        }

        void validate()
        {
            if (!handler_ && !handler_with_req_)
            {
                throw std::runtime_error(name_ + (!name_.empty() ? ": " : "") + "no handler for url " + rule_);
            }
        }

        template <typename Func>
        typename std::enable_if<black_magic::CallHelper<Func, black_magic::S<Args...>>::value, void>::type
        operator()(Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<Args...>>::value ||
            black_magic::CallHelper<Func, black_magic::S<flask::request, Args...>>::value
            , 
                "Handler type is mismatched with URL paramters");
            static_assert(!std::is_same<void, decltype(f(std::declval<Args>()...))>::value, 
                "Handler function cannot have void return type; valid return types: string, int, flask::resposne, flask::json::wvalue");

                handler_ = [f = std::move(f)](Args ... args){
                    return response(f(args...));
                };
                handler_with_req_ = nullptr;
        }

        template <typename Func>
        typename std::enable_if<!black_magic::CallHelper<Func, black_magic::S<Args...>>::value, void>::type
        operator()(Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<Args...>>::value ||
            black_magic::CallHelper<Func, black_magic::S<flask::request, Args...>>::value
            , 
                "Handler type is mismatched with URL paramters");
            static_assert(!std::is_same<void, decltype(f(std::declval<flask::request>(), std::declval<Args>()...))>::value, 
                "Handler function cannot have void return type; valid return types: string, int, flask::resposne, flask::json::wvalue");

                handler_with_req_ = [f = std::move(f)](const flask::request& request, Args ... args){
                    return response(f(request, args...));
                };
                handler_ = nullptr;
        }

        template <typename Func>
        void operator()(std::string name, Func&& f)
        {
            name_ = std::move(name);
            (*this).operator()<Func>(std::forward(f));
        }

        response handle(const request& req, const routing_params& params)
        {
            //return handler_();
            return 
                call<
                    call_params<
                        decltype(handler_), 
                        decltype(handler_with_req_)>, 
                    0, 0, 0, 0, 
                    black_magic::S<Args...>, 
                    black_magic::S<>
                >()(
                    call_params<
                        decltype(handler_), 
                        decltype(handler_with_req_)>
                    {handler_, handler_with_req_, params, req}
                );
            //return response(500);
        }

    private:
        std::function<response(Args...)> handler_;
        std::function<response(flask::request, Args...)> handler_with_req_;

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
            unsigned rule_index{};
            std::array<unsigned, (int)ParamType::MAX> param_childrens{};
            std::unordered_map<std::string, unsigned> children;

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
            for(auto x : node->param_childrens)
            {
                if (!x)
                    continue;
                Node* child = &nodes_[x];
                optimizeNode(child);
            }
            if (node->children.empty())
                return;
            bool mergeWithChild = true;
            for(auto& kv : node->children)
            {
                Node* child = &nodes_[kv.second];
                if (!child->IsSimpleNode())
                {
                    mergeWithChild = false;
                    break;
                }
            }
            if (mergeWithChild)
            {
                decltype(node->children) merged;
                for(auto& kv : node->children)
                {
                    Node* child = &nodes_[kv.second];
                    for(auto& child_kv : child->children)
                    {
                        merged[kv.first + child_kv.first] = child_kv.second;
                    }
                }
                node->children = std::move(merged);
                optimizeNode(node);
            }
            else
            {
                for(auto& kv : node->children)
                {
                    Node* child = &nodes_[kv.second];
                    optimizeNode(child);
                }
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

            if (node->param_childrens[(int)ParamType::UINT])
            {
                char c = req.url[pos];
                if ((c >= '0' && c <= '9') || c == '+')
                {
                    char* eptr;
                    errno = 0;
                    unsigned long long int value = strtoull(req.url.data()+pos, &eptr, 10);
                    if (errno != ERANGE && eptr != req.url.data()+pos)
                    {
                        params->uint_params.push_back(value);
                        auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::UINT]], eptr - req.url.data(), params);
                        update_found(ret);
                        params->uint_params.pop_back();
                    }
                }
            }

            if (node->param_childrens[(int)ParamType::DOUBLE])
            {
                char c = req.url[pos];
                if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.')
                {
                    char* eptr;
                    errno = 0;
                    double value = strtod(req.url.data()+pos, &eptr);
                    if (errno != ERANGE && eptr != req.url.data()+pos)
                    {
                        params->double_params.push_back(value);
                        auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::DOUBLE]], eptr - req.url.data(), params);
                        update_found(ret);
                        params->double_params.pop_back();
                    }
                }
            }

            if (node->param_childrens[(int)ParamType::STRING])
            {
                size_t epos = pos;
                for(; epos < req.url.size(); epos ++)
                {
                    if (req.url[epos] == '/')
                        break;
                }

                if (epos != pos)
                {
                    params->string_params.push_back(req.url.substr(pos, epos-pos));
                    auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::STRING]], epos, params);
                    update_found(ret);
                    params->string_params.pop_back();
                }
            }

            if (node->param_childrens[(int)ParamType::PATH])
            {
                size_t epos = req.url.size();

                if (epos != pos)
                {
                    params->string_params.push_back(req.url.substr(pos, epos-pos));
                    auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::PATH]], epos, params);
                    update_found(ret);
                    params->string_params.pop_back();
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

            return {found, match_params};
        }

        void add(const std::string& url, unsigned rule_index)
        {
            unsigned idx{0};

            for(unsigned i = 0; i < url.size(); i ++)
            {
                char c = url[i];
                if (c == '<')
                {
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

                    for(auto& x:paramTraits)
                    {
                        if (url.compare(i, x.name.size(), x.name) == 0)
                        {
                            if (!nodes_[idx].param_childrens[(int)x.type])
                            {
                                auto new_node_idx = new_node();
                                nodes_[idx].param_childrens[(int)x.type] = new_node_idx;
                            }
                            idx = nodes_[idx].param_childrens[(int)x.type];
                            i += x.name.size();
                            break;
                        }
                    }

                    i --;
                }
                else
                {
                    std::string piece(&c, 1);
                    if (!nodes_[idx].children.count(piece))
                    {
                        auto new_node_idx = new_node();
                        nodes_[idx].children.emplace(piece, new_node_idx);
                    }
                    idx = nodes_[idx].children[piece];
                }
            }
            if (nodes_[idx].rule_index)
                throw std::runtime_error("handler already exists for " + url);
            nodes_[idx].rule_index = rule_index;
        }
    private:
        void debug_node_print(Node* n, int level)
        {
            for(int i = 0; i < (int)ParamType::MAX; i ++)
            {
                if (n->param_childrens[i])
                {
                    std::cerr << std::string(2*level, ' ') /*<< "("<<n->param_childrens[i]<<") "*/;
                    switch((ParamType)i)
                    {
                        case ParamType::INT:
                            std::cerr << "<int>";
                            break;
                        case ParamType::UINT:
                            std::cerr << "<uint>";
                            break;
                        case ParamType::DOUBLE:
                            std::cerr << "<float>";
                            break;
                        case ParamType::STRING:
                            std::cerr << "<str>";
                            break;
                        case ParamType::PATH:
                            std::cerr << "<path>";
                            break;
                        default:
                            std::cerr << "<ERROR>";
                            break;
                    }
                    std::cerr << std::endl;
                    debug_node_print(&nodes_[n->param_childrens[i]], level+1);
                }
            }
            for(auto& kv : n->children)
            {
                std::cerr << std::string(2*level, ' ') /*<< "(" << kv.second << ") "*/ << kv.first << std::endl;
                debug_node_print(&nodes_[kv.second], level+1);
            }
        }

    public:
        void debug_print()
        {
            debug_node_print(head(), 0);
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
#ifdef FLASK_ENABLE_LOGGING
            std::cerr << req.url << ' ' << rules_[rule_index]->rule_ << std::endl;
#endif
            return rules_[rule_index]->handle(req, found.second);
        }

        void debug_print()
        {
            trie_.debug_print();
        }

    private:
        std::vector<std::unique_ptr<BaseRule>> rules_;
        Trie trie_;
    };
}
