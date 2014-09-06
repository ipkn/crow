#pragma once
#include "http_request.h"
#include "http_response.h"

namespace crow
{
    // Any middleware requires following 3 members:

    // struct context;
    //      storing data for the middleware; can be read from another middleware or handlers

    // template <typename AllContext>
    // void before_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
    //      called before handling the request.
    //      if res.end() is called, the operation is halted. 
    //      (still call after_handle of this middleware)

    // template <typename AllContext>
    // void after_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
    //      called after handling the request.

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
