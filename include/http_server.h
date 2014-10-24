#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio.hpp>
#include <cstdint>
#include <atomic>
#include <future>
#include <vector>

#include <memory>

#include "http_connection.h"
#include "datetime.h"
#include "logging.h"
#include "dumb_timer_queue.h"

namespace crow
{
    using namespace boost;
    using tcp = asio::ip::tcp;
    
    template <typename Handler, typename ... Middlewares>
    class Server
    {
    public:
        Server(Handler* handler, uint16_t port, std::tuple<Middlewares...>* middlewares = nullptr, uint16_t concurrency = 1)
            : acceptor_(io_service_, tcp::endpoint(asio::ip::address(), port)), 
            signals_(io_service_, SIGINT, SIGTERM),
            handler_(handler), 
            concurrency_(concurrency),
            port_(port),
            middlewares_(middlewares)
        {
        }

        void run()
        {
            if (concurrency_ < 0)
                concurrency_ = 1;

            for(int i = 0; i < concurrency_;  i++)
                io_service_pool_.emplace_back(new boost::asio::io_service());

            std::vector<std::future<void>> v;
            for(uint16_t i = 0; i < concurrency_; i ++)
                v.push_back(
                        std::async(std::launch::async, [this, i]{
                            // initializing timer queue
                            auto& timer_queue = detail::dumb_timer_queue::get_current_dumb_timer_queue();

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
                [&](const boost::system::error_code& error, int signal_number){
                    stop();
                });

            do_accept();

            v.push_back(std::async(std::launch::async, [this]{
                io_service_.run();
                CROW_LOG_INFO << "Exiting.";
            }));
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
            auto p = new Connection<Handler, Middlewares...>(pick_io_service(), handler_, server_name_, middlewares_);
            acceptor_.async_accept(p->socket(), 
                [this, p](boost::system::error_code ec)
                {
                    if (!ec)
                    {
                        p->start();
                    }
                    do_accept();
                });
        }

    private:
        asio::io_service io_service_;
        std::vector<std::unique_ptr<asio::io_service>> io_service_pool_;
        tcp::acceptor acceptor_;
        boost::asio::signal_set signals_;

        Handler* handler_;
        uint16_t concurrency_{1};
        std::string server_name_ = "Crow/0.1";
        uint16_t port_;
        unsigned int roundrobin_index_{};

        std::tuple<Middlewares...>* middlewares_;
    };
}
