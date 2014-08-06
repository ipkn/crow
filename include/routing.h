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

    protected:

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
#ifdef CROW_ENABLE_LOGGING
                std::cerr << "ERROR cannot find handler" << std::endl;
#endif
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
        }

        template <typename ... MethodArgs>
        self_t& methods(HTTPMethod method, MethodArgs ... args_method)
        {
            methods(args_method...);
            methods_ |= 1<<(int)method;
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
        uint32_t methods_{1<<(int)HTTPMethod::GET};

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

            bool IsSimpleNode() const;
        };

        Trie();

        void validate();

        void add(const std::string& url, unsigned rule_index);

        std::pair<unsigned, routing_params> find(
            const request& req, 
            const Node* node = nullptr, 
            unsigned pos = 0, 
            routing_params* params = nullptr) const;

        void debug_print();

    private:
        void debug_node_print(Node* n, int level);

        void optimizeNode(Node* node);
        void optimize();

        const Node* head() const;
        Node* head();

        unsigned new_node();

    private:
        std::vector<Node> nodes_;
    };

    class Router
    {
    public:
        Router();
        template <uint64_t N>
        typename black_magic::arguments<N>::type::template rebind<TaggedRule>& new_rule_tagged(const std::string& rule)
        {
            using RuleT = typename black_magic::arguments<N>::type::template rebind<TaggedRule>;
            auto ruleObject = new RuleT(rule);
            rules_.emplace_back(ruleObject);
            trie_.add(rule, rules_.size() - 1);
            return *ruleObject;
        }

        void validate();

        void handle(const request& req, response& res);
        
        void debug_print();

    private:
        std::vector<std::unique_ptr<BaseRule>> rules_;
        Trie trie_;
    };
}
