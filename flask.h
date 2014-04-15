#pragma once
#include <string>
#include <functional>
#include <memory>
#include <future>
#include <stdint.h>
#include <type_traits>

#include "http_server.h"
#include "utility.h"
#include "routing.h"

// TEST
#include <iostream>

#define FLASK_ROUTE(app, url) app.route<flask::black_magic::get_parameter_tag(url)>(url)

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

        template <uint64_t Tag>
        auto route(std::string&& rule)
            -> typename std::result_of<decltype(&Router::new_rule_tagged<Tag>)(Router, std::string&&)>::type
        {
            return router_.new_rule_tagged<Tag>(std::move(rule));
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
        void debug_print()
        {
            std::cerr << "Routing:" << std::endl;
            router_.debug_print();
        }

    private:
        uint16_t port_ = 80;

        Router router_;
    };
};

