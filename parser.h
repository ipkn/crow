#pragma once

#include <string>
#include <unordered_map>
#include <boost/algorithm/string.hpp>

#include "http_request.h"

namespace crow
{
    template <typename Handler>
    struct HTTPParser : public http_parser
    {
        static int on_message_begin(http_parser* self_)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            self->clear();
            return 0;
        }
        static int on_url(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            self->url.insert(self->url.end(), at, at+length);
            return 0;
        }
        static int on_header_field(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            switch (self->header_building_state)
            {
                case 0:
                    if (!self->header_value.empty())
                    {
                        boost::algorithm::to_lower(self->header_field);
                        self->headers.emplace(std::move(self->header_field), std::move(self->header_value));
                    }
                    self->header_field.assign(at, at+length);
                    self->header_building_state = 1;
                    break;
                case 1:
                    self->header_field.insert(self->header_field.end(), at, at+length);
                    break;
            }
            return 0;
        }
        static int on_header_value(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            switch (self->header_building_state)
            {
                case 0:
                    self->header_value.insert(self->header_value.end(), at, at+length);
                    break;
                case 1:
                    self->header_building_state = 0;
                    self->header_value.assign(at, at+length);
                    break;
            }
            return 0;
        }
        static int on_headers_complete(http_parser* self_)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            if (!self->header_field.empty())
            {
                boost::algorithm::to_lower(self->header_field);
                self->headers.emplace(std::move(self->header_field), std::move(self->header_value));
            }
            return 0;
        }
        static int on_body(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            self->body.insert(self->body.end(), at, at+length);
            return 0;
        }
        static int on_message_complete(http_parser* self_)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            self->process_message();
            return 0;
        }
        HTTPParser(Handler* handler) :
            settings_ {
                on_message_begin,
                on_url,
                nullptr,
                on_header_field,
                on_header_value,
                on_headers_complete,
                on_body,
                on_message_complete,
            },
            handler_(handler)
        {
            http_parser_init(this, HTTP_REQUEST);
        }

        bool feed(const char* buffer, int length)
        {
            int nparsed = http_parser_execute(this, &settings_, buffer, length);
            return nparsed == length;
        }

        bool done()
        {
            int nparsed = http_parser_execute(this, &settings_, nullptr, 0);
            return nparsed == 0;
        }

        void clear()
        {
            url.clear();
            header_building_state = 0;
            header_field.clear();
            header_value.clear();
            headers.clear();
            body.clear();
        }

        void process_message()
        {
            handler_->handle();
        }

        request to_request()
        {
            return request{(HTTPMethod)method, std::move(url), std::move(headers), std::move(body)};
        }

        std::string url;
        int header_building_state = 0;
        std::string header_field;
        std::string header_value;
        std::unordered_map<std::string, std::string> headers;
        std::string body;

        http_parser_settings settings_;

        Handler* handler_;
    };
}
