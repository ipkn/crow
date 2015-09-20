#pragma once
#include <boost/asio.hpp>
#include "settings.h"
namespace crow
{
    using namespace boost;
    using tcp = asio::ip::tcp;

    struct SocketAdaptor
    {
        using context = void;
        SocketAdaptor(boost::asio::io_service& io_service, context*)
            : socket_(io_service)
        {
        }

        tcp::socket& raw_socket()
        {
            return socket_;
        }

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
            socket_.close();
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
        SSLAdaptor(boost::asio::io_service& io_service, context* ctx)
            : ssl_socket_(io_service, *ctx)
        {
        }

        boost::asio::ssl::stream<tcp::socket>& socket()
        {
            return ssl_socket_;
        }

        tcp::socket::lowest_layer_type&
        raw_socket()
        {
            return ssl_socket_.lowest_layer();
        }

        tcp::endpoint remote_endpoint()
        {
            return raw_socket().remote_endpoint();
        }

        bool is_open()
        {
            return raw_socket().is_open();
        }

        void close()
        {
            raw_socket().close();
        }

        template <typename F> 
        void start(F f)
        {
            ssl_socket_.async_handshake(boost::asio::ssl::stream_base::server,
                    [f](const boost::system::error_code& ec) {
                        f(ec);
                    });
        }

        boost::asio::ssl::stream<tcp::socket> ssl_socket_;
    };
#endif
}
