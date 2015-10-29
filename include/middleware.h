#pragma once
#include <boost/algorithm/string/trim.hpp>
#include <boost/tokenizer.hpp>
#include "http_request.h"
#include "http_response.h"

namespace crow
{
    // Any middleware requires following 3 members:

    // struct context;
    //      storing data for the middleware; can be read from another middleware or handlers

    // before_handle
    //      called before handling the request.
    //      if res.end() is called, the operation is halted. 
    //      (still call after_handle of this middleware)
    //      2 signatures:
    //      void before_handle(request& req, response& res, context& ctx)
    //          if you only need to access this middlewares context.
    //      template <typename AllContext>
    //      void before_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
    //          you can access another middlewares' context by calling `all_ctx.template get<MW>()'
    //          ctx == all_ctx.template get<CurrentMiddleware>()

    // after_handle
    //      called after handling the request.
    //      void after_handle(request& req, response& res, context& ctx)
    //      template <typename AllContext>
    //      void after_handle(request& req, response& res, context& ctx, AllContext& all_ctx)

    struct CookieParser
    {
        struct context
        {
            std::unordered_map<std::string, std::string> jar;
            std::unordered_map<std::string, std::string> cookies_to_add;

            std::string get_cookie(const std::string& key) const
            {
                auto cookie = jar.find(key);
                if (cookie != jar.end())
                    return cookie->second;
                return {};
            }

            void set_cookie(const std::string& key, const std::string& value)
            {
                cookies_to_add.emplace(key, value);
            }
        };

        void before_handle(request& req, response& res, context& ctx)
        {
            auto cookie_headers = req.headers.find("Cookie");
            if (cookie_headers == req.headers.end())
                return;

            std::string cookies{cookie_headers->second};
            // Break the cookie value string up into tokens that are delimited
            // by ';'. If there are quotes, they are properly escaped.
            auto tokens = boost::tokenizer<boost::escaped_list_separator<char>>{cookies, boost::escaped_list_separator<char>{'\\', ';', '"'}};
            for (const std::string& cookie : tokens)
            {
                size_t pos_equal = cookie.find('=');
                if (pos_equal == std::string::npos)
                    break;

                std::string name{cookie.substr(0, pos_equal)};
                std::string value{cookie.substr(pos_equal+1)};
                boost::trim(name);
                boost::trim(value);
                ctx.jar.emplace(std::move(name), std::move(value));
            }
        }

        void after_handle(request& req, response& res, context& ctx)
        {
            for(auto& cookie:ctx.cookies_to_add)
            {
                res.add_header("Set-Cookie", cookie.first + "=" + cookie.second);
            }
        }
    };

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
