#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include "utility.h"

namespace crow
{
    enum class HTTPMethod
    {
        DELETE,
        GET,
        HEAD,
        POST,
        PUT,
        CONNECT,
        OPTIONS,
        TRACE,
    };

    inline std::string method_name(HTTPMethod method)
    {
        switch(method)
        {
            case HTTPMethod::DELETE:
                return "DELETE";
            case HTTPMethod::GET:
                return "GET";
            case HTTPMethod::HEAD:
                return "HEAD";
            case HTTPMethod::POST:
                return "POST";
            case HTTPMethod::PUT:
                return "PUT";
            case HTTPMethod::CONNECT:
                return "CONNECT";
            case HTTPMethod::OPTIONS:
                return "OPTIONS";
            case HTTPMethod::TRACE:
                return "TRACE";
        }
        return "invalid";
    }

    enum class ParamType
    {
        INT,
        UINT,
        DOUBLE,
        STRING,
        PATH,

        MAX
    };

    struct routing_params
    {
        std::vector<int64_t> int_params;
        std::vector<uint64_t> uint_params;
        std::vector<double> double_params;
        std::vector<std::string> string_params;

        void debug_print() const
        {
            std::cerr << "routing_params" << std::endl;
            for(auto i:int_params)
                std::cerr<<i <<", " ;
            std::cerr<<std::endl;
            for(auto i:uint_params)
                std::cerr<<i <<", " ;
            std::cerr<<std::endl;
            for(auto i:double_params)
                std::cerr<<i <<", " ;
            std::cerr<<std::endl;
            for(auto& i:string_params)
                std::cerr<<i <<", " ;
            std::cerr<<std::endl;
        }

        template <typename T>
        T get(unsigned) const;

    };

    template<>
    inline int64_t routing_params::get<int64_t>(unsigned index) const
    {
        return int_params[index];
    }

    template<>
    inline uint64_t routing_params::get<uint64_t>(unsigned index) const
    {
        return uint_params[index];
    }

    template<>
    inline double routing_params::get<double>(unsigned index) const
    {
        return double_params[index];
    }

    template<>
    inline std::string routing_params::get<std::string>(unsigned index) const
    {
        return string_params[index];
    }
}

constexpr crow::HTTPMethod operator "" _method(const char* str, size_t len)
{
    return
        crow::black_magic::is_equ_p(str, "GET", 3) ? crow::HTTPMethod::GET :
        crow::black_magic::is_equ_p(str, "DELETE", 6) ? crow::HTTPMethod::DELETE :
        crow::black_magic::is_equ_p(str, "HEAD", 4) ? crow::HTTPMethod::HEAD :
        crow::black_magic::is_equ_p(str, "POST", 4) ? crow::HTTPMethod::POST :
        crow::black_magic::is_equ_p(str, "PUT", 3) ? crow::HTTPMethod::PUT :
        crow::black_magic::is_equ_p(str, "OPTIONS", 7) ? crow::HTTPMethod::OPTIONS :
        crow::black_magic::is_equ_p(str, "CONNECT", 7) ? crow::HTTPMethod::CONNECT :
        crow::black_magic::is_equ_p(str, "TRACE", 5) ? crow::HTTPMethod::TRACE :
        throw std::runtime_error("invalid http method");
};

