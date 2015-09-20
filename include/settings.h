#pragma once
// settings for crow
// TODO - replace with runtime config. libucl?

/* #ifdef - enables debug mode */
#define CROW_ENABLE_DEBUG

/* #ifdef - enables logging */
#define CROW_ENABLE_LOGGING

/* #ifdef - enables SSL */
//#define CROW_ENABLE_SSL

/* #define - specifies log level */
/*
    DEBUG       = 0
    INFO        = 1
    WARNING     = 2
    ERROR       = 3
    CRITICAL    = 4

    default to INFO
*/
#define CROW_LOG_LEVEL 1


// compiler flags
#if __cplusplus >= 201402L
#define CROW_CAN_USE_CPP14
#endif

#if defined(_MSC_VER)
#if _MSC_VER < 1900
#define CROW_MSVC_WORKAROUND
#define constexpr const
#define noexcept throw()
#endif
#endif
