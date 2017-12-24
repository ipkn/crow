#pragma once
#include <boost/algorithm/string/predicate.hpp>
#include <boost/array.hpp>
#include "crow/socket_adaptors.h"
#include "crow/http_request.h"
#include "crow/TinySHA1.hpp"

namespace crow
{
    namespace websocket
    {
        enum class WebSocketReadState
        {
            MiniHeader,
            Len16,
            Len64,
            Mask,
            Payload,
        };

		struct connection
		{
            virtual void send_binary(const std::string& msg) = 0;
            virtual void send_text(const std::string& msg) = 0;
            virtual void close(const std::string& msg = "quit") = 0;
            virtual ~connection(){}

            void userdata(void* u) { userdata_ = u; }
            void* userdata() { return userdata_; }

        private:
            void* userdata_;
		};

		template <typename Adaptor>
        class Connection : public connection
        {
			public:
				Connection(const crow::request& req, Adaptor&& adaptor, 
						std::function<void(crow::websocket::connection&)> open_handler,
						std::function<void(crow::websocket::connection&, const std::string&, bool)> message_handler,
						std::function<void(crow::websocket::connection&, const std::string&)> close_handler,
						std::function<void(crow::websocket::connection&)> error_handler,
						std::function<bool(const crow::request&)> accept_handler)
					: adaptor_(std::move(adaptor)), open_handler_(std::move(open_handler)), message_handler_(std::move(message_handler)), close_handler_(std::move(close_handler)), error_handler_(std::move(error_handler))
					, accept_handler_(std::move(accept_handler))
				{
					if (!boost::iequals(req.get_header_value("upgrade"), "websocket"))
					{
						adaptor.close();
						delete this;
						return;
					}

					if (accept_handler_)
					{
						if (!accept_handler_(req))
						{
							adaptor.close();
							delete this;
							return;
						}
					}

					// Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
					// Sec-WebSocket-Version: 13
                    std::string magic = req.get_header_value("Sec-WebSocket-Key") +  "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
                    sha1::SHA1 s;
                    s.processBytes(magic.data(), magic.size());
                    uint8_t digest[20];
                    s.getDigestBytes(digest);   
                    start(crow::utility::base64encode((char*)digest, 20));
				}

                template<typename CompletionHandler>
                void dispatch(CompletionHandler handler)
                {
                    adaptor_.get_io_service().dispatch(handler);
                }

                template<typename CompletionHandler>
                void post(CompletionHandler handler)
                {
                    adaptor_.get_io_service().post(handler);
                }

                void send_pong(const std::string& msg)
                {
                    dispatch([this, msg]{
                        char buf[3] = "\x8A\x00";
                        buf[1] += msg.size();
                        write_buffers_.emplace_back(buf, buf+2);
                        write_buffers_.emplace_back(msg);
                        do_write();
                    });
                }

                void send_binary(const std::string& msg) override
                {
                    dispatch([this, msg]{
                        auto header = build_header(2, msg.size());
                        write_buffers_.emplace_back(std::move(header));
                        write_buffers_.emplace_back(msg);
                        do_write();
                    });
                }

                void send_text(const std::string& msg) override
                {
                    dispatch([this, msg]{
                        auto header = build_header(1, msg.size());
                        write_buffers_.emplace_back(std::move(header));
                        write_buffers_.emplace_back(msg);
                        do_write();
                    });
                }

                void close(const std::string& msg) override
                {
                    dispatch([this, msg]{
                        has_sent_close_ = true;
                        if (has_recv_close_ && !is_close_handler_called_)
                        {
                            is_close_handler_called_ = true;
                            if (close_handler_)
                                close_handler_(*this, msg);
                        }
                        auto header = build_header(0x8, msg.size());
                        write_buffers_.emplace_back(std::move(header));
                        write_buffers_.emplace_back(msg);
                        do_write();
                    });
                }

            protected:

                std::string build_header(int opcode, size_t size)
                {
                    char buf[2+8] = "\x80\x00";
                    buf[0] += opcode;
                    if (size < 126)
                    {
                        buf[1] += size;
                        return {buf, buf+2};
                    }
                    else if (size < 0x10000)
                    {
                        buf[1] += 126;
                        *(uint16_t*)(buf+2) = htons((uint16_t)size);
                        return {buf, buf+4};
                    }
                    else
                    {
                        buf[1] += 127;
                        *reinterpret_cast<uint64_t*>(buf+2) = ((1==htonl(1)) ? static_cast<uint64_t>(size) : (static_cast<uint64_t>(htonl((size) & 0xFFFFFFFF)) << 32) | htonl(static_cast<uint64_t>(size) >> 32));
                        return {buf, buf+10};
                    }
                }

                void start(std::string&& hello)
                {
                    static std::string header = "HTTP/1.1 101 Switching Protocols\r\n"
                        "Upgrade: websocket\r\n"
                        "Connection: Upgrade\r\n"
                        "Sec-WebSocket-Accept: ";
                    static std::string crlf = "\r\n";
                    write_buffers_.emplace_back(header);
                    write_buffers_.emplace_back(std::move(hello));
                    write_buffers_.emplace_back(crlf);
                    write_buffers_.emplace_back(crlf);
                    do_write();
                    if (open_handler_)
                        open_handler_(*this);
                    do_read();
                }

                void do_read()
                {
                    is_reading = true;
                    switch(state_)
                    {
                        case WebSocketReadState::MiniHeader:
                            {
                                //boost::asio::async_read(adaptor_.socket(), boost::asio::buffer(&mini_header_, 1), 
                                adaptor_.socket().async_read_some(boost::asio::buffer(&mini_header_, 2), 
                                    [this](const boost::system::error_code& ec, std::size_t 
#ifdef CROW_ENABLE_DEBUG
                                        bytes_transferred
#endif
                                        )

                                    {
                                        is_reading = false;
                                        mini_header_ = ntohs(mini_header_);
#ifdef CROW_ENABLE_DEBUG
                                        
                                        if (!ec && bytes_transferred != 2)
                                        {
                                            throw std::runtime_error("WebSocket:MiniHeader:async_read fail:asio bug?");
                                        }
#endif

                                        if (!ec && ((mini_header_ & 0x80) == 0x80))
                                        {
                                            if ((mini_header_ & 0x7f) == 127)
                                            {
                                                state_ = WebSocketReadState::Len64;
                                            }
                                            else if ((mini_header_ & 0x7f) == 126)
                                            {
                                                state_ = WebSocketReadState::Len16;
                                            }
                                            else
                                            {
                                                remaining_length_ = mini_header_ & 0x7f;
                                                state_ = WebSocketReadState::Mask;
                                            }
                                            do_read();
                                        }
                                        else
                                        {
                                            close_connection_ = true;
                                            adaptor_.close();
                                            if (error_handler_)
                                                error_handler_(*this);
                                            check_destroy();
                                        }
                                    });
                            }
                            break;
                        case WebSocketReadState::Len16:
                            {
                                remaining_length_ = 0;
                                remaining_length16_ = 0;
                                boost::asio::async_read(adaptor_.socket(), boost::asio::buffer(&remaining_length16_, 2), 
                                    [this](const boost::system::error_code& ec, std::size_t 
#ifdef CROW_ENABLE_DEBUG
                                        bytes_transferred
#endif
                                        ) 
                                    {
                                        is_reading = false;
                                        remaining_length16_ = ntohs(remaining_length16_);
                                        remaining_length_ = remaining_length16_;
#ifdef CROW_ENABLE_DEBUG
                                        if (!ec && bytes_transferred != 2)
                                        {
                                            throw std::runtime_error("WebSocket:Len16:async_read fail:asio bug?");
                                        }
#endif

                                        if (!ec)
                                        {
                                            state_ = WebSocketReadState::Mask;
                                            do_read();
                                        }
                                        else
                                        {
                                            close_connection_ = true;
                                            adaptor_.close();
                                            if (error_handler_)
                                                error_handler_(*this);
                                            check_destroy();
                                        }
                                    });
                            }
                            break;
                        case WebSocketReadState::Len64:
                            {
                                boost::asio::async_read(adaptor_.socket(), boost::asio::buffer(&remaining_length_, 8), 
                                    [this](const boost::system::error_code& ec, std::size_t 
#ifdef CROW_ENABLE_DEBUG
                                        bytes_transferred
#endif
                                        ) 
                                    {
                                        is_reading = false;
                                        remaining_length_ = ((1==ntohl(1)) ? (remaining_length_) : ((uint64_t)ntohl((remaining_length_) & 0xFFFFFFFF) << 32) | ntohl((remaining_length_) >> 32));
#ifdef CROW_ENABLE_DEBUG
                                        if (!ec && bytes_transferred != 8)
                                        {
                                            throw std::runtime_error("WebSocket:Len16:async_read fail:asio bug?");
                                        }
#endif

                                        if (!ec)
                                        {
                                            state_ = WebSocketReadState::Mask;
                                            do_read();
                                        }
                                        else
                                        {
                                            close_connection_ = true;
                                            adaptor_.close();
                                            if (error_handler_)
                                                error_handler_(*this);
                                            check_destroy();
                                        }
                                    });
                            }
                            break;
                        case WebSocketReadState::Mask:
                                boost::asio::async_read(adaptor_.socket(), boost::asio::buffer((char*)&mask_, 4), 
                                    [this](const boost::system::error_code& ec, std::size_t 
#ifdef CROW_ENABLE_DEBUG 
                                        bytes_transferred
#endif
                                    )
                                    {
                                        is_reading = false;
#ifdef CROW_ENABLE_DEBUG
                                        if (!ec && bytes_transferred != 4)
                                        {
                                            throw std::runtime_error("WebSocket:Mask:async_read fail:asio bug?");
                                        }
#endif

                                        if (!ec)
                                        {
                                            state_ = WebSocketReadState::Payload;
                                            do_read();
                                        }
                                        else
                                        {
                                            close_connection_ = true;
                                            if (error_handler_)
                                                error_handler_(*this);
                                            adaptor_.close();
                                        }
                                    });
                            break;
                        case WebSocketReadState::Payload:
                            {
                                size_t to_read = buffer_.size();
                                if (remaining_length_ < to_read)
                                    to_read = remaining_length_;
                                adaptor_.socket().async_read_some( boost::asio::buffer(buffer_, to_read), 
                                    [this](const boost::system::error_code& ec, std::size_t bytes_transferred)
                                    {
                                        is_reading = false;

                                        if (!ec)
                                        {
                                            fragment_.insert(fragment_.end(), buffer_.begin(), buffer_.begin() + bytes_transferred);
                                            remaining_length_ -= bytes_transferred;
                                            if (remaining_length_ == 0)
                                            {
                                                handle_fragment();
                                                state_ = WebSocketReadState::MiniHeader;
                                                do_read();
                                            }
                                        }
                                        else
                                        {
                                            close_connection_ = true;
                                            if (error_handler_)
                                                error_handler_(*this);
                                            adaptor_.close();
                                        }
                                    });
                            }
                            break;
                    }
                }

                bool is_FIN()
                {
                    return mini_header_ & 0x8000;
                }

                int opcode()
                {
                    return (mini_header_ & 0x0f00) >> 8;
                }

                void handle_fragment()
                {
                    for(decltype(fragment_.length()) i = 0; i < fragment_.length(); i ++)
                    {
                        fragment_[i] ^= ((char*)&mask_)[i%4];
                    }
                    switch(opcode())
                    {
                        case 0: // Continuation
                            {
                                message_ += fragment_;
                                if (is_FIN())
                                {
                                    if (message_handler_)
                                        message_handler_(*this, message_, is_binary_);
                                    message_.clear();
                                }
                            }
                        case 1: // Text
                            {
                                is_binary_ = false;
                                message_ += fragment_;
                                if (is_FIN())
                                {
                                    if (message_handler_)
                                        message_handler_(*this, message_, is_binary_);
                                    message_.clear();
                                }
                            }
                            break;
                        case 2: // Binary
                            {
                                is_binary_ = true;
                                message_ += fragment_;
                                if (is_FIN())
                                {
                                    if (message_handler_)
                                        message_handler_(*this, message_, is_binary_);
                                    message_.clear();
                                }
                            }
                            break;
                        case 0x8: // Close
                            {
                                has_recv_close_ = true;
                                if (!has_sent_close_)
                                {
                                    close(fragment_);
                                }
                                else
                                {
                                    adaptor_.close();
                                    close_connection_ = true;
                                    if (!is_close_handler_called_)
                                    {
                                        if (close_handler_)
                                            close_handler_(*this, fragment_);
                                        is_close_handler_called_ = true;
                                    }
                                    check_destroy();
                                }
                            }
                            break;
                        case 0x9: // Ping
                            {
                                send_pong(fragment_);
                            }
                            break;
                        case 0xA: // Pong
                            {
                                pong_received_ = true;
                            }
                            break;
                    }

                    fragment_.clear();
                }

                void do_write()
                {
                    if (sending_buffers_.empty())
                    {
                        sending_buffers_.swap(write_buffers_);
                        std::vector<boost::asio::const_buffer> buffers;
                        buffers.reserve(sending_buffers_.size());
                        for(auto& s:sending_buffers_)
                        {
                            buffers.emplace_back(boost::asio::buffer(s));
                        }
                        boost::asio::async_write(adaptor_.socket(), buffers, 
                            [&](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
                            {
                                sending_buffers_.clear();
                                if (!ec && !close_connection_)
                                {
                                    if (!write_buffers_.empty())
                                        do_write();
                                    if (has_sent_close_)
                                        close_connection_ = true;
                                }
                                else
                                {
                                    close_connection_ = true;
                                    check_destroy();
                                }
                            });
                    }
                }

                void check_destroy()
                {
                    //if (has_sent_close_ && has_recv_close_)
                    if (!is_close_handler_called_)
                        if (close_handler_)
                            close_handler_(*this, "uncleanly");
                    if (sending_buffers_.empty() && !is_reading)
                        delete this;
                }
			private:
				Adaptor adaptor_;

                std::vector<std::string> sending_buffers_;
                std::vector<std::string> write_buffers_;

                boost::array<char, 4096> buffer_;
                bool is_binary_;
                std::string message_;
                std::string fragment_;
                WebSocketReadState state_{WebSocketReadState::MiniHeader};
                uint16_t remaining_length16_{0};
                uint64_t remaining_length_{0};
                bool close_connection_{false};
                bool is_reading{false};
                uint32_t mask_;
                uint16_t mini_header_;
                bool has_sent_close_{false};
                bool has_recv_close_{false};
                bool error_occured_{false};
                bool pong_received_{false};
                bool is_close_handler_called_{false};

				std::function<void(crow::websocket::connection&)> open_handler_;
				std::function<void(crow::websocket::connection&, const std::string&, bool)> message_handler_;
				std::function<void(crow::websocket::connection&, const std::string&)> close_handler_;
				std::function<void(crow::websocket::connection&)> error_handler_;
				std::function<bool(const crow::request&)> accept_handler_;
        };
    }
}
