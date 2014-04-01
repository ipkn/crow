#pragma once

namespace flask
{
    struct request
    {
        std::string url;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };
}
