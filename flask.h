#pragma once
#include <string>
#include <functional>
#include <memory>
#include <future>
#include <stdint.h>

#include "http_response.h"
#include "http_server.h"

// TEST
#include <iostream>

namespace flask
{
    class Flask
    {
    public:
        Flask()
        {
        }

        response handle(const request& req)
        {
            if (yameHandlers_.count(req.url) == 0)
            {
                return response(404);
            }
            return yameHandlers_[req.url]();
        }

        template <typename F>
        void route(const std::string& url, F f)
        {
            auto yameHandler = [f]{
                return response(f());
            };
            yameHandlers_.emplace(url, yameHandler);
        }

        Flask& port(std::uint16_t port)
        {
            port_ = port;
            return *this;
        }

        void run()
        {
            Server<Flask> server(this, port_);
            server.run();
        }
    private:
        uint16_t port_ = 80;

        // Someday I will become real handler!
        std::unordered_map<std::string, std::function<response()>> yameHandlers_;
    };
};

