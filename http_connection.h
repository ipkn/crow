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
#include "datetime.h"

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
            static std::unordered_map<int, std::string> statusCodes = {
                {200, "HTTP/1.1 200 OK\r\n"},
                {201, "HTTP/1.1 201 Created\r\n"},
                {202, "HTTP/1.1 202 Accepted\r\n"},
                {204, "HTTP/1.1 204 No Content\r\n"},

                {300, "HTTP/1.1 300 Multiple Choices\r\n"},
                {301, "HTTP/1.1 301 Moved Permanently\r\n"},
                {302, "HTTP/1.1 302 Moved Temporarily\r\n"},
                {304, "HTTP/1.1 304 Not Modified\r\n"},

                {400, "HTTP/1.1 400 Bad Request\r\n"},
                {401, "HTTP/1.1 401 Unauthorized\r\n"},
                {403, "HTTP/1.1 403 Forbidden\r\n"},
                {404, "HTTP/1.1 404 Not Found\r\n"},

                {500, "HTTP/1.1 500 Internal Server Error\r\n"},
                {501, "HTTP/1.1 501 Not Implemented\r\n"},
                {502, "HTTP/1.1 502 Bad Gateway\r\n"},
                {503, "HTTP/1.1 503 Service Unavailable\r\n"},
            };

            request req = parser_.to_request();
            if (parser_.http_major == 1 && parser_.http_minor == 0)
            {
                // HTTP/1.0
                if (!(req.headers.count("connection") && boost::iequals(req.headers["connection"],"Keep-Alive")))
                    close_connection_ = true;
            }
            else
            {
                // HTTP/1.1
                if (req.headers.count("connection") && req.headers["connection"] == "close")
                    close_connection_ = true;
            }

            res = handler_->handle(req);

#ifdef FLASK_ENABLE_LOGGING
            std::cerr << "HTTP/" << parser_.http_major << "." << parser_.http_minor << ' ';
            std::cerr << method_name(req.method);
            std::cerr << " " << res.code << std::endl;
#endif

            static std::string seperator = ": ";
            static std::string crlf = "\r\n";

            buffers_.clear();
            buffers_.reserve(4*(res.headers.size()+4)+3);

            if (res.body.empty() && res.json_value.t() == json::type::Object)
            {
                res.body = json::dump(res.json_value);
            }

            if (!statusCodes.count(res.code))
                res.code = 500;
            {
                auto& status = statusCodes.find(res.code)->second;
                buffers_.emplace_back(status.data(), status.size());
            }

            if (res.code >= 400 && res.body.empty())
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
                std::string date_str = DateTime().str();
                auto ret = res.headers.emplace("Date", date_str);
                buffers_.emplace_back(ret.first->first.data(), ret.first->first.size());
                buffers_.emplace_back(seperator.data(), seperator.size());
                buffers_.emplace_back(ret.first->second.data(), ret.first->second.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
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
                            socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                            socket_.close();

                            life_--;
                            if ((int)life_ == 0)
                                delete this;
                        }
                    }
                    else
                    {
                        parser_.done();
                        socket_.close();

                        life_--;
                        if ((int)life_ == 0)
                            delete this;
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
                            delete this;
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
