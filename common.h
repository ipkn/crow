#pragma once

#include <string>

namespace flask
{
    enum class ParamType
    {
        INVALID, 

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

        template <typename T>
        T get(unsigned) const;

    };

    template<>
    int64_t routing_params::get<int64_t>(unsigned index) const
    {
        return int_params.at(index);
    }

    template<>
    uint64_t routing_params::get<uint64_t>(unsigned index) const
    {
        return uint_params.at(index);
    }

    template<>
    double routing_params::get<double>(unsigned index) const
    {
        return double_params.at(index);
    }

    template<>
    std::string routing_params::get<std::string>(unsigned index) const
    {
        return string_params.at(index);
    }
}
