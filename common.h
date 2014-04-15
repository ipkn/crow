#pragma once

#include <string>

namespace flask
{
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
    int64_t routing_params::get<int64_t>(unsigned index) const
    {
        return int_params[index];
    }

    template<>
    uint64_t routing_params::get<uint64_t>(unsigned index) const
    {
        return uint_params[index];
    }

    template<>
    double routing_params::get<double>(unsigned index) const
    {
        return double_params[index];
    }

    template<>
    std::string routing_params::get<std::string>(unsigned index) const
    {
        return string_params[index];
    }
}
