#pragma once
#include <boost/asio.hpp>
#include <http_parser.h>
#include "parser.h"

namespace flask
{
    using namespace boost;
    using tcp = asio::ip::tcp;
    template <typename Handler>
    class Connection
    {
    public:
        Connection(tcp::socket&& socket, Handler* handler) : socket_(std::move(socket)), handler_(handler)
        {
        }

        void start()
        {
            do_read();
        }

    private:
        void do_read()
        {
            socket_.async_read_some(boost::asio::buffer(buffer_), 
                [this](boost::system::error_code ec, std::size_t bytes_transferred)
                {
                    if (!ec)
                    {
                        parser_.feed(buffer_.data(), bytes_transferred);
                    }
                    else
                    {
                        parser_.done();
                    }
                });
        }

    private:
        tcp::socket socket_;
        Handler* handler_;

        std::array<char, 8192> buffer_;

        HTTPParser parser_;
    };
}
