#pragma once

#include <string>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>

#include "crow/settings.h"

namespace crow
{
    enum class LogLevel
    {
#ifndef ERROR
        DEBUG = 0,
        INFO,
        WARNING,
        ERROR,
        CRITICAL,
#endif

        Debug = 0,
        Info,
        Warning,
        Error,
        Critical,
    };

    class ILogHandler {
        public:
            virtual void log(std::string message, LogLevel level) = 0;
    };

    class CerrLogHandler : public ILogHandler {
        public:
            void log(std::string message, LogLevel /*level*/) override {
                std::cerr << message;
            }
    };

    class logger {

        private:
            //
            static std::string timestamp()
            {
                char date[32];
                time_t t = time(0);

                tm my_tm;

#if defined(_MSC_VER) or defined(__MINGW32__)
                gmtime_s(&my_tm, &t);
#else
                gmtime_r(&t, &my_tm);
#endif

                size_t sz = strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &my_tm);
                return std::string(date, date+sz);
            }

        public:


            logger(std::string prefix, LogLevel level) : level_(level) {
    #ifdef CROW_ENABLE_LOGGING
                    stringstream_ << "(" << timestamp() << ") [" << prefix << "] ";
    #endif

            }
            ~logger() {
    #ifdef CROW_ENABLE_LOGGING
                if(level_ >= get_current_log_level()) {
                    stringstream_ << std::endl;
                    get_handler_ref()->log(stringstream_.str(), level_);
                }
    #endif
            }

            //
            template <typename T>
            logger& operator<<(T const &value) {

    #ifdef CROW_ENABLE_LOGGING
                if(level_ >= get_current_log_level()) {
                    stringstream_ << value;
                }
    #endif
                return *this;
            }

            //
            static void setLogLevel(LogLevel level) {
                get_log_level_ref() = level;
            }

            static void setHandler(ILogHandler* handler) {
                get_handler_ref() = handler;
            }

            static LogLevel get_current_log_level() {
                return get_log_level_ref();
            }

        private:
            //
            static LogLevel& get_log_level_ref()
            {
                static LogLevel current_level = (LogLevel)CROW_LOG_LEVEL;
                return current_level;
            }
            static ILogHandler*& get_handler_ref()
            {
                static CerrLogHandler default_handler;
                static ILogHandler* current_handler = &default_handler;
                return current_handler;
            }

            //
            std::ostringstream stringstream_;
            LogLevel level_;
    };
}

#define CROW_LOG_CRITICAL   \
        if (crow::logger::get_current_log_level() <= crow::LogLevel::Critical) \
            crow::logger("CRITICAL", crow::LogLevel::Critical)
#define CROW_LOG_ERROR      \
        if (crow::logger::get_current_log_level() <= crow::LogLevel::Error) \
            crow::logger("ERROR   ", crow::LogLevel::Error)
#define CROW_LOG_WARNING    \
        if (crow::logger::get_current_log_level() <= crow::LogLevel::Warning) \
            crow::logger("WARNING ", crow::LogLevel::Warning)
#define CROW_LOG_INFO       \
        if (crow::logger::get_current_log_level() <= crow::LogLevel::Info) \
            crow::logger("INFO    ", crow::LogLevel::Info)
#define CROW_LOG_DEBUG      \
        if (crow::logger::get_current_log_level() <= crow::LogLevel::Debug) \
            crow::logger("DEBUG   ", crow::LogLevel::Debug)

