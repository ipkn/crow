#pragma once

#include "utility.h"

namespace crow
{
    namespace detail
    {
        template <typename ... Middlewares>
        struct partial_context
            : public black_magic::pop_back<Middlewares...>::template rebind<partial_context>
            , public black_magic::last_element_type<Middlewares...>::type::context
        {
        };

        template <>
        struct partial_context<>
        {
        };

        template <typename ... Middlewares>
        struct context : private partial_context<Middlewares...>
        //struct context : private Middlewares::context... // simple but less type-safe
        {
            template <typename T> 
            typename T::context& get()
            {
                return static_cast<typename T::context&>(*this);
            }
        };
    }
}
