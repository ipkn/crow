#pragma once

#include <cstdint>
#include <utility>
#include <string>
#include <tuple>

#include "utility.h"

namespace flask
{
    namespace black_magic
    {
        constexpr bool is_equ_n(const_str a, int ai, const_str b, int bi, int n)
        {
            return 
                ai + n > a.size() || bi + n > b.size() 
                    ? false :
                n == 0 
                    ? true : 
                a[ai] != b[bi] 
                    ? false : 
                is_equ_n(a,ai+1,b,bi+1,n-1);
        }

        constexpr bool is_int(const_str s, int i)
        {
            return is_equ_n(s, i, "<int>", 0, 5);
        }

        constexpr bool is_float(const_str s, int i)
        {
            return is_equ_n(s, i, "<float>", 0, 7) ||
                is_equ_n(s, i, "<double>", 0, 8);
        }

        constexpr bool is_str(const_str s, int i)
        {
            return is_equ_n(s, i, "<str>", 0, 5);
        }

        constexpr bool is_path(const_str s, int i)
        {
            return is_equ_n(s, i, "<path>", 0, 6);
        }

        template <typename ...Args>
        struct Caller
        {
            template <typename F>
            void operator()(F f, Args... args)
            {
                f(args...);
            }
        };


        template <int N, typename ... Args> struct S;
        template <int N, typename Arg, typename ... Args> struct S<N, Arg, Args...> {
            static_assert(N <= 4+1, "too many routing arguments (maximum 5)");
            template <typename T>
            using push = typename std::conditional<(N>4), S<N, Arg, Args...>, S<N+1, Arg, Args..., T>>::type;
            using pop = S<N-1, Args...>;
        };
        template <> struct S<0>
        {
            template <typename T>
            using push = S<1, T>;
        };

        template <typename F, typename Set>
        struct CallHelper;
        template <typename F, int N, typename ...Args>
        struct CallHelper<F, S<N, Args...>>
        {
            template <typename F1, typename ...Args1, typename = 
                decltype(std::declval<F1>()(std::declval<Args1>()...))
                >
            static char __test(int);

            template <typename ...>
            static int __test(...);

            static constexpr bool value = sizeof(__test<F, Args...>(0)) == sizeof(char);
        };

        static_assert(CallHelper<void(), S<0>>::value, "");
        static_assert(CallHelper<void(int), S<1, int>>::value, "");
        static_assert(!CallHelper<void(int), S<0>>::value, "");
        static_assert(!CallHelper<void(int), S<2, int, int>>::value, "");

        template <typename F, 
            typename Set = S<0>>
        constexpr bool validate_helper(const_str rule, unsigned i=0)
        {
            return 
                i == rule.size() 
                    ? CallHelper<F, Set>::value :
                is_int(rule, i)
                    ? validate_helper<F, typename Set::template push<int>>(rule, find_closing_tag(rule, i+1)+1) :
                is_float(rule, i)
                    ? validate_helper<F, typename Set::template push<double>>(rule, find_closing_tag(rule, i+1)+1) :
                is_str(rule, i)
                    ? validate_helper<F, typename Set::template push<std::string>>(rule, find_closing_tag(rule, i+1)+1) :
                is_path(rule, i)
                    ? validate_helper<F, typename Set::template push<std::string>>(rule, find_closing_tag(rule, i+1)+1) :
                validate_helper<F, Set>(rule, i+1)
            ;
        }

        static_assert(validate_helper<void()>("/"),"");
        static_assert(validate_helper<void(int)>("/<int>"),"");
        static_assert(!validate_helper<void()>("/<int>"),"");
    }

    class Router
    {
    public:
        constexpr Router(black_magic::const_str rule) : rule(rule)
        {
        }

        template <typename F>
        constexpr bool validate() const
        {
            return black_magic::validate_helper<F>(rule);
        }
    private:
        black_magic::const_str rule;
    };
}
