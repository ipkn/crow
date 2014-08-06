#pragma once
#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <atomic>
#include <chrono>
#include <array>

#include <http_parser.h>

#include "datetime.h"
#include "parser.h"
#include "http_response.h"
#include "logging.h"
#include "settings.h"

namespace crow
{
    using namespace boost;
    using tcp = asio::ip::tcp;
#ifdef CROW_ENABLE_DEBUG
    static int connectionCount;
#endif
    template <typename Handler>
    class Connection : public std::enable_shared_from_this<Connection<Handler>>
    {
    public:
        Connection(tcp::socket&& socket, Handler* handler, const std::string& server_name) 
            : socket_(std::move(socket)), 
            handler_(handler), 
            parser_(this), 
            server_name_(server_name),
            deadline_(socket_.get_io_service())
        {
#ifdef CROW_ENABLE_DEBUG
            connectionCount ++;
            CROW_LOG_DEBUG << "Connection open, total " << connectionCount << ", " << this;
#endif
        }
        
        ~Connection()
        {
            res.complete_request_handler_ = nullptr;
#ifdef CROW_ENABLE_DEBUG
            connectionCount --;
            CROW_LOG_DEBUG << "Connection closed, total " << connectionCount << ", " << this;
#endif
        }

        void start()
        {
            auto self = this->shared_from_this();
            start_deadline();

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

            CROW_LOG_INFO << "Request: "<< this << " HTTP/" << parser_.http_major << "." << parser_.http_minor << ' '
             << method_name(req.method) << " " << req.url;


            if (!is_invalid_request)
            {
                deadline_.cancel();
                auto self = this->shared_from_this();
                res.complete_request_handler_ = [self]{ self->complete_request(); };
                res.is_alive_helper_ = [this]()->bool{ return socket_.is_open(); };
                handler_->handle(req, res);
            }
			else
			{
				complete_request();
			}
        }

        void complete_request()
        {
            CROW_LOG_INFO << "Response: " << this << ' ' << res.code << ' ' << close_connection_;

			if (!socket_.is_open())
				return;

            auto self = this->shared_from_this();
            res.complete_request_handler_ = nullptr;

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
                content_length_ = std::to_string(res.body.size());
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
            res.clear();
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
            auto self = this->shared_from_this();
            socket_.async_read_some(boost::asio::buffer(buffer_), 
                [self, this](const boost::system::error_code& ec, std::size_t bytes_transferred)
                {
                    bool error_while_reading = true;
                    if (!ec)
                    {
                        bool ret = parser_.feed(buffer_.data(), bytes_transferred);
                        if (ret)
                        {
                            do_read();
                            error_while_reading = false;
                        }
                    }

                    if (error_while_reading)
                    {
                        deadline_.cancel();
                        parser_.done();
                        socket_.close();
                    }
                    else
                    {
                        start_deadline();
                    }
                });
        }

        void do_write()
        {
            auto self = this->shared_from_this();
            boost::asio::async_write(socket_, buffers_, 
                [&, self](const boost::system::error_code& ec, std::size_t bytes_transferred)
                {
                    if (!ec)
                    {
                        start_deadline();
                        if (close_connection_)
                        {
                            socket_.close();
                        }
                    }
                });
        }

        void start_deadline(int timeout = 5)
        {
            deadline_.expires_from_now(boost::posix_time::seconds(timeout));
            auto self = this->shared_from_this();
            deadline_.async_wait([self, this](const boost::system::error_code& ec) 
            {
                if (ec || !socket_.is_open())
                {
                    return;
                }
                bool is_deadline_passed = deadline_.expires_at() <= boost::asio::deadline_timer::traits_type::now();
                if (is_deadline_passed)
                {
                    socket_.close();
                }
            });
        }

    private:
        tcp::socket socket_;
        Handler* handler_;

        std::array<char, 8192> buffer_;

        HTTPParser<Connection> parser_;
        response res;

        bool close_connection_ = false;

        const std::string& server_name_;
        std::vector<boost::asio::const_buffer> buffers_;

        std::string content_length_;
        std::string date_str_;

        boost::asio::deadline_timer deadline_;
    };

}
