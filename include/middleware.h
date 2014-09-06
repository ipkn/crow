#pragma once
#include "http_request.h"
#include "http_response.h"

namespace crow
{
    class CookieParser
    {
        struct context
        {
            std::unordered_map<std::string, std::string> jar;
        };

        template <typename AllContext>
        void before_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
        {
            // ctx == all_ctx.bind<CookieParser>()
            // ctx.jar[] = ;
        }

        template <typename AllContext>
        void after_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
        {
        }
    }

    /*
    App<CookieParser, AnotherJarMW> app;
    A B C
    A::context
        int aa;

    ctx1 : public A::context
    ctx2 : public ctx1, public B::context
    ctx3 : public ctx2, public C::context

    C depends on A

    C::handle
        context.aaa

    App::context : private CookieParser::contetx, ... 
    {
        jar

    }

    SimpleApp
    */
}
