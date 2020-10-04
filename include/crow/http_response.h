#pragma once
#include <string>
#include <unordered_map>

#include "crow/json.h"
#include "crow/http_request.h"
#include "crow/ci_map.h"

#include "crow/socket_adaptors.h"
#include "crow/logging.h"
#if !defined(_WIN32)
#include <sys/stat.h>
#include <sys/sendfile.h>
#endif

namespace crow
{
    template <typename Adaptor, typename Handler, typename ... Middlewares>
    class Connection;
    struct response
    {
        template <typename Adaptor, typename Handler, typename ... Middlewares>
        friend class crow::Connection;

        int code{200};
        std::string body;
        json::wvalue json_value;

        // `headers' stores HTTP headers.
        ci_map headers;

        void set_header(std::string key, std::string value)
        {
            headers.erase(key);
            headers.emplace(std::move(key), std::move(value));
        }
        void add_header(std::string key, std::string value)
        {
            headers.emplace(std::move(key), std::move(value));
        }

        const std::string& get_header_value(const std::string& key)
        {
            return crow::get_header_value(headers, key);
        }


        response() {}
        explicit response(int code) : code(code) {}
        response(std::string body) : body(std::move(body)) {}
        response(json::wvalue&& json_value) : json_value(std::move(json_value))
        {
            json_mode();
        }
        response(int code, std::string body) : code(code), body(std::move(body)) {}
        response(const json::wvalue& json_value) : body(json::dump(json_value))
        {
            json_mode();
        }
        response(int code, const json::wvalue& json_value) : code(code), body(json::dump(json_value))
        {
            json_mode();
        }

        response(response&& r)
        {
            *this = std::move(r);
        }

        response& operator = (const response& r) = delete;

        response& operator = (response&& r) noexcept
        {
            body = std::move(r.body);
            json_value = std::move(r.json_value);
            code = r.code;
            headers = std::move(r.headers);
            completed_ = r.completed_;
            return *this;
        }

        bool is_completed() const noexcept
        {
            return completed_;
        }

        void clear()
        {
            body.clear();
            json_value.clear();
            code = 200;
            headers.clear();
            completed_ = false;
        }

        void redirect(const std::string& location)
        {
            code = 301;
            set_header("Location", location);
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
 /* adding static file support here
  * middlware must call res.set_static_file_info(filename)
  * you must add route starting with /your/restricted/path/<string>
  */
#if !defined(_WIN32)
        struct static_file_info{
            std::string path = "";
            struct stat statbuf;
            int statResult;
        };
        static_file_info file_info;

        void set_static_file_info(std::string path){
            file_info.path = path;
            file_info.statResult = stat(file_info.path.c_str(), &file_info.statbuf);
            if (file_info.statResult == 0)
            {
                code = 200;
                this->add_header("Content-length", std::to_string(file_info.statbuf.st_size));
            }
        }

        SocketAdaptor* adaptor;
        void do_write_sendfile() {
            off_t start_= 0;
//add mimetypes, headers?
//Content-Disposition, Content-Type


            int fd_{open(file_info.path.c_str(), O_RDONLY)};
            //assert(fd_ != 0);
            if (file_info.statResult == 0)
            {
                ssize_t bytes_sent = 0 ;
                size_t total_bytes_sent = 0;
                sendfile(adaptor->socket().native_handle(), fd_, &start_, file_info.statbuf.st_size - start_);
            }

        }
#endif
/* static file support end */
        private:
            bool completed_{};
            std::function<void()> complete_request_handler_;
            std::function<bool()> is_alive_helper_;

            //In case of a JSON object, set the Content-Type header
            void json_mode()
            {
                set_header("Content-Type", "application/json");
            }
    };
}
