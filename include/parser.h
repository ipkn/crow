#pragma once

#include <string>
#include <unordered_map>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <algorithm>

#include "http_request.h"

namespace crow
{
    template <typename Handler>
    struct HTTPParser : public http_parser
    {
        template<const char delimiter>
        struct tokenize_by_char
        {
            template<typename It>
            bool operator()(It& next, It end, std::string & tok)
            {
                if (next == end)
                    return false;
                const char dels = delimiter;
                const char* del = &dels;
                auto pos = std::search(next, end, del, del + 1);
                tok.assign(next, pos);
                next = pos;
                if (next != end)
                    std::advance(next, 1);
                return true;
            }

            void reset() {}
        };

        static ci_map get_url_params(std::string url)
        {
            const char url_delimiter = '&';
            const char param_delimiter = '=';
            ci_map ret;

            unsigned int qMarkPos = url.find("?");
            if(!(qMarkPos >=0 && qMarkPos != (url.length()-1))) {
                return ret;
            }
            
            auto params = url.substr(qMarkPos+1);

            // substitute ';' for '&' for recommended process of delimintation
            // (http://www.w3.org/TR/1999/REC-html401-19991224/appendix/notes.html#h-B.2.2)
            std::replace(params.begin(), params.end(), ';', url_delimiter);                

            // url tokenizer
            for (auto i : boost::tokenizer<tokenize_by_char<url_delimiter>>(params)) {
                std::string key, value; 
                auto parts = boost::tokenizer<tokenize_by_char<param_delimiter>>(i);
                int count = 0;
                for(auto p = parts.begin(); p != parts.end(); ++p, ++count) {
                    if(count == 0){
                        key = *p;
                    } else {
                        value = *p;
                    }
                }
                ret.insert(std::make_pair(key, value));
            }
            
            return ret;
        }

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

            // url params
            self->url_params = get_url_params(self->url);

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
                self->headers.emplace(std::move(self->header_field), std::move(self->header_value));
            }
            self->process_header();
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
            handler_(handler)
        {
            http_parser_init(this, HTTP_REQUEST);
        }

        // return false on error
        bool feed(const char* buffer, int length)
        {
            const static http_parser_settings settings_{
                on_message_begin,
                on_url,
                nullptr,
                on_header_field,
                on_header_value,
                on_headers_complete,
                on_body,
                on_message_complete,
            };

            int nparsed = http_parser_execute(this, &settings_, buffer, length);
            return nparsed == length;
        }

        bool done()
        {
            return feed(nullptr, 0);
        }

        void clear()
        {
            url.clear();
            header_building_state = 0;
            header_field.clear();
            header_value.clear();
            headers.clear();
            url_params.clear();
            body.clear();
        }

        void process_header()
        {
            handler_->handle_header();
        }

        void process_message()
        {
            handler_->handle();
        }

        request to_request() const
        {
            return request{(HTTPMethod)method, std::move(url), std::move(url_params), std::move(headers), std::move(body)};
        }

        bool check_version(int major, int minor) const
        {
            return http_major == major && http_minor == minor;
        }

        std::string url;

        int header_building_state = 0;
        std::string header_field;
        std::string header_value;
        ci_map headers;
        ci_map url_params;
        std::string body;

        Handler* handler_;
    };
}
