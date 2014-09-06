#pragma once

#include "common.h"

namespace crow
{
    struct request
    {
        HTTPMethod method;
        std::string url;
        std::unordered_map<std::string, std::string> headers;
        std::string body;

        void* middleware_context{};
    };
}
