#pragma once

#include <string>
#include <functional>
#include <memory>
#include <future>
#include <cstdint>
#include <type_traits>
#include <thread>

#include "settings.h"
#include "logging.h" 
#include "http_server.h"
#include "utility.h"
#include "routing.h"
#include "middleware_context.h"
#include "http_request.h"

#define CROW_ROUTE(app, url) app.route<crow::black_magic::get_parameter_tag(url)>(url)

namespace crow
{
    template <typename ... Middlewares>
    class Crow
    {
    public:
        using self_t = Crow;
        using server_t = Server<Crow, Middlewares...>;
        Crow()
        : server_(this)
        {
        }

        void handle(const request& req, response& res)
        {
            router_.handle(req, res);
        }

        template <uint64_t Tag>
        auto route(std::string&& rule)
            -> typename std::result_of<decltype(&Router::new_rule_tagged<Tag>)(Router, std::string&&)>::type
        {
            return router_.new_rule_tagged<Tag>(std::move(rule));
        }

        self_t& port(std::uint16_t port)
        {
            server_.set_port(port);
            return *this;
        }

        self_t& multithreaded()
        {
            return concurrency(std::thread::hardware_concurrency());
        }

        self_t& concurrency(std::uint16_t concurrency)
        {
            server_.set_concurrency(concurrency);
            return *this;
        }

        void validate()
        {
            router_.validate();
        }

        void run()
        {
            validate();
            server_.run();
        }

        void debug_print()
        {
            CROW_LOG_DEBUG << "Routing:";
            router_.debug_print();
        }

        // middleware
        using context_t = detail::context<Middlewares...>;
        template <typename T>
        typename T::context& get_context(const request& req)
        {
            static_assert(black_magic::contains<T, Middlewares...>::value, "App doesn't have the specified middleware type.");
            auto& ctx = *reinterpret_cast<context_t*>(req.middleware_context);
            return ctx.template get<T>();
        }

        template <typename T>
        T* get_middleware()
        {
            static_assert(black_magic::contains<T, Middlewares...>::value, "App doesn't have the specified middleware type.");
            return server_.template get_middleware<T>();
        }

    private:

        server_t server_;
        Router router_;
    };
    template <typename ... Middlewares>
    using App = Crow<Middlewares...>;
    using SimpleApp = Crow<>;
};

