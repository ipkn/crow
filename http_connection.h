#pragma once
#include <boost/asio.hpp>
#include <http_parser.h>
#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/array.hpp>
#include <boost/lexical_cast.hpp>
#include <atomic>
#include <chrono>

#include "datetime.h"
#include "parser.h"
#include "http_response.h"
#include "logging.h"

namespace crow
{
    using namespace boost;
    using tcp = asio::ip::tcp;
    template <typename Handler>
    class Connection
    {
    public:
        Connection(tcp::socket&& socket, Handler* handler, const std::string& server_name) 
            : socket_(std::move(socket)), 
            handler_(handler), 
            parser_(this), 
            server_name_(server_name) 
        {
        }

        void start()
        {
            do_read();
        }

        void handle_header()
        {
            // HTTP 1.1 Expect: 100-continue
            if (parser_.check_version(1, 1) && parser_.headers.count("expect") && parser_.headers["expect"] == "100-continue")
            {
                buffers_.clear();
                static std::string expect_100_continue = "HTTP/1.1 100 Continue\r\n\r\n";
                buffers_.emplace_back(expect_100_continue.data(), expect_100_continue.size());
                do_write();
            }
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

            bool is_invalid_request = false;

            request req = parser_.to_request();
            if (parser_.check_version(1, 0))
            {
                // HTTP/1.0
                if (!(req.headers.count("connection") && boost::iequals(req.headers["connection"],"Keep-Alive")))
                    close_connection_ = true;
            }
            else if (parser_.check_version(1, 1))
            {
                // HTTP/1.1
                if (req.headers.count("connection") && req.headers["connection"] == "close")
                    close_connection_ = true;
                if (!req.headers.count("host"))
                {
                    is_invalid_request = true;
                    res = response(400);
                }
            }

            if (!is_invalid_request)
            {
                res = handler_->handle(req);
            }

            CROW_LOG_INFO << "HTTP/" << parser_.http_major << "." << parser_.http_minor << ' '
             << method_name(req.method) << " " << req.url
             << " " << res.code << ' ' << close_connection_;

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
                content_length_ = boost::lexical_cast<std::string>(res.body.size());
                static std::string content_length_tag = "Content-Length: ";
                buffers_.emplace_back(content_length_tag.data(), content_length_tag.size());
                buffers_.emplace_back(content_length_.data(), content_length_.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
            }
            if (!has_server)
            {
                static std::string server_tag = "Server: ";
                buffers_.emplace_back(server_tag.data(), server_tag.size());
                buffers_.emplace_back(server_name_.data(), server_name_.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
            }
            if (!has_date)
            {
                static std::string date_tag = "Date: ";
                date_str_ = get_cached_date_str();
                buffers_.emplace_back(date_tag.data(), date_tag.size());
                buffers_.emplace_back(date_str_.data(), date_str_.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
            }

            buffers_.emplace_back(crlf.data(), crlf.size());
            buffers_.emplace_back(res.body.data(), res.body.size());

            do_write();
        }

    private:
        static std::string get_cached_date_str()
        {
            using namespace std::chrono;
            thread_local auto last = steady_clock::now();
            thread_local std::string date_str = DateTime().str();

            if (steady_clock::now() - last >= seconds(1))
            {
                last = steady_clock::now();
                date_str = DateTime().str();
            }
            return date_str;
        }

        void do_read()
        {
            life_++;
            socket_.async_read_some(boost::asio::buffer(buffer_), 
                [this](boost::system::error_code ec, std::size_t bytes_transferred)
                {
                    bool do_complete_task = false;
                    if (!ec)
                    {
                        bool ret = parser_.feed(buffer_.data(), bytes_transferred);
                        if (ret)
                            do_read();
                        else
                            do_complete_task = true;
                    }
                    else
                        do_complete_task = true;
                    if (do_complete_task)
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

        int life_ {};
        bool close_connection_ = false;

        const std::string& server_name_;
        std::vector<boost::asio::const_buffer> buffers_;

        std::string content_length_;
        std::string date_str_;
    };
}
