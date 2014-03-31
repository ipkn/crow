#pragma once
#include <string>
#include <functional>
#include <memory>
#include <future>
#include <stdint.h>

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

        void handle()
        {
        }

        void route(const std::string& url, std::function<std::string()> f)
        {
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
    };
};

