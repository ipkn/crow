#pragma once
#include <string>
#include <unordered_map>
#include "json.h"

namespace crow
{
    template <typename T>
    class Connection;
    struct response
    {
        template <typename T> 
        friend class crow::Connection;

        std::string body;
        json::wvalue json_value;
        int code{200};
        std::unordered_map<std::string, std::string> headers;

        response() {}
        explicit response(int code) : code(code) {}
        response(std::string body) : body(std::move(body)) {}
        response(json::wvalue&& json_value) : json_value(std::move(json_value)) {}
        response(const json::wvalue& json_value) : body(json::dump(json_value)) {}
        response(int code, std::string body) : body(std::move(body)), code(code) {}

        response(response&& r)
        {
            *this = std::move(r);
        }

        response& operator = (const response& r) = delete;

        response& operator = (response&& r)
        {
            body = std::move(r.body);
            json_value = std::move(r.json_value);
            code = r.code;
            headers = std::move(r.headers);
			completed_ = r.completed_;
            return *this;
        }

        void clear()
        {
            body.clear();
            json_value.clear();
            code = 200;
            headers.clear();
            completed_ = false;
        }

        void write(const std::string& body_part)
        {
            body += body_part;
        }

        void end()
        {
            if (!completed_)
            {
                completed_ = true;
                if (complete_request_handler_)
                {
                    complete_request_handler_();
                }
            }
        }

        void end(const std::string& body_part)
        {
            body += body_part;
            end();
        }

        bool is_alive()
        {
            return is_alive_helper_ && is_alive_helper_();
        }

        private:
            bool completed_{};
            std::function<void()> complete_request_handler_;
            std::function<bool()> is_alive_helper_;
    };
}
