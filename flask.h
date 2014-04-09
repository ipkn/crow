#pragma once
#include <string>
#include <functional>
#include <memory>
#include <future>
#include <stdint.h>
#include <type_traits>

#include "http_server.h"
#include "routing.h"

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
            return router_.handle(req);
        }

        auto route(std::string&& rule)
            -> typename std::result_of<decltype(&Router::new_rule)(Router, std::string&&)>::type
        {
            return router_.new_rule(std::move(rule));
        }

        Flask& port(std::uint16_t port)
        {
            port_ = port;
            return *this;
        }

        void validate()
        {
            router_.validate();
        }

        void run()
        {
            validate();
            Server<Flask> server(this, port_);
            server.run();
        }
    private:
        uint16_t port_ = 80;

        Router router_;
    };
};

