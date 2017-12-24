#pragma once

#include <cstdint>
#include <utility>
#include <tuple>
#include <unordered_map>
#include <memory>
#include <boost/lexical_cast.hpp>
#include <vector>

#include "crow/common.h"
#include "crow/http_response.h"
#include "crow/http_request.h"
#include "crow/utility.h"
#include "crow/logging.h"
#include "crow/websocket.h"

namespace crow
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
        
        virtual void validate() = 0;
        std::unique_ptr<BaseRule> upgrade()
        {
            if (rule_to_upgrade_)
                return std::move(rule_to_upgrade_);
            return {};
        }

        virtual void handle(const request&, response&, const routing_params&) = 0;
        virtual void handle_upgrade(const request&, response& res, SocketAdaptor&&) 
        {
            res = response(404);
            res.end();
        }
#ifdef CROW_ENABLE_SSL
        virtual void handle_upgrade(const request&, response& res, SSLAdaptor&&) 
        {
            res = response(404);
            res.end();
        }
#endif

        uint32_t get_methods()
        {
            return methods_;
        }

        template <typename F>
        void foreach_method(F f)
        {
            for(uint32_t method = 0, method_bit = 1; method < (uint32_t)HTTPMethod::InternalMethodCount; method++, method_bit<<=1)
            {
                if (methods_ & method_bit)
                    f(method);
            }
        }

        const std::string& rule() { return rule_; }

    protected:
        uint32_t methods_{1<<(int)HTTPMethod::Get};

        std::string rule_;
        std::string name_;

        std::unique_ptr<BaseRule> rule_to_upgrade_;

        friend class Router;
        template <typename T>
        friend struct RuleParameterTraits;
    };


    namespace detail
    {
        namespace routing_handler_call_helper
        {
            template <typename T, int Pos>
            struct call_pair
            {
                using type = T;
                static const int pos = Pos;
            };

            template <typename H1>
            struct call_params
            {
                H1& handler;
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
                    cparams.handler(
                        cparams.req,
                        cparams.res,
                        cparams.params.template get<typename Args1::type>(Args1::pos)... 
                    );
                }
            };

            template <typename Func, typename ... ArgsWrapped>
            struct Wrapped
            {
                template <typename ... Args>
                void set_(Func f, typename std::enable_if<
                    !std::is_same<typename std::tuple_element<0, std::tuple<Args..., void>>::type, const request&>::value
                , int>::type = 0)
                {
                    handler_ = (
#ifdef CROW_CAN_USE_CPP14
                        [f = std::move(f)]
#else
                        [f]
#endif
                        (const request&, response& res, Args... args){
                            res = response(f(args...));
                            res.end();
                        });
                }

                template <typename Req, typename ... Args>
                struct req_handler_wrapper
                {
                    req_handler_wrapper(Func f)
                        : f(std::move(f))
                    {
                    }

                    void operator()(const request& req, response& res, Args... args)
                    {
                        res = response(f(req, args...));
                        res.end();
                    }

                    Func f;
                };

                template <typename ... Args>
                void set_(Func f, typename std::enable_if<
                        std::is_same<typename std::tuple_element<0, std::tuple<Args..., void>>::type, const request&>::value &&
                        !std::is_same<typename std::tuple_element<1, std::tuple<Args..., void, void>>::type, response&>::value
                        , int>::type = 0)
                {
                    handler_ = req_handler_wrapper<Args...>(std::move(f));
                    /*handler_ = (
                        [f = std::move(f)]
                        (const request& req, response& res, Args... args){
                             res = response(f(req, args...));
                             res.end();
                        });*/
                }

                template <typename ... Args>
                void set_(Func f, typename std::enable_if<
                        std::is_same<typename std::tuple_element<0, std::tuple<Args..., void>>::type, const request&>::value &&
                        std::is_same<typename std::tuple_element<1, std::tuple<Args..., void, void>>::type, response&>::value
                        , int>::type = 0)
                {
                    handler_ = std::move(f);
                }

                template <typename ... Args>
                struct handler_type_helper
                {
                    using type = std::function<void(const crow::request&, crow::response&, Args...)>;
                    using args_type = black_magic::S<typename black_magic::promote_t<Args>...>; 
                };

                template <typename ... Args>
                struct handler_type_helper<const request&, Args...>
                {
                    using type = std::function<void(const crow::request&, crow::response&, Args...)>;
                    using args_type = black_magic::S<typename black_magic::promote_t<Args>...>; 
                };

                template <typename ... Args>
                struct handler_type_helper<const request&, response&, Args...>
                {
                    using type = std::function<void(const crow::request&, crow::response&, Args...)>;
                    using args_type = black_magic::S<typename black_magic::promote_t<Args>...>; 
                };

                typename handler_type_helper<ArgsWrapped...>::type handler_;

                void operator()(const request& req, response& res, const routing_params& params)
                {
                    detail::routing_handler_call_helper::call<
                        detail::routing_handler_call_helper::call_params<
                            decltype(handler_)>,
                        0, 0, 0, 0, 
                        typename handler_type_helper<ArgsWrapped...>::args_type,
                        black_magic::S<>
                    >()(
                        detail::routing_handler_call_helper::call_params<
                            decltype(handler_)>
                        {handler_, params, req, res}
                   );
                }
            };

        }
    }

    class WebSocketRule : public BaseRule
    {
        using self_t = WebSocketRule;
    public:
        WebSocketRule(std::string rule)
            : BaseRule(std::move(rule))
        {
        }

        void validate() override
        {
        }

        void handle(const request&, response& res, const routing_params&) override
        {
            res = response(404);
            res.end();
        }

        void handle_upgrade(const request& req, response&, SocketAdaptor&& adaptor) override 
        {
            new crow::websocket::Connection<SocketAdaptor>(req, std::move(adaptor), open_handler_, message_handler_, close_handler_, error_handler_, accept_handler_);
        }
#ifdef CROW_ENABLE_SSL
        void handle_upgrade(const request& req, response&, SSLAdaptor&& adaptor) override
        {
            new crow::websocket::Connection<SSLAdaptor>(req, std::move(adaptor), open_handler_, message_handler_, close_handler_, error_handler_, accept_handler_);
        }
#endif

        template <typename Func>
        self_t& onopen(Func f)
        {
            open_handler_ = f;
            return *this;
        }

        template <typename Func>
        self_t& onmessage(Func f)
        {
            message_handler_ = f;
            return *this;
        }

        template <typename Func>
        self_t& onclose(Func f)
        {
            close_handler_ = f;
            return *this;
        }

        template <typename Func>
        self_t& onerror(Func f)
        {
            error_handler_ = f;
            return *this;
        }

        template <typename Func>
        self_t& onaccept(Func f)
        {
            accept_handler_ = f;
            return *this;
        }

    protected:
        std::function<void(crow::websocket::connection&)> open_handler_;
        std::function<void(crow::websocket::connection&, const std::string&, bool)> message_handler_;
        std::function<void(crow::websocket::connection&, const std::string&)> close_handler_;
        std::function<void(crow::websocket::connection&)> error_handler_;
        std::function<bool(const crow::request&)> accept_handler_;
    };

    template <typename T>
    struct RuleParameterTraits
    {
        using self_t = T;
        WebSocketRule& websocket() 
        {
            auto p =new WebSocketRule(((self_t*)this)->rule_);
            ((self_t*)this)->rule_to_upgrade_.reset(p);
            return *p;
        }

        self_t& name(std::string name) noexcept
        {
            ((self_t*)this)->name_ = std::move(name);
            return (self_t&)*this;
        }

        self_t& methods(HTTPMethod method)
        {
            ((self_t*)this)->methods_ = 1 << (int)method;
            return (self_t&)*this;
        }

        template <typename ... MethodArgs>
        self_t& methods(HTTPMethod method, MethodArgs ... args_method)
        {
            methods(args_method...);
            ((self_t*)this)->methods_ |= 1 << (int)method;
            return (self_t&)*this;
        }

    };

    class DynamicRule : public BaseRule, public RuleParameterTraits<DynamicRule>
    {
    public:

        DynamicRule(std::string rule)
            : BaseRule(std::move(rule))
        {
        }

        void validate() override
        {
            if (!erased_handler_)
            {
                throw std::runtime_error(name_ + (!name_.empty() ? ": " : "") + "no handler for url " + rule_);
            }
        }

        void handle(const request& req, response& res, const routing_params& params) override
        {
            erased_handler_(req, res, params);
        }

        template <typename Func>
        void operator()(Func f)
        {
#ifdef CROW_MSVC_WORKAROUND
            using function_t = utility::function_traits<decltype(&Func::operator())>;
#else
            using function_t = utility::function_traits<Func>;
#endif
            erased_handler_ = wrap(std::move(f), black_magic::gen_seq<function_t::arity>());
        }

        // enable_if Arg1 == request && Arg2 == response
        // enable_if Arg1 == request && Arg2 != resposne
        // enable_if Arg1 != request
#ifdef CROW_MSVC_WORKAROUND
        template <typename Func, size_t ... Indices>
#else
        template <typename Func, unsigned ... Indices>
#endif
        std::function<void(const request&, response&, const routing_params&)> 
        wrap(Func f, black_magic::seq<Indices...>)
        {
#ifdef CROW_MSVC_WORKAROUND
            using function_t = utility::function_traits<decltype(&Func::operator())>;
#else
            using function_t = utility::function_traits<Func>;
#endif
            if (!black_magic::is_parameter_tag_compatible(
                black_magic::get_parameter_tag_runtime(rule_.c_str()), 
                black_magic::compute_parameter_tag_from_args_list<
                    typename function_t::template arg<Indices>...>::value))
            {
                throw std::runtime_error("route_dynamic: Handler type is mismatched with URL parameters: " + rule_);
            }
            auto ret = detail::routing_handler_call_helper::Wrapped<Func, typename function_t::template arg<Indices>...>();
            ret.template set_<
                typename function_t::template arg<Indices>...
            >(std::move(f));
            return ret;
        }

        template <typename Func>
        void operator()(std::string name, Func&& f)
        {
            name_ = std::move(name);
            (*this).template operator()<Func>(std::forward(f));
        }
    private:
        std::function<void(const request&, response&, const routing_params&)> erased_handler_;

    };

    template <typename ... Args>
    class TaggedRule : public BaseRule, public RuleParameterTraits<TaggedRule<Args...>>
    {
    public:
        using self_t = TaggedRule<Args...>;

        TaggedRule(std::string rule)
            : BaseRule(std::move(rule))
        {
        }

        void validate() override
        {
            if (!handler_)
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
                "Handler type is mismatched with URL parameters");
            static_assert(!std::is_same<void, decltype(f(std::declval<Args>()...))>::value, 
                "Handler function cannot have void return type; valid return types: string, int, crow::resposne, crow::json::wvalue");

            handler_ = (
#ifdef CROW_CAN_USE_CPP14
                [f = std::move(f)]
#else
                [f]
#endif
                (const request&, response& res, Args ... args){
                    res = response(f(args...));
                    res.end();
                });
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
                "Handler type is mismatched with URL parameters");
            static_assert(!std::is_same<void, decltype(f(std::declval<crow::request>(), std::declval<Args>()...))>::value, 
                "Handler function cannot have void return type; valid return types: string, int, crow::resposne, crow::json::wvalue");

            handler_ = (
#ifdef CROW_CAN_USE_CPP14
                [f = std::move(f)]
#else
                [f]
#endif
                (const crow::request& req, crow::response& res, Args ... args){
                    res = response(f(req, args...));
                    res.end();
                });
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
                "Handler type is mismatched with URL parameters");
            static_assert(std::is_same<void, decltype(f(std::declval<crow::request>(), std::declval<crow::response&>(), std::declval<Args>()...))>::value, 
                "Handler function with response argument should have void return type");

                handler_ = std::move(f);
        }

        template <typename Func>
        void operator()(std::string name, Func&& f)
        {
            name_ = std::move(name);
            (*this).template operator()<Func>(std::forward(f));
        }

        void handle(const request& req, response& res, const routing_params& params) override
        {
            detail::routing_handler_call_helper::call<
                detail::routing_handler_call_helper::call_params<
                    decltype(handler_)>, 
                0, 0, 0, 0, 
                black_magic::S<Args...>, 
                black_magic::S<>
            >()(
                detail::routing_handler_call_helper::call_params<
                    decltype(handler_)>
                {handler_, params, req, res}
            );
        }

    private:
        std::function<void(const crow::request&, crow::response&, Args...)> handler_;

    };

    const int RULE_SPECIAL_REDIRECT_SLASH = 1;

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
        Router()
        {
        }

        DynamicRule& new_rule_dynamic(const std::string& rule)
        {
            auto ruleObject = new DynamicRule(rule);
            all_rules_.emplace_back(ruleObject);

            return *ruleObject;
        }

        template <uint64_t N>
        typename black_magic::arguments<N>::type::template rebind<TaggedRule>& new_rule_tagged(const std::string& rule)
        {
            using RuleT = typename black_magic::arguments<N>::type::template rebind<TaggedRule>;

            auto ruleObject = new RuleT(rule);
            all_rules_.emplace_back(ruleObject);

            return *ruleObject;
        }

        void internal_add_rule_object(const std::string& rule, BaseRule* ruleObject)
        {
            bool has_trailing_slash = false;
            std::string rule_without_trailing_slash;
            if (rule.size() > 1 && rule.back() == '/')
            {
                has_trailing_slash = true;
                rule_without_trailing_slash = rule;
                rule_without_trailing_slash.pop_back();
            }

            ruleObject->foreach_method([&](int method)
                    {
                        per_methods_[method].rules.emplace_back(ruleObject);
                        per_methods_[method].trie.add(rule, per_methods_[method].rules.size() - 1);

                        // directory case: 
                        //   request to `/about' url matches `/about/' rule 
                        if (has_trailing_slash)
                        {
                            per_methods_[method].trie.add(rule_without_trailing_slash, RULE_SPECIAL_REDIRECT_SLASH);
                        }
                    });

        }

        void validate()
        {
            for(auto& rule:all_rules_)
            {
                if (rule)
                {
                    auto upgraded = rule->upgrade();
                    if (upgraded)
                        rule = std::move(upgraded);
                    rule->validate();
                    internal_add_rule_object(rule->rule(), rule.get());
                }
            }
            for(auto& per_method:per_methods_)
            {
                per_method.trie.validate();
            }
        }

        template <typename Adaptor> 
        void handle_upgrade(const request& req, response& res, Adaptor&& adaptor)
        {
            if (req.method >= HTTPMethod::InternalMethodCount)
                return;
            auto& per_method = per_methods_[(int)req.method];
            auto& trie = per_method.trie;
            auto& rules = per_method.rules;

            auto found = trie.find(req.url);
            unsigned rule_index = found.first;
            if (!rule_index)
            {
                CROW_LOG_DEBUG << "Cannot match rules " << req.url << ' ' << method_name(req.method);
                res = response(404);
                res.end();
                return;
            }

            if (rule_index >= rules.size())
                throw std::runtime_error("Trie internal structure corrupted!");

            if (rule_index == RULE_SPECIAL_REDIRECT_SLASH)
            {
                CROW_LOG_INFO << "Redirecting to a url with trailing slash: " << req.url;
                res = response(301);

                // TODO absolute url building
                if (req.get_header_value("Host").empty())
                {
                    res.add_header("Location", req.url + "/");
                }
                else
                {
                    res.add_header("Location", "http://" + req.get_header_value("Host") + req.url + "/");
                }
                res.end();
                return;
            }

            CROW_LOG_DEBUG << "Matched rule (upgrade) '" << rules[rule_index]->rule_ << "' " << (uint32_t)req.method << " / " << rules[rule_index]->get_methods();

            // any uncaught exceptions become 500s
            try
            {
                rules[rule_index]->handle_upgrade(req, res, std::move(adaptor));
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

        void handle(const request& req, response& res)
        {
            if (req.method >= HTTPMethod::InternalMethodCount)
                return;
            auto& per_method = per_methods_[(int)req.method];
            auto& trie = per_method.trie;
            auto& rules = per_method.rules;

            auto found = trie.find(req.url);

            unsigned rule_index = found.first;

            if (!rule_index)
            {
                CROW_LOG_DEBUG << "Cannot match rules " << req.url << ' ' << method_name(req.method);
                res = response(404);
                res.end();
                return;
            }

            if (rule_index >= rules.size())
                throw std::runtime_error("Trie internal structure corrupted!");

            if (rule_index == RULE_SPECIAL_REDIRECT_SLASH)
            {
                CROW_LOG_INFO << "Redirecting to a url with trailing slash: " << req.url;
                res = response(301);

                // TODO absolute url building
                if (req.get_header_value("Host").empty())
                {
                    res.add_header("Location", req.url + "/");
                }
                else
                {
                    res.add_header("Location", "http://" + req.get_header_value("Host") + req.url + "/");
                }
                res.end();
                return;
            }

            CROW_LOG_DEBUG << "Matched rule '" << rules[rule_index]->rule_ << "' " << (uint32_t)req.method << " / " << rules[rule_index]->get_methods();

            // any uncaught exceptions become 500s
            try
            {
                rules[rule_index]->handle(req, res, found.second);
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
            for(int i = 0; i < (int)HTTPMethod::InternalMethodCount; i ++)
            {
                CROW_LOG_DEBUG << method_name((HTTPMethod)i);
                per_methods_[i].trie.debug_print();
            }
        }

    private:
        struct PerMethod
        {
            std::vector<BaseRule*> rules;
            Trie trie;

            // rule index 0, 1 has special meaning; preallocate it to avoid duplication.
            PerMethod() : rules(2) {}
        };
        std::array<PerMethod, (int)HTTPMethod::InternalMethodCount> per_methods_;
        std::vector<std::unique_ptr<BaseRule>> all_rules_;
    };
}
