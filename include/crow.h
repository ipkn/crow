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
#include "utility.h"
#include "routing.h"
#include "middleware_context.h"
#include "http_request.h"
#include "http_server.h"


#ifdef CROW_MSVC_WORKAROUND
#define CROW_ROUTE(app, url) app.route_dynamic(url)
#else
#define CROW_ROUTE(app, url) app.route<crow::black_magic::get_parameter_tag(url)>(url)
#endif

namespace crow
{
#ifdef CROW_ENABLE_SSL
    using ssl_context_t = boost::asio::ssl::context;
#endif
    template <typename ... Middlewares>
    class Crow
    {
    public:
        using self_t = Crow;
        using server_t = Server<Crow, SocketAdaptor, Middlewares...>;
#ifdef CROW_ENABLE_SSL
        using ssl_server_t = Server<Crow, SSLAdaptor, Middlewares...>;
#endif
        Crow()
        {
        }

        void handle(const request& req, response& res)
        {
            router_.handle(req, res);
        }

        DynamicRule& route_dynamic(std::string&& rule)
        {
            return router_.new_rule_dynamic(std::move(rule));
        }

        template <uint64_t Tag>
        auto route(std::string&& rule)
            -> typename std::result_of<decltype(&Router::new_rule_tagged<Tag>)(Router, std::string&&)>::type
        {
            return router_.new_rule_tagged<Tag>(std::move(rule));
        }

        self_t& port(std::uint16_t port)
        {
            port_ = port;
            return *this;
        }

        self_t& multithreaded()
        {
            return concurrency(std::thread::hardware_concurrency());
        }

        self_t& concurrency(std::uint16_t concurrency)
        {
            if (concurrency < 1)
                concurrency = 1;
            concurrency_ = concurrency;
            return *this;
        }

        void validate()
        {
            router_.validate();
        }

        void run()
        {
            validate();
#ifdef CROW_ENABLE_SSL
            if (use_ssl_)
            {
                ssl_server_t server(this, port_, &middlewares_, concurrency_, &ssl_context_);
                server.run();
            }
            else
#endif
            {
                server_t server(this, port_, &middlewares_, concurrency_, nullptr);
                server.run();
            }
        }

        void debug_print()
        {
            CROW_LOG_DEBUG << "Routing:";
            router_.debug_print();
        }

#ifdef CROW_ENABLE_SSL
        self_t& ssl_file(const std::string& crt_filename, const std::string& key_filename)
        {
            use_ssl_ = true;
            ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
            ssl_context_.use_certificate_file(crt_filename, ssl_context_t::pem);
            ssl_context_.use_private_key_file(key_filename, ssl_context_t::pem);
            ssl_context_.set_options(
                    boost::asio::ssl::context::default_workarounds
                          | boost::asio::ssl::context::no_sslv2
                          | boost::asio::ssl::context::no_sslv3
                    );
            return *this;
        }

        self_t& ssl_file(const std::string& pem_filename)
        {
            use_ssl_ = true;
            ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
            ssl_context_.load_verify_file(pem_filename);
            ssl_context_.set_options(
                    boost::asio::ssl::context::default_workarounds
                          | boost::asio::ssl::context::no_sslv2
                          | boost::asio::ssl::context::no_sslv3
                    );
            return *this;
        }

        self_t& ssl(boost::asio::ssl::context&& ctx)
        {
            use_ssl_ = true;
            ssl_context_ = std::move(ctx);
            return *this;
        }


        bool use_ssl_{false};
        ssl_context_t ssl_context_{boost::asio::ssl::context::sslv23};

#else
        template <typename T, typename ... Remain>
        self_t& ssl_file(T&& t, Remain&&...)
        {
            // We can't call .ssl() member function unless CROW_ENABLE_SSL is defined.
            static_assert(
                    // make static_assert dependent to T; always false
                    std::is_base_of<T, void>::value, 
                    "Define CROW_ENABLE_SSL to enable ssl support.");
            return *this;
        }

        template <typename T>
        self_t& ssl(T&& ctx)
        {
            // We can't call .ssl() member function unless CROW_ENABLE_SSL is defined.
            static_assert(
                    // make static_assert dependent to T; always false
                    std::is_base_of<T, void>::value, 
                    "Define CROW_ENABLE_SSL to enable ssl support.");
            return *this;
        }
#endif

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
        T& get_middleware()
        {
            return utility::get_element_by_type<T, Middlewares...>(middlewares_);
        }

    private:
        uint16_t port_ = 80;
        uint16_t concurrency_ = 1;

        Router router_;

        std::tuple<Middlewares...> middlewares_;
    };
    template <typename ... Middlewares>
    using App = Crow<Middlewares...>;
    using SimpleApp = Crow<>;
};

