#include <iostream>
#include <string>
namespace flask
{
    struct HTTPParser : public http_parser
    {
        static int on_message_begin(http_parser* self_)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            return 0;
        }
        static int on_url(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            std::cout << std::string(at, at+length) << std::endl;
            return 0;
        }
        static int on_status(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            std::cout << std::string(at, at+length) << std::endl;
            return 0;
        }
        static int on_header_field(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            std::cout << std::string(at, at+length) << std::endl;
            return 0;
        }
        static int on_header_value(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            std::cout << std::string(at, at+length) << std::endl;
            return 0;
        }
        static int on_headers_complete(http_parser* self_)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            return 0;
        }
        static int on_body(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            std::cout << std::string(at, at+length) << std::endl;
            return 0;
        }
        static int on_message_complete(http_parser* self_)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            return 0;
        }
        HTTPParser() :
            settings_ {
                on_message_begin,
                on_url,
                on_status,
                on_header_field,
                on_header_value,
                on_headers_complete,
                on_body,
                on_message_complete,
            }
        {
            http_parser_init(this, HTTP_REQUEST);
        }

        void feed(const char* buffer, int length)
        {
            int nparsed = http_parser_execute(this, &settings_, buffer, length);
        }

        void done()
        {
            int nparsed = http_parser_execute(this, &settings_, nullptr, 0);
        }

        http_parser_settings settings_;
    };
}
