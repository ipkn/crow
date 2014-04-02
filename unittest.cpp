#include "routing.h"
#include <functional>
#include "utility.h"

using namespace flask;
using namespace flask::black_magic;

template <int N> struct ThrowTest{};

int main()
{
    try
    {
        throw ThrowTest<is_int("1<int>22",0)>(); 
    }
    catch(ThrowTest<0>)
    {
    }

    try
    {
        throw ThrowTest<is_int("1<int>22",1)>(); 
    }
    catch(ThrowTest<1>)
    {
    }

    {
        constexpr Router r = Router("/");
        static_assert(r.validate<void()>(), "Good handler");
        static_assert(!r.validate<void(int)>(), "Bad handler - no int argument");
    }
    {
        constexpr Router r = Router("/blog/<int>");
        static_assert(!r.validate<void()>(), "Bad handler - need argument");
        static_assert(r.validate<void(int)>(), "Good handler");
        static_assert(!r.validate<void(std::string)>(), "Bad handler - int is not convertible to std::string");
        static_assert(r.validate<void(double)>(), "Acceptable handler - int will be converted to double");
    }
}
