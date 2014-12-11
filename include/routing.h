#pragma once

#include <cstdint>
#include <utility>
#include <tuple>
#include <unordered_map>
#include <memory>
#include <boost/lexical_cast.hpp>
#include <vector>

#include "common.h"
#include "http_response.h"
#include "http_request.h"
#include "utility.h"
#include "logging.h"

namespace crow
{
    class BaseRule
    {
    public:
        virtual ~BaseRule()
        {
        }
        
        virtual void validate() = 0;

        virtual void handle(const request&, response&, const routing_params&) = 0;

        uint32_t methods()
        {
            return methods_;
        }

    protected:
        uint32_t methods_{1<<(int)HTTPMethod::GET};
    };

    template <typename ... Args>
    class TaggedRule : public BaseRule
    {
    private:
        template <typename H1, typename H2, typename H3>
        struct call_params
        {
            H1& handler;
            H2& handler_with_req;
            H3& handler_with_req_res;
            const routing_params& params;
            const request& req;
            response& res;
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename S1, typename S2> 
        struct call
        {
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
        struct call<F, NInt, NUint, NDouble, NString, black_magic::S<int64_t, Args1...>, black_magic::S<Args2...>>
        {
            void operator()(F cparams)
            {
                using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<int64_t, NInt>>;
                call<F, NInt+1, NUint, NDouble, NString,
                    black_magic::S<Args1...>, pushed>()(cparams);
            }
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
        struct call<F, NInt, NUint, NDouble, NString, black_magic::S<uint64_t, Args1...>, black_magic::S<Args2...>>
        {
            void operator()(F cparams)
            {
                using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<uint64_t, NUint>>;
                call<F, NInt, NUint+1, NDouble, NString,
                    black_magic::S<Args1...>, pushed>()(cparams);
            }
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
        struct call<F, NInt, NUint, NDouble, NString, black_magic::S<double, Args1...>, black_magic::S<Args2...>>
        {
            void operator()(F cparams)
            {
                using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<double, NDouble>>;
                call<F, NInt, NUint, NDouble+1, NString,
                    black_magic::S<Args1...>, pushed>()(cparams);
            }
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
        struct call<F, NInt, NUint, NDouble, NString, black_magic::S<std::string, Args1...>, black_magic::S<Args2...>>
        {
            void operator()(F cparams)
            {
                using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<std::string, NString>>;
                call<F, NInt, NUint, NDouble, NString+1,
                    black_magic::S<Args1...>, pushed>()(cparams);
            }
        };

        template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1> 
        struct call<F, NInt, NUint, NDouble, NString, black_magic::S<>, black_magic::S<Args1...>>
        {
            void operator()(F cparams)
            {
                if (cparams.handler) 
                {
                    cparams.res = cparams.handler(
                        cparams.params.template get<typename Args1::type>(Args1::pos)... 
                    );
                    cparams.res.end();
                    return;
                }
                if (cparams.handler_with_req)
                {
                    cparams.res = cparams.handler_with_req(
                        cparams.req,
                        cparams.params.template get<typename Args1::type>(Args1::pos)... 
                    );
                    cparams.res.end();
                    return;
                }
                if (cparams.handler_with_req_res)
                {
                    cparams.handler_with_req_res(
                        cparams.req,
                        cparams.res,
                        cparams.params.template get<typename Args1::type>(Args1::pos)... 
                    );
                    return;
                }
                CROW_LOG_DEBUG << "ERROR cannot find handler";

                // we already found matched url; this is server error
                cparams.res = response(500);
            }
        };
    public:
        using self_t = TaggedRule<Args...>;
        TaggedRule(std::string rule)
            : rule_(std::move(rule))
        {
        }
        
        self_t& name(std::string name) noexcept
        {
            name_ = std::move(name);
            return *this;
        }

        self_t& methods(HTTPMethod method)
        {
            methods_ = 1<<(int)method;
            return *this;
        }

        template <typename ... MethodArgs>
        self_t& methods(HTTPMethod method, MethodArgs ... args_method)
        {
            methods(args_method...);
            methods_ |= 1<<(int)method;
            return *this;
        }

        void validate()
        {
            if (!handler_ && !handler_with_req_ && !handler_with_req_res_)
            {
                throw std::runtime_error(name_ + (!name_.empty() ? ": " : "") + "no handler for url " + rule_);
            }
        }

        template <typename Func>
        typename std::enable_if<black_magic::CallHelper<Func, black_magic::S<Args...>>::value, void>::type
        operator()(Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<Args...>>::value ||
                black_magic::CallHelper<Func, black_magic::S<crow::request, Args...>>::value , 
                "Handler type is mismatched with URL paramters");
            static_assert(!std::is_same<void, decltype(f(std::declval<Args>()...))>::value, 
                "Handler function cannot have void return type; valid return types: string, int, crow::resposne, crow::json::wvalue");

                handler_ = [f = std::move(f)](Args ... args){
                    return response(f(args...));
                };
                handler_with_req_ = nullptr;
                handler_with_req_res_ = nullptr;
        }

        template <typename Func>
        typename std::enable_if<
            !black_magic::CallHelper<Func, black_magic::S<Args...>>::value &&
            black_magic::CallHelper<Func, black_magic::S<crow::request, Args...>>::value, 
            void>::type
        operator()(Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<Args...>>::value ||
                black_magic::CallHelper<Func, black_magic::S<crow::request, Args...>>::value, 
                "Handler type is mismatched with URL paramters");
            static_assert(!std::is_same<void, decltype(f(std::declval<crow::request>(), std::declval<Args>()...))>::value, 
                "Handler function cannot have void return type; valid return types: string, int, crow::resposne, crow::json::wvalue");

                handler_with_req_ = [f = std::move(f)](const crow::request& req, Args ... args){
                    return response(f(req, args...));
                };
                handler_ = nullptr;
                handler_with_req_res_ = nullptr;
        }

        template <typename Func>
        typename std::enable_if<
            !black_magic::CallHelper<Func, black_magic::S<Args...>>::value &&
            !black_magic::CallHelper<Func, black_magic::S<crow::request, Args...>>::value, 
            void>::type
        operator()(Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<Args...>>::value ||
                black_magic::CallHelper<Func, black_magic::S<crow::request, Args...>>::value ||
                black_magic::CallHelper<Func, black_magic::S<crow::request, crow::response&, Args...>>::value
                , 
                "Handler type is mismatched with URL paramters");
            static_assert(std::is_same<void, decltype(f(std::declval<crow::request>(), std::declval<crow::response&>(), std::declval<Args>()...))>::value, 
                "Handler function with response argument should have void return type");

                handler_with_req_res_ = std::move(f);
                //[f = std::move(f)](const crow::request& req, crow::response& res, Args ... args){
                //    f(req, response, args...);
                //};
                handler_ = nullptr;
                handler_with_req_ = nullptr;
        }

        template <typename Func>
        void operator()(std::string name, Func&& f)
        {
            name_ = std::move(name);
            (*this).template operator()<Func>(std::forward(f));
        }

        void handle(const request& req, response& res, const routing_params& params) override
        {
            call<
                call_params<
                    decltype(handler_), 
                    decltype(handler_with_req_),
                    decltype(handler_with_req_res_)>, 
                0, 0, 0, 0, 
                black_magic::S<Args...>, 
                black_magic::S<>
            >()(
                call_params<
                    decltype(handler_), 
                    decltype(handler_with_req_),
                    decltype(handler_with_req_res_)>
                {handler_, handler_with_req_, handler_with_req_res_, params, req, res}
            );
        }

    private:
        std::function<response(Args...)> handler_;
        std::function<response(const crow::request&, Args...)> handler_with_req_;
        std::function<void(const crow::request&, crow::response&, Args...)> handler_with_req_res_;

        std::string rule_;
        std::string name_;

        template <typename T, int Pos>
        struct call_pair
        {
            using type = T;
            static const int pos = Pos;
        };

        friend class Router;
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

        std::pair<unsigned, routing_params> find(const std::string& req_url, const Node* node = nullptr, unsigned pos = 0, routing_params* params = nullptr) const
        {
            routing_params empty;
            if (params == nullptr)
                params = &empty;

            unsigned found{};
            routing_params match_params;

            if (node == nullptr)
                node = head();
            if (pos == req_url.size())
                return {node->rule_index, *params};

            auto update_found = [&found, &match_params](std::pair<unsigned, routing_params>& ret)
            {
                if (ret.first && (!found || found > ret.first))
                {
                    found = ret.first;
                    match_params = std::move(ret.second);
                }
            };

            if (node->param_childrens[(int)ParamType::INT])
            {
                char c = req_url[pos];
                if ((c >= '0' && c <= '9') || c == '+' || c == '-')
                {
                    char* eptr;
                    errno = 0;
                    long long int value = strtoll(req_url.data()+pos, &eptr, 10);
                    if (errno != ERANGE && eptr != req_url.data()+pos)
                    {
                        params->int_params.push_back(value);
                        auto ret = find(req_url, &nodes_[node->param_childrens[(int)ParamType::INT]], eptr - req_url.data(), params);
                        update_found(ret);
                        params->int_params.pop_back();
                    }
                }
            }

            if (node->param_childrens[(int)ParamType::UINT])
            {
                char c = req_url[pos];
                if ((c >= '0' && c <= '9') || c == '+')
                {
                    char* eptr;
                    errno = 0;
                    unsigned long long int value = strtoull(req_url.data()+pos, &eptr, 10);
                    if (errno != ERANGE && eptr != req_url.data()+pos)
                    {
                        params->uint_params.push_back(value);
                        auto ret = find(req_url, &nodes_[node->param_childrens[(int)ParamType::UINT]], eptr - req_url.data(), params);
                        update_found(ret);
                        params->uint_params.pop_back();
                    }
                }
            }

            if (node->param_childrens[(int)ParamType::DOUBLE])
            {
                char c = req_url[pos];
                if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.')
                {
                    char* eptr;
                    errno = 0;
                    double value = strtod(req_url.data()+pos, &eptr);
                    if (errno != ERANGE && eptr != req_url.data()+pos)
                    {
                        params->double_params.push_back(value);
                        auto ret = find(req_url, &nodes_[node->param_childrens[(int)ParamType::DOUBLE]], eptr - req_url.data(), params);
                        update_found(ret);
                        params->double_params.pop_back();
                    }
                }
            }

            if (node->param_childrens[(int)ParamType::STRING])
            {
                size_t epos = pos;
                for(; epos < req_url.size(); epos ++)
                {
                    if (req_url[epos] == '/')
                        break;
                }

                if (epos != pos)
                {
                    params->string_params.push_back(req_url.substr(pos, epos-pos));
                    auto ret = find(req_url, &nodes_[node->param_childrens[(int)ParamType::STRING]], epos, params);
                    update_found(ret);
                    params->string_params.pop_back();
                }
            }

            if (node->param_childrens[(int)ParamType::PATH])
            {
                size_t epos = req_url.size();

                if (epos != pos)
                {
                    params->string_params.push_back(req_url.substr(pos, epos-pos));
                    auto ret = find(req_url, &nodes_[node->param_childrens[(int)ParamType::PATH]], epos, params);
                    update_found(ret);
                    params->string_params.pop_back();
                }
            }

            for(auto& kv : node->children)
            {
                const std::string& fragment = kv.first;
                const Node* child = &nodes_[kv.second];

                if (req_url.compare(pos, fragment.size(), fragment) == 0)
                {
                    auto ret = find(req_url, child, pos + fragment.size(), params);
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
                    CROW_LOG_DEBUG << std::string(2*level, ' ') /*<< "("<<n->param_childrens[i]<<") "*/;
                    switch((ParamType)i)
                    {
                        case ParamType::INT:
                            CROW_LOG_DEBUG << "<int>";
                            break;
                        case ParamType::UINT:
                            CROW_LOG_DEBUG << "<uint>";
                            break;
                        case ParamType::DOUBLE:
                            CROW_LOG_DEBUG << "<float>";
                            break;
                        case ParamType::STRING:
                            CROW_LOG_DEBUG << "<str>";
                            break;
                        case ParamType::PATH:
                            CROW_LOG_DEBUG << "<path>";
                            break;
                        default:
                            CROW_LOG_DEBUG << "<ERROR>";
                            break;
                    }

                    debug_node_print(&nodes_[n->param_childrens[i]], level+1);
                }
            }
            for(auto& kv : n->children)
            {
                CROW_LOG_DEBUG << std::string(2*level, ' ') /*<< "(" << kv.second << ") "*/ << kv.first;
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

        void handle(const request& req, response& res)
        {
            auto found = trie_.find(req.url);

            unsigned rule_index = found.first;

            if (!rule_index)
            {
                CROW_LOG_DEBUG << "Cannot match rules " << req.url;
                res = response(404);
                res.end();
                return;
            }

            if (rule_index >= rules_.size())
                throw std::runtime_error("Trie internal structure corrupted!");

            if ((rules_[rule_index]->methods() & (1<<(uint32_t)req.method)) == 0)
            {
                CROW_LOG_DEBUG << "Rule found but method mismatch: " << req.url << " with " << method_name(req.method) << "(" << (uint32_t)req.method << ") / " << rules_[rule_index]->methods();
                res = response(404);
                res.end();
                return;
            }

            CROW_LOG_DEBUG << "Matched rule '" << ((TaggedRule<>*)rules_[rule_index].get())->rule_ << "' " << (uint32_t)req.method << " / " << rules_[rule_index]->methods();

            // any uncaught exceptions become 500s
            try
            {
                rules_[rule_index]->handle(req, res, found.second);
            }
            catch(std::exception& e)
            {
                CROW_LOG_ERROR << "An uncaught exception occurred: " << e.what();
                res = response(500);
                res.end();
                return;   
            }
            catch(...)
            {
                CROW_LOG_ERROR << "An uncaught exception occurred. The type was unknown so no information was available.";
                res = response(500);
                res.end();
                return;   
            }
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
