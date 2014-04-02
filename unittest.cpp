#include "routing.h"
#include <functional>
#include "utility.h"

using namespace flask;
using namespace flask::black_magic;

template <int N> struct T{};

int main()
{
    try
    {
        throw T<is_int("1<int>22",0)>(); 
    }
    catch(T<0>)
    {
    }

    try
    {
        throw T<is_int("1<int>22",1)>(); 
    }
    catch(T<1>)
    {
    }
}
