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
        Server(Handler* handler, uint16_t port)
            : acceptor_(io_service_, tcp::endpoint(asio::ip::address(), port)), socket_(io_service_), handler_(handler)
        {
            do_accept();
        }

        void run()
        {
            auto _ = std::async(std::launch::async, [this]{io_service_.run();});
        }

    private:
        void do_accept()
        {
            acceptor_.async_accept(socket_, 
                [this](boost::system::error_code ec)
                {
                    if (!ec)
                        (new Connection<Handler>(std::move(socket_), handler_))->start();
                    do_accept();
                });
        }

    private:
        asio::io_service io_service_;
        tcp::acceptor acceptor_;
        tcp::socket socket_;
        Handler* handler_;
    };
}
