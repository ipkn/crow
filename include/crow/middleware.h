#pragma once
#include <boost/algorithm/string/trim.hpp>
#include "crow/http_request.h"
#include "crow/http_response.h"

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

            std::string get_cookie(const std::string& key)
            {
                if (jar.count(key))
                    return jar[key];
                return {};
            }

            void set_cookie(const std::string& key, const std::string& value)
            {
                cookies_to_add.emplace(key, value);
            }
        };

        void before_handle(request& req, response& res, context& ctx)
        {
            int count = req.headers.count("Cookie");
            if (!count)
                return;
            if (count > 1)
            {
                res.code = 400;
                res.end();
                return;
            }
            std::string cookies = req.get_header_value("Cookie");
            size_t pos = 0;
            while(pos < cookies.size())
            {
                size_t pos_equal = cookies.find('=', pos);
                if (pos_equal == cookies.npos)
                    break;
                std::string name = cookies.substr(pos, pos_equal-pos);
                boost::trim(name);
                pos = pos_equal+1;
                while(pos < cookies.size() && cookies[pos] == ' ') pos++;
                if (pos == cookies.size())
                    break;

                std::string value;

                if (cookies[pos] == '"')
                {
                    int dquote_meet_count = 0;
                    pos ++;
                    size_t pos_dquote = pos-1;
                    do
                    {
                        pos_dquote = cookies.find('"', pos_dquote+1);
                        dquote_meet_count ++;
                    } while(pos_dquote < cookies.size() && cookies[pos_dquote-1] == '\\');
                    if (pos_dquote == cookies.npos)
                        break;

                    if (dquote_meet_count == 1)
                        value = cookies.substr(pos, pos_dquote - pos);
                    else
                    {
                        value.clear();
                        value.reserve(pos_dquote-pos);
                        for(size_t p = pos; p < pos_dquote; p++)
                        {
                            // FIXME minimal escaping
                            if (cookies[p] == '\\' && p + 1 < pos_dquote)
                            {
                                p++;
                                if (cookies[p] == '\\' || cookies[p] == '"')
                                    value += cookies[p];
                                else
                                {
                                    value += '\\';
                                    value += cookies[p];
                                }
                            }
                            else
                                value += cookies[p];
                        }
                    }

                    ctx.jar.emplace(std::move(name), std::move(value));
                    pos = cookies.find(";", pos_dquote+1);
                    if (pos == cookies.npos)
                        break;
                    pos++;
                    while(pos < cookies.size() && cookies[pos] == ' ') pos++;
                    if (pos == cookies.size())
                        break;
                }
                else
                {
                    size_t pos_semicolon = cookies.find(';', pos);
                    value = cookies.substr(pos, pos_semicolon - pos);
                    boost::trim(value);
                    ctx.jar.emplace(std::move(name), std::move(value));
                    pos = pos_semicolon;
                    if (pos == cookies.npos)
                        break;
                    pos ++;
                    while(pos < cookies.size() && cookies[pos] == ' ') pos++;
                    if (pos == cookies.size())
                        break;
                }
            }
        }

        void after_handle(request& /*req*/, response& res, context& ctx)
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
