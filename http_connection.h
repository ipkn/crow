#pragma once
#include <boost/asio.hpp>
#include <http_parser.h>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/array.hpp>
#include <boost/lexical_cast.hpp>

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
        Connection(tcp::socket&& socket, Handler* handler, const std::string& server_name) : socket_(std::move(socket)), handler_(handler), parser_(this), server_name_(server_name)
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

            buffers_.clear();
            buffers_.reserve(4*(res.headers.size()+4)+3);

            if (!statusCodes.count(res.code))
                res.code = 500;
            {
                auto& status = statusCodes.find(res.code)->second;
                buffers_.emplace_back(status.data(), status.size());
            }

            if (res.code > 400 && res.body.empty())
                res.body = statusCodes[res.code].substr(9);

            bool has_content_length = false;
            bool has_date = false;
            bool has_server = false;

            for(auto& kv : res.headers)
            {
                buffers_.emplace_back(kv.first.data(), kv.first.size());
                buffers_.emplace_back(seperator.data(), seperator.size());
                buffers_.emplace_back(kv.second.data(), kv.second.size());
                buffers_.emplace_back(crlf.data(), crlf.size());

                if (boost::iequals(kv.first, "content-length"))
                    has_content_length = true;
                if (boost::iequals(kv.first, "date"))
                    has_date = true;
                if (boost::iequals(kv.first, "server"))
                    has_server = true;
            }

            if (!has_content_length)
            {
                auto ret = res.headers.emplace("Content-Length", boost::lexical_cast<std::string>(res.body.size()));
                buffers_.emplace_back(ret.first->first.data(), ret.first->first.size());
                buffers_.emplace_back(seperator.data(), seperator.size());
                buffers_.emplace_back(ret.first->second.data(), ret.first->second.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
            }
            if (!has_server)
            {
                auto ret = res.headers.emplace("Server", server_name_);
                buffers_.emplace_back(ret.first->first.data(), ret.first->first.size());
                buffers_.emplace_back(seperator.data(), seperator.size());
                buffers_.emplace_back(ret.first->second.data(), ret.first->second.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
            }
            if (!has_date)
            {
            }

            buffers_.emplace_back(crlf.data(), crlf.size());
            buffers_.emplace_back(res.body.data(), res.body.size());

            do_write();
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

        void do_write()
        {
            life_++;
            boost::asio::async_write(socket_, buffers_, 
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

        const std::string& server_name_;
        std::vector<boost::asio::const_buffer> buffers_;
    };
}
