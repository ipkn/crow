#pragma once

#include <cstdint>

#include "utility.h"

namespace flask
{
    namespace black_magic
    {
        constexpr bool is_equ_n(StrWrap a, int ai, StrWrap b, int bi, int n)
        {
            return n == 0 ? true : a[ai] != b[bi] ? false : is_equ_n(a,ai+1,b,bi+1,n-1);
        }

        constexpr bool is_int(StrWrap s, int i)
        {
            return is_equ_n(s, i, "<int>", 0, 5);
        }

        constexpr bool is_str(StrWrap s, int i)
        {
            return is_equ_n(s, i, "<str>", 0, 5);
        }

        //constexpr ? parse_route(StrWrap s)
        //{
            //return 
        //}
    }

    class Router
    {
    public:
        constexpr Router(black_magic::StrWrap s)
        {
        }
    };
}
