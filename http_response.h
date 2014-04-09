#pragma once
#include <string>
#include <unordered_map>

namespace flask
{
    std::unordered_map<int, std::string> statusCodes = {
        {200, "HTTP/1.1 200 OK\r\n"},
        {201, "HTTP/1.1 201 Created\r\n"},
        {202, "HTTP/1.1 202 Accepted\r\n"},
        {204, "HTTP/1.1 204 No Content\r\n"},

        {300, "HTTP/1.1 300 Multiple Choices\r\n"},
        {301, "HTTP/1.1 301 Moved Permanently\r\n"},
        {302, "HTTP/1.1 302 Moved Temporarily\r\n"},
        {304, "HTTP/1.1 304 Not Modified\r\n"},

        {400, "HTTP/1.1 400 Bad Request\r\n"},
        {401, "HTTP/1.1 401 Unauthorized\r\n"},
        {403, "HTTP/1.1 403 Forbidden\r\n"},
        {404, "HTTP/1.1 404 Not Found\r\n"},

        {500, "HTTP/1.1 500 Internal Server Error\r\n"},
        {501, "HTTP/1.1 501 Not Implemented\r\n"},
        {502, "HTTP/1.1 502 Bad Gateway\r\n"},
        {503, "HTTP/1.1 503 Service Unavailable\r\n"},
    };

    struct response
    {
        int status = 200;
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        response() {}
        response(int status) : status(status) {}
        response(std::string body) : body(std::move(body)) {}
        response(std::string body, int status) : body(std::move(body)), status(status) {}
    };
}
