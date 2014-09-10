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
        ci_map headers;
        std::string body;

        void add_header(std::string key, std::string value)
        {
            headers.emplace(std::move(key), std::move(value));
        }

        const std::string& get_header_value(const std::string& key)
        {
            return crow::get_header_value(headers, key);
        }

        void* middleware_context{};
    };
}
