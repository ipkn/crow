#pragma once
#include <string>
#include <unordered_map>

namespace flask
{
    struct response
    {
        std::string body;
        int code{200};
        std::unordered_map<std::string, std::string> headers;
        response() {}
        explicit response(int code) : code(code) {}
        response(std::string body) : body(std::move(body)) {}
        response(int code, std::string body) : body(std::move(body)), code(code) {}
    };
}
