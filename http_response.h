#pragma once
#include <string>
#include <unordered_map>
#include "json.h"

namespace flask
{
    struct response
    {
        std::string body;
        json::wvalue json_value;
        int code{200};
        std::unordered_map<std::string, std::string> headers;
        response() {}
        explicit response(int code) : code(code) {}
        response(std::string body) : body(std::move(body)) {}
        response(json::wvalue&& json_value) : json_value(std::move(json_value)) {}
        response(const json::wvalue& json_value) : body(json::encode(json_value)) {}
        response(int code, std::string body) : body(std::move(body)), code(code) {}
        response(response&& r)
        {
            *this = std::move(r);
        }
        response& operator = (response&& r)
        {
            body = std::move(r.body);
            json_value = std::move(r.json_value);
            code = r.code;
            headers = std::move(r.headers);
            return *this;
        }
    };
}
