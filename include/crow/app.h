#pragma once

#include <chrono>
#include <string>
#include <functional>
#include <memory>
#include <future>
#include <cstdint>
#include <type_traits>
#include <thread>
#include <condition_variable>

#include "crow/settings.h"
#include "crow/logging.h"
#include "crow/utility.h"
#include "crow/routing.h"
#include "crow/middleware_context.h"
#include "crow/http_request.h"
#include "crow/http_server.h"
#include "crow/dumb_timer_queue.h"


#ifdef CROW_MSVC_WORKAROUND
#define CROW_ROUTE(app, url) app.route_dynamic(url)
#else
#define CROW_ROUTE(app, url) app.route<crow::black_magic::get_parameter_tag(url)>(url)
#endif

namespace crow
{
    int detail::dumb_timer_queue::tick = 5;
#ifdef CROW_ENABLE_SSL
    using ssl_context_t = boost::asio::ssl::context;
#endif
    ///The main server application

    ///
    /// Use `SimpleApp` or `App<Middleware1, Middleware2, etc...>`
    template <typename ... Middlewares>
    class Crow
    {
    public:
        ///This crow application
        using self_t = Crow;
        ///The HTTP server
        using server_t = Server<Crow, SocketAdaptor, Middlewares...>;
#ifdef CROW_ENABLE_SSL
        ///An HTTP server that runs on SSL with an SSLAdaptor
        using ssl_server_t = Server<Crow, SSLAdaptor, Middlewares...>;
#endif
        Crow()
        {
        }

        ///Process an Upgrade request

        ///
        ///Currently used to upgrrade an HTTP connection to a WebSocket connection
        template <typename Adaptor>
        void handle_upgrade(const request& req, response& res, Adaptor&& adaptor)
        {
            router_.handle_upgrade(req, res, adaptor);
        }

        ///Process the request and generate a response for it
        void handle(const request& req, response& res)
        {
            router_.handle(req, res);
        }

        ///Create a dynamic route using a rule (**Use CROW_ROUTE instead**)
        DynamicRule& route_dynamic(std::string&& rule)
        {
            return router_.new_rule_dynamic(std::move(rule));
        }

        ///Create a route using a rule (**Use CROW_ROUTE instead**)
        template <uint64_t Tag>
        auto route(std::string&& rule)
            -> typename std::result_of<decltype(&Router::new_rule_tagged<Tag>)(Router, std::string&&)>::type
        {
            return router_.new_rule_tagged<Tag>(std::move(rule));
        }

        self_t& signal_clear()
        {
            signals_.clear();
            return *this;
        }

        self_t& signal_add(int signal_number)
        {
            signals_.push_back(signal_number);
            return *this;
        }

        ///Set the port that Crow will handle requests on
        self_t& port(std::uint16_t port)
        {
            port_ = port;
            return *this;
        }

        ///Set the connection timeout in seconds (default is 5)
        self_t& timeout(std::uint8_t timeout)
        {
            detail::dumb_timer_queue::tick = timeout;
            return *this;
        }

        ///Set the server name (default Crow/0.2)
        self_t& server_name(std::string server_name)
        {
            server_name_ = server_name;
            return *this;
        }

        ///The IP address that Crow will handle requests on (default is 0.0.0.0)
        self_t& bindaddr(std::string bindaddr)
        {
            bindaddr_ = bindaddr;
            return *this;
        }

        ///Run the server on multiple threads using all available threads
        self_t& multithreaded()
        {
            return concurrency(std::thread::hardware_concurrency());
        }

        ///Run the server on multiple threads using a specific number
        self_t& concurrency(std::uint16_t concurrency)
        {
            if (concurrency < 1)
                concurrency = 1;
            concurrency_ = concurrency;
            return *this;
        }

        ///Set the server's log level

        ///
        /// Possible values are:<br>
        /// crow::LogLevel::Debug       (0)<br>
        /// crow::LogLevel::Info        (1)<br>
        /// crow::LogLevel::Warning     (2)<br>
        /// crow::LogLevel::Error       (3)<br>
        /// crow::LogLevel::Critical    (4)<br>
        self_t& loglevel(crow::LogLevel level)
        {
            crow::logger::setLogLevel(level);
            return *this;
        }

        ///Set a custom duration and function to run on every tick
        template <typename Duration, typename Func>
        self_t& tick(Duration d, Func f) {
            tick_interval_ = std::chrono::duration_cast<std::chrono::milliseconds>(d);
            tick_function_ = f;
            return *this;
        }

        ///A wrapper for `validate()` in the router

        ///
        ///Go through the rules, upgrade them if possible, and add them to the list of rules
        void validate()
        {
            router_.validate();
        }

        ///Notify anything using `wait_for_server_start()` to proceed
        void notify_server_start()
        {
            std::unique_lock<std::mutex> lock(start_mutex_);
            server_started_ = true;
            cv_started_.notify_all();
        }

        ///Run the server
        void run()
        {
#ifndef CROW_DISABLE_STATIC_DIR
            route<crow::black_magic::get_parameter_tag(CROW_STATIC_ENDPOINT)>(CROW_STATIC_ENDPOINT)
            ([](const crow::request&, crow::response& res, std::string file_path_partial)
            {
              res.set_static_file_info(CROW_STATIC_DIRECTORY + file_path_partial);
              res.end();
            });
            validate();
#endif

#ifdef CROW_ENABLE_SSL
            if (use_ssl_)
            {
                ssl_server_ = std::move(std::unique_ptr<ssl_server_t>(new ssl_server_t(this, bindaddr_, port_, server_name_, &middlewares_, concurrency_, &ssl_context_)));
                ssl_server_->set_tick_function(tick_interval_, tick_function_);
                notify_server_start();
                ssl_server_->run();
            }
            else
#endif
            {
                server_ = std::move(std::unique_ptr<server_t>(new server_t(this, bindaddr_, port_, server_name_, &middlewares_, concurrency_, nullptr)));
                server_->set_tick_function(tick_interval_, tick_function_);
                server_->signal_clear();
                for (auto snum : signals_)
                {
                    server_->signal_add(snum);
                }
                notify_server_start();
                server_->run();
            }
        }

        ///Stop the server
        void stop()
        {
#ifdef CROW_ENABLE_SSL
            if (use_ssl_)
            {
		if (ssl_server_) {
                    ssl_server_->stop();
		}
            }
            else
#endif
            {
		if (server_) {
                    server_->stop();
		}
            }
        }

        void debug_print()
        {
            CROW_LOG_DEBUG << "Routing:";
            router_.debug_print();
        }


#ifdef CROW_ENABLE_SSL

        ///use certificate and key files for SSL
        self_t& ssl_file(const std::string& crt_filename, const std::string& key_filename)
        {
            use_ssl_ = true;
            ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
            ssl_context_.set_verify_mode(boost::asio::ssl::verify_client_once);
            ssl_context_.use_certificate_file(crt_filename, ssl_context_t::pem);
            ssl_context_.use_private_key_file(key_filename, ssl_context_t::pem);
            ssl_context_.set_options(
                    boost::asio::ssl::context::default_workarounds
                          | boost::asio::ssl::context::no_sslv2
                          | boost::asio::ssl::context::no_sslv3
                    );
            return *this;
        }

        ///use .pem file for SSL
        self_t& ssl_file(const std::string& pem_filename)
        {
            use_ssl_ = true;
            ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
            ssl_context_.set_verify_mode(boost::asio::ssl::verify_client_once);
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
        self_t& ssl_file(T&&, Remain&&...)
        {
            // We can't call .ssl() member function unless CROW_ENABLE_SSL is defined.
            static_assert(
                    // make static_assert dependent to T; always false
                    std::is_base_of<T, void>::value,
                    "Define CROW_ENABLE_SSL to enable ssl support.");
            return *this;
        }

        template <typename T>
        self_t& ssl(T&&)
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

        ///Wait until the server has properly started
        void wait_for_server_start()
        {
            std::unique_lock<std::mutex> lock(start_mutex_);
            if (server_started_)
                return;
            cv_started_.wait(lock);
        }

    private:
        uint16_t port_ = 80;
        uint16_t concurrency_ = 1;
        std::string server_name_ = "Crow/0.2";
        std::string bindaddr_ = "0.0.0.0";
        Router router_;

        std::chrono::milliseconds tick_interval_;
        std::function<void()> tick_function_;

        std::tuple<Middlewares...> middlewares_;

#ifdef CROW_ENABLE_SSL
        std::unique_ptr<ssl_server_t> ssl_server_;
#endif
        std::unique_ptr<server_t> server_;

        std::vector<int> signals_{SIGINT, SIGTERM};

        bool server_started_{false};
        std::condition_variable cv_started_;
        std::mutex start_mutex_;
    };
    template <typename ... Middlewares>
    using App = Crow<Middlewares...>;
    using SimpleApp = Crow<>;
}
