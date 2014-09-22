#pragma once

#include "common.h"
#include "ci_map.h"

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
        std::string url;
        ci_map url_params;
        ci_map headers;
        std::string body;

        void* middleware_context{};

        request()
            : method(HTTPMethod::GET)
        {
        }

        request(HTTPMethod method, std::string url, ci_map url_params, ci_map headers, std::string body)
            : method(method), url(std::move(url)), url_params(std::move(url_params)), headers(std::move(headers)), body(std::move(body))
        {
        }

        void add_header(std::string key, std::string value)
        {
            headers.emplace(std::move(key), std::move(value));
        }

        const std::string& get_header_value(const std::string& key)
        {
            return crow::get_header_value(headers, key);
        }

    };
}
