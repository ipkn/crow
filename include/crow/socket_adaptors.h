#pragma once
#include <boost/asio.hpp>
#ifdef CROW_ENABLE_SSL
#include <boost/asio/ssl.hpp>
#endif
#include "crow/settings.h"
#if BOOST_VERSION >= 107000
#define GET_IO_SERVICE(s) ((boost::asio::io_context&)(s).get_executor().context())
#else
#define GET_IO_SERVICE(s) ((s).get_io_service())
#endif
namespace crow
{
    using namespace boost;
    using tcp = asio::ip::tcp;

    ///A wrapper for the asio::ip::tcp::socket and asio::ssl::stream
    struct SocketAdaptor
    {
        using context = void;
        SocketAdaptor(boost::asio::io_service& io_service, context*)
            : socket_(io_service)
        {
        }

        boost::asio::io_service& get_io_service()
        {
            return GET_IO_SERVICE(socket_);
        }

        /// Get the TCP socket handling data trasfers, regardless of what layer is handling transfers on top of the socket.
        tcp::socket& raw_socket()
        {
            return socket_;
        }

        /// Get the object handling data transfers, this can be either a TCP socket or an SSL stream (if SSL is enabled).
        tcp::socket& socket()
        {
            return socket_;
        }

        tcp::endpoint remote_endpoint()
        {
            return socket_.remote_endpoint();
        }

        bool is_open()
        {
            return socket_.is_open();
        }

        void close()
        {
            boost::system::error_code ec;
            socket_.close(ec);
        }

        void shutdown_readwrite()
        {
            boost::system::error_code ec;
            socket_.shutdown(boost::asio::socket_base::shutdown_type::shutdown_both, ec);
        }

        void shutdown_write()
        {
            boost::system::error_code ec;
            socket_.shutdown(boost::asio::socket_base::shutdown_type::shutdown_send, ec);
        }

        void shutdown_read()
        {
            boost::system::error_code ec;
            socket_.shutdown(boost::asio::socket_base::shutdown_type::shutdown_receive, ec);
        }

        template <typename F> 
        void start(F f)
        {
            f(boost::system::error_code());
        }

        tcp::socket socket_;
    };

#ifdef CROW_ENABLE_SSL
    struct SSLAdaptor
    {
        using context = boost::asio::ssl::context;
        using ssl_socket_t = boost::asio::ssl::stream<tcp::socket>;
        SSLAdaptor(boost::asio::io_service& io_service, context* ctx)
            : ssl_socket_(new ssl_socket_t(io_service, *ctx))
        {
        }

        boost::asio::ssl::stream<tcp::socket>& socket()
        {
            return *ssl_socket_;
        }

        tcp::socket::lowest_layer_type&
        raw_socket()
        {
            return ssl_socket_->lowest_layer();
        }

        tcp::endpoint remote_endpoint()
        {
            return raw_socket().remote_endpoint();
        }

        bool is_open()
        {
            return ssl_socket_ ? raw_socket().is_open() : false;
        }

        void close()
        {
            if (is_open())
            {
                boost::system::error_code ec;
                raw_socket().close(ec);
            }
        }

        void shutdown_readwrite()
        {
            if (is_open())
            {
                boost::system::error_code ec;
                raw_socket().shutdown(boost::asio::socket_base::shutdown_type::shutdown_both, ec);
            }
        }

        void shutdown_write()
        {
            if (is_open())
            {
                boost::system::error_code ec;
                raw_socket().shutdown(boost::asio::socket_base::shutdown_type::shutdown_send, ec);
            }
        }

        void shutdown_read()
        {
            if (is_open())
            {
                boost::system::error_code ec;
                raw_socket().shutdown(boost::asio::socket_base::shutdown_type::shutdown_receive, ec);
            }
        }

        boost::asio::io_service& get_io_service()
        {
            return GET_IO_SERVICE(raw_socket());
        }

        template <typename F> 
        void start(F f)
        {
            ssl_socket_->async_handshake(boost::asio::ssl::stream_base::server,
                    [f](const boost::system::error_code& ec) {
                        f(ec);
                    });
        }

        std::unique_ptr<boost::asio::ssl::stream<tcp::socket>> ssl_socket_;
    };
#endif
}
