#pragma once

#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace crow
{
    namespace black_magic
    {
        struct OutOfRange
        {
            OutOfRange(unsigned pos, unsigned length) {}
        };
        constexpr unsigned requires_in_range( unsigned i, unsigned len )
        {
            return i >= len ? throw OutOfRange(i, len) : i;
        }

        class const_str
        {
            const char * const begin_;
            unsigned size_;

            public:
            template< unsigned N >
                constexpr const_str( const char(&arr)[N] ) : begin_(arr), size_(N - 1) {
                    static_assert( N >= 1, "not a string literal");
                }
            constexpr char operator[]( unsigned i ) const { 
                return requires_in_range(i, size_), begin_[i]; 
            }

            constexpr operator const char *() const { 
                return begin_; 
            }

            constexpr const char* begin() const { return begin_; }
            constexpr const char* end() const { return begin_ + size_; }

            constexpr unsigned size() const { 
                return size_; 
            }
        };


        constexpr unsigned find_closing_tag(const_str s, unsigned p)
        {
            return s[p] == '>' ? p : find_closing_tag(s, p+1);
        }

        constexpr bool is_valid(const_str s, unsigned i = 0, int f = 0)
        {
            return 
                i == s.size()
                    ? f == 0 :
                f < 0 || f >= 2
                    ? false :
                s[i] == '<'
                    ? is_valid(s, i+1, f+1) :
                s[i] == '>'
                    ? is_valid(s, i+1, f-1) :
                is_valid(s, i+1, f);
        }

        constexpr bool is_equ_p(const char* a, const char* b, unsigned n)
        {
            return
                *a == 0 && *b == 0 && n == 0 
                    ? true :
                (*a == 0 || *b == 0)
                    ? false :
                n == 0
                    ? true :
                *a != *b
                    ? false :
                is_equ_p(a+1, b+1, n-1);
        }

        constexpr bool is_equ_n(const_str a, unsigned ai, const_str b, unsigned bi, unsigned n)
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

        constexpr bool is_int(const_str s, unsigned i)
        {
            return is_equ_n(s, i, "<int>", 0, 5);
        }

        constexpr bool is_uint(const_str s, unsigned i)
        {
            return is_equ_n(s, i, "<uint>", 0, 6);
        }

        constexpr bool is_float(const_str s, unsigned i)
        {
            return is_equ_n(s, i, "<float>", 0, 7) ||
                is_equ_n(s, i, "<double>", 0, 8);
        }

        constexpr bool is_str(const_str s, unsigned i)
        {
            return is_equ_n(s, i, "<str>", 0, 5) ||
                is_equ_n(s, i, "<string>", 0, 8);
        }

        constexpr bool is_path(const_str s, unsigned i)
        {
            return is_equ_n(s, i, "<path>", 0, 6);
        }

        constexpr uint64_t get_parameter_tag(const_str s, unsigned p = 0)
        {
            return
                p == s.size() 
                    ?  0 :
                s[p] == '<' ? ( 
                    is_int(s, p)
                        ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 1 :
                    is_uint(s, p)
                        ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 2 :
                    is_float(s, p)
                        ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 3 :
                    is_str(s, p)
                        ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 4 :
                    is_path(s, p)
                        ? get_parameter_tag(s, find_closing_tag(s, p)) * 6 + 5 :
                    throw std::runtime_error("invalid parameter type")
                    ) : 
                get_parameter_tag(s, p+1);
        }

        template <typename ... T>
        struct S
        {
            template <typename U>
            using push = S<U, T...>;
            template <typename U>
            using push_back = S<T..., U>;
            template <template<typename ... Args> class U>
            using rebind = U<T...>;
        };
template <typename F, typename Set>
        struct CallHelper;
        template <typename F, typename ...Args>
        struct CallHelper<F, S<Args...>>
        {
            template <typename F1, typename ...Args1, typename = 
                decltype(std::declval<F1>()(std::declval<Args1>()...))
                >
            static char __test(int);

            template <typename ...>
            static int __test(...);

            static constexpr bool value = sizeof(__test<F, Args...>(0)) == sizeof(char);
        };


        template <int N>
        struct single_tag_to_type
        {
        };

        template <>
        struct single_tag_to_type<1>
        {
            using type = int64_t;
        };

        template <>
        struct single_tag_to_type<2>
        {
            using type = uint64_t;
        };

        template <>
        struct single_tag_to_type<3>
        {
            using type = double;
        };

        template <>
        struct single_tag_to_type<4>
        {
            using type = std::string;
        };

        template <>
        struct single_tag_to_type<5>
        {
            using type = std::string;
        };


        template <uint64_t Tag> 
        struct arguments
        {
            using subarguments = typename arguments<Tag/6>::type;
            using type = 
                typename subarguments::template push<typename single_tag_to_type<Tag%6>::type>;
        };

        template <> 
        struct arguments<0>
        {
            using type = S<>;
        };

        template <typename ... T>
        struct last_element_type
        {
            using type = typename std::tuple_element<sizeof...(T)-1, std::tuple<T...>>::type;
        };


        template <>
        struct last_element_type<>
        {
        };


        // from http://stackoverflow.com/questions/13072359/c11-compile-time-array-with-logarithmic-evaluation-depth
        template<class T> using Invoke = typename T::type;

        template<unsigned...> struct seq{ using type = seq; };

        template<class S1, class S2> struct concat;

        template<unsigned... I1, unsigned... I2>
        struct concat<seq<I1...>, seq<I2...>>
          : seq<I1..., (sizeof...(I1)+I2)...>{};

        template<class S1, class S2>
        using Concat = Invoke<concat<S1, S2>>;

        template<unsigned N> struct gen_seq;
        template<unsigned N> using GenSeq = Invoke<gen_seq<N>>;

        template<unsigned N>
        struct gen_seq : Concat<GenSeq<N/2>, GenSeq<N - N/2>>{};

        template<> struct gen_seq<0> : seq<>{};
        template<> struct gen_seq<1> : seq<0>{};

        template <typename Seq, typename Tuple> 
        struct pop_back_helper;

        template <unsigned ... N, typename Tuple>
        struct pop_back_helper<seq<N...>, Tuple>
        {
            template <template <typename ... Args> class U>
            using rebind = U<typename std::tuple_element<N, Tuple>::type...>;
        };

        template <typename ... T>
        struct pop_back //: public pop_back_helper<typename gen_seq<sizeof...(T)-1>::type, std::tuple<T...>>
        {
            template <template <typename ... Args> class U>
            using rebind = typename pop_back_helper<typename gen_seq<sizeof...(T)-1>::type, std::tuple<T...>>::template rebind<U>;
        };

        template <>
        struct pop_back<>
        {
            template <template <typename ... Args> class U>
            using rebind = U<>;
        };

        // from http://stackoverflow.com/questions/2118541/check-if-c0x-parameter-pack-contains-a-type
        template < typename Tp, typename... List >
        struct contains : std::true_type {};

        template < typename Tp, typename Head, typename... Rest >
        struct contains<Tp, Head, Rest...>
        : std::conditional< std::is_same<Tp, Head>::value,
            std::true_type,
            contains<Tp, Rest...>
        >::type {};

        template < typename Tp >
        struct contains<Tp> : std::false_type {};

        template <typename T>
        struct empty_context
        {
        };

    } // namespace black_magic

    namespace detail
    {

        template <class T, std::size_t N, class... Args>
        struct get_index_of_element_from_tuple_by_type_impl
        {
            static constexpr auto value = N;
        };

        template <class T, std::size_t N, class... Args>
        struct get_index_of_element_from_tuple_by_type_impl<T, N, T, Args...>
        {
            static constexpr auto value = N;
        };

        template <class T, std::size_t N, class U, class... Args>
        struct get_index_of_element_from_tuple_by_type_impl<T, N, U, Args...>
        {
            static constexpr auto value = get_index_of_element_from_tuple_by_type_impl<T, N + 1, Args...>::value;
        };

    } // namespace detail

    namespace utility
    {
        template <class T, class... Args>
        T& get_element_by_type(std::tuple<Args...>& t)
        {
            return std::get<detail::get_index_of_element_from_tuple_by_type_impl<T, 0, Args...>::value>(t);
        }

    } // namespace utility
}
