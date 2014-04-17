#pragma once

#include <boost/asio.hpp>
#include <stdint.h>

#include "http_connection.h"

// TEST
#include <iostream>

namespace flask
{
    using namespace boost;
    using tcp = asio::ip::tcp;
    template <typename Handler>
    class Server
    {
    public:
        Server(Handler* handler, uint16_t port, uint16_t concurrency = 1)
            : acceptor_(io_service_, tcp::endpoint(asio::ip::address(), port)), socket_(io_service_), handler_(handler), concurrency_(concurrency)
        {
            do_accept();
        }

        void run()
        {
            std::vector<std::future<void>> v;
            for(uint16_t i = 0; i < concurrency_; i ++)
                v.push_back(
                        std::async(std::launch::async, [this]{io_service_.run();})
                        );
        }

        void stop()
        {
            io_service_.stop();
        }

    private:
        void do_accept()
        {
            acceptor_.async_accept(socket_, 
                [this](boost::system::error_code ec)
                {
                    if (!ec)
                        (new Connection<Handler>(std::move(socket_), handler_, server_name_))->start();
                    do_accept();
                });
        }

    private:
        asio::io_service io_service_;
        tcp::acceptor acceptor_;
        tcp::socket socket_;
        Handler* handler_;
        uint16_t concurrency_ = 1;
        std::string server_name_ = "Flask++/0.1";
    };
}
