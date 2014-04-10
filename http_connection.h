#pragma once
#include <boost/asio.hpp>
#include <http_parser.h>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/array.hpp>

#include "parser.h"
#include "http_response.h"

namespace flask
{
    using namespace boost;
    using tcp = asio::ip::tcp;
    template <typename Handler>
    class Connection
    {
    public:
        Connection(tcp::socket&& socket, Handler* handler) : socket_(std::move(socket)), handler_(handler), parser_(this)
        {
        }

        void start()
        {
            do_read();
        }

        void handle()
        {
            request req = parser_.to_request();
            res = handler_->handle(req);

            static std::string seperator = ": ";
            static std::string crlf = "\r\n";

            std::vector<boost::asio::const_buffer> buffers;

            buffers.push_back(boost::asio::buffer(statusCodes[res.status]));

            bool has_content_length = false;
            for(auto& kv : res.headers)
            {
                buffers.push_back(boost::asio::buffer(kv.first));
                buffers.push_back(boost::asio::buffer(seperator));
                buffers.push_back(boost::asio::buffer(kv.second));
                buffers.push_back(boost::asio::buffer(crlf));

                if (boost::iequals(kv.first, "content-length"))
                    has_content_length = true;
            }

            if (!has_content_length)
                close_connection_ = true;

            buffers.push_back(boost::asio::buffer(crlf));
            buffers.push_back(boost::asio::buffer(res.body));

            do_write(buffers);
        }

    private:
        void do_read()
        {
            life_++;
            socket_.async_read_some(boost::asio::buffer(buffer_), 
                [this](boost::system::error_code ec, std::size_t bytes_transferred)
                {
                    if (!ec)
                    {
                        bool ret = parser_.feed(buffer_.data(), bytes_transferred);
						if (ret)
							do_read();
						else
						{
							socket_.close();

							life_--;
							if ((int)life_ == 0)
							{
								delete this;
							}
						}
                    }
                    else
                    {
                        parser_.done();
                        socket_.close();

                        life_--;
                        if ((int)life_ == 0)
                        {
                            delete this;
                        }
                    }
                });
        }

        void do_write(const std::vector<boost::asio::const_buffer>& buffers)
        {
            life_++;
            boost::asio::async_write(socket_, buffers, 
                [this](const boost::system::error_code& ec, std::size_t bytes_transferred)
                {
                    bool should_close = false;
                    if (!ec)
                    {
                        if (close_connection_)
                        {
                            should_close = true;
                        }
                    }
                    else
                    {
                        should_close = true;
                    }
                    if (should_close)
                    {
                        socket_.close();
                        life_--;
                        if ((int)life_ == 0)
                        {
                            delete this;
                        }
                    }
                });
        }

    private:
        tcp::socket socket_;
        Handler* handler_;

        boost::array<char, 8192> buffer_;

        HTTPParser<Connection> parser_;
        response res;

        std::atomic<int> life_;
        bool close_connection_ = false;
    };
}
