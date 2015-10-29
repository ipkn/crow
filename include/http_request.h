#pragma once

#include <boost/algorithm/string.hpp>
#include "common.h"
#include "ci_map.h"
#include "query_string.h"

namespace crow
{
    template <typename T>
    inline const std::string& get_header_value(const T& headers, const std::string& key)
    {
        if (headers.count(key))
        {
            return headers.find(key)->second;
        }
        static std::string empty;
        return empty;
    }

    struct request
    {
        HTTPMethod method;
        std::string raw_url;
        std::string url;
        query_string url_params;
        ci_map headers;
        std::unordered_map<std::string, std::string> cookies;
        std::string body;

        void* middleware_context{};

        request()
            : method(HTTPMethod::GET)
        {
        }

        request(HTTPMethod method, std::string raw_url, std::string url, query_string url_params, ci_map headers, std::string body)
            : method(method), raw_url(std::move(raw_url)), url(std::move(url)), url_params(std::move(url_params)), headers(std::move(headers)), body(std::move(body))
        {
            read_cookies();
        }

        void add_header(std::string key, std::string value)
        {
            headers.emplace(std::move(key), std::move(value));
            read_cookies();
        }

        const std::string& get_header_value(const std::string& key) const
        {
            return crow::get_header_value(headers, key);
        }

        std::string get_cookie(const std::string& key) const
        {
          auto cookie = cookies.find(key);
          if (cookie == cookies.end())
              return "";
          else
              return cookie->second;
        }
        
        void read_cookies() {
            if (headers.find("Cookie") == headers.end())
                return;

            std::vector<std::string> tokens;
            boost::split(tokens, get_header_value("Cookie"), boost::is_any_of("; "));

            for (auto& token : tokens)
            {
                int pos = token.find('=');
                std::string key{token.substr(0, pos)};
                std::string value{token.substr(pos + 1)};
                cookies.emplace(std::move(key), std::move(value));
            }
        }
    };
}
