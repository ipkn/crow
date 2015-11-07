#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio.hpp>
#ifdef CROW_ENABLE_SSL
#include <boost/asio/ssl.hpp>
#endif
#include <cstdint>
#include <atomic>
#include <future>
#include <vector>

#include <memory>

#include "http_connection.h"
#include "logging.h"
#include "dumb_timer_queue.h"

namespace crow
{
    using namespace boost;
    using tcp = asio::ip::tcp;
    
    template <typename Handler, typename Adaptor = SocketAdaptor, typename ... Middlewares>
    class Server
    {
    public:
        Server(Handler* handler, uint16_t port, std::tuple<Middlewares...>* middlewares = nullptr, uint16_t concurrency = 1, typename Adaptor::context* adaptor_ctx = nullptr)
            : acceptor_(io_service_, tcp::endpoint(asio::ip::address(), port)), 
            signals_(io_service_, SIGINT, SIGTERM),
            handler_(handler), 
            concurrency_(concurrency),
            port_(port),
            middlewares_(middlewares),
            adaptor_ctx_(adaptor_ctx)
        {
        }

        void run()
        {
            if (concurrency_ < 0)
                concurrency_ = 1;

            for(int i = 0; i < concurrency_;  i++)
                io_service_pool_.emplace_back(new boost::asio::io_service());
            get_cached_date_str_pool_.resize(concurrency_);
            timer_queue_pool_.resize(concurrency_);

            std::vector<std::future<void>> v;
            for(uint16_t i = 0; i < concurrency_; i ++)
                v.push_back(
                        std::async(std::launch::async, [this, i]{

                            // thread local date string get function
                            auto last = std::chrono::steady_clock::now();

                            std::string date_str;
                            auto update_date_str = [&]
                            {
                                auto last_time_t = time(0);
                                tm my_tm;

#ifdef _MSC_VER
                                gmtime_s(&my_tm, &last_time_t);
#else
                                gmtime_r(&last_time_t, &my_tm);
#endif
                                date_str.resize(100);
                                size_t date_str_sz = strftime(&date_str[0], 99, "%a, %d %b %Y %H:%M:%S GMT", &my_tm);
                                date_str.resize(date_str_sz);
                            };
                            update_date_str();
                            get_cached_date_str_pool_[i] = [&]()->std::string
                            {
                                if (std::chrono::steady_clock::now() - last >= std::chrono::seconds(1))
                                {
                                    last = std::chrono::steady_clock::now();
                                    update_date_str();
                                }
                                return date_str;
                            };

                            // initializing timer queue
                            detail::dumb_timer_queue timer_queue;
                            timer_queue_pool_[i] = &timer_queue;

                            timer_queue.set_io_service(*io_service_pool_[i]);
                            boost::asio::deadline_timer timer(*io_service_pool_[i]);
                            timer.expires_from_now(boost::posix_time::seconds(1));

                            std::function<void(const boost::system::error_code& ec)> handler;
                            handler = [&](const boost::system::error_code& ec){
                                if (ec)
                                    return;
                                timer_queue.process();
                                timer.expires_from_now(boost::posix_time::seconds(1));
                                timer.async_wait(handler);
                            };
                            timer.async_wait(handler);

                            io_service_pool_[i]->run();
                        }));
            CROW_LOG_INFO << server_name_ << " server is running, local port " << port_;

            signals_.async_wait(
                [&](const boost::system::error_code& /*error*/, int /*signal_number*/){
                    stop();
                });

            for (int i = 0; i < concurrency_; i++)
            {
                while (timer_queue_pool_[i] == nullptr)
                    std::this_thread::yield();
            }

            do_accept();

            std::thread([this]{
                io_service_.run();
                CROW_LOG_INFO << "Exiting.";
            }).join();
        }

        void stop()
        {
            io_service_.stop();
            for(auto& io_service:io_service_pool_)
                io_service->stop();
        }

    private:
        asio::io_service& pick_io_service()
        {
            // TODO load balancing
            roundrobin_index_++;
            if (roundrobin_index_ >= io_service_pool_.size())
                roundrobin_index_ = 0;
            return *io_service_pool_[roundrobin_index_];
        }

        void do_accept()
        {
            asio::io_service& is = pick_io_service();
            auto p = new Connection<Adaptor, Handler, Middlewares...>(
                is, handler_, server_name_, middlewares_,
                get_cached_date_str_pool_[roundrobin_index_], *timer_queue_pool_[roundrobin_index_],
                adaptor_ctx_);
            acceptor_.async_accept(p->socket(), 
                [this, p, &is](boost::system::error_code ec)
                {
                    if (!ec)
                    {
                        is.post([p]
                        {
                            p->start();
                        });
                    }
                    do_accept();
                });
        }

    private:
        asio::io_service io_service_;
        std::vector<std::unique_ptr<asio::io_service>> io_service_pool_;
        std::vector<detail::dumb_timer_queue*> timer_queue_pool_;
        std::vector<std::function<std::string()>> get_cached_date_str_pool_;
        tcp::acceptor acceptor_;
        boost::asio::signal_set signals_;

        Handler* handler_;
        uint16_t concurrency_{1};
        std::string server_name_ = "Crow/0.1";
        uint16_t port_;
        unsigned int roundrobin_index_{};

        std::tuple<Middlewares...>* middlewares_;

#ifdef CROW_ENABLE_SSL
        bool use_ssl_{false};
        boost::asio::ssl::context ssl_context_{boost::asio::ssl::context::sslv23};
#endif
        typename Adaptor::context* adaptor_ctx_;
    };
}
