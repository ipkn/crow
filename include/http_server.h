#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio.hpp>
#include <boost/variant.hpp>
#include <cstdint>
#include <atomic>
#include <future>
#include <typeinfo>
#include <typeindex>

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
        Server(Handler* handler)
            : acceptor_(io_service_), 
            signals_(io_service_, SIGINT, SIGTERM),
            handler_(handler)
        {
        }

        void run()
        {
            tcp::endpoint endpoint(asio::ip::address(), port_);
            acceptor_.open(endpoint.protocol());
            acceptor_.set_option(tcp::acceptor::reuse_address(true));
            acceptor_.bind(endpoint);
            acceptor_.listen();

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

        void set_port(uint16_t port)
        {
            port_ = port;
        }

        void set_concurrency(uint16_t concurrency) 
        {
            if (concurrency < 1)
                concurrency = 1;
            concurrency_ = concurrency;
        }

        /*template <int N>
        void* get_middleware_helper(type_index t_ti)
        {
            if(N >= 2) {//tuple_size<decltype(middlewares_)>::value){
                return nullptr;
            }

            decltype(std::get<N>(middlewares_)) mw = std::get<N>(middlewares_);
            type_index mw_ti = typeid(mw);
            
            if(mw_ti == t_ti) {
                //return nullptr;
                return (void*)&std::get<N>(middlewares_);

            } else {
                //return nullptr;
                return get_middleware_helper<N+1>(t_ti);
            }
        }*/

        // http://stackoverflow.com/a/8194518/254190 (edited a bit)
        template <size_t n, typename... T>
        void* dynamic_get_impl(size_t i, const std::tuple<T...>& tpl)
        {
            if (i == n)
                return (void*)&std::get<n>(tpl);
            else if (n == sizeof...(T) - 1)
                throw std::out_of_range("Tuple element out of range.");
            else
                return dynamic_get_impl<(n < sizeof...(T)-1 ? n+1 : 0)>(i, tpl);
        }

        template <typename U, typename... T>
        U* dynamic_get(size_t i, const std::tuple<T...>& tpl)
        {
            return static_cast<U*>(dynamic_get_impl<0>(i, tpl));
        }

        template <typename T>
        T* get_middleware()
        {
            type_index t_ti = typeid(T);

            for(int i = 0; i < tuple_size<decltype(middlewares_)>::value; ++i) {
                auto mw = dynamic_get<T, Middlewares...>(i, middlewares_);
                if(type_index(typeid(*mw)) == t_ti) {
                    return mw;
                }
            }
            return nullptr;
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
        uint16_t port_{80};
        unsigned int roundrobin_index_{};

        std::tuple<Middlewares...> middlewares_;

    };
}
