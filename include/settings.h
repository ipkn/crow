#pragma once
// settings for crow
// TODO - replace with runtime config. libucl?

/* #ifdef - enables debug mode */
#define CROW_ENABLE_DEBUG

/* #ifdef - enables logging */
#define CROW_ENABLE_LOGGING

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
