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

        response handle()
        {
            return yameHandler_();
        }

        template <typename F>
        void route(const std::string& url, F f)
        {
            yameHandler_ = [f]{
                return response(f());
            };
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
        std::function<response()> yameHandler_;
    };
};

