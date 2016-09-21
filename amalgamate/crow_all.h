#pragma once

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <iostream>

// ----------------------------------------------------------------------------
// qs_parse (modified)
// https://github.com/bartgrantham/qs_parse
// ----------------------------------------------------------------------------
/*  Similar to strncmp, but handles URL-encoding for either string  */
int qs_strncmp(const char * s, const char * qs, size_t n);


/*  Finds the beginning of each key/value pair and stores a pointer in qs_kv.
 *  Also decodes the value portion of the k/v pair *in-place*.  In a future
 *  enhancement it will also have a compile-time option of sorting qs_kv
 *  alphabetically by key.  */
int qs_parse(char * qs, char * qs_kv[], int qs_kv_size);


/*  Used by qs_parse to decode the value portion of a k/v pair  */
int qs_decode(char * qs);


/*  Looks up the value according to the key on a pre-processed query string
 *  A future enhancement will be a compile-time option to look up the key
 *  in a pre-sorted qs_kv array via a binary search.  */
//char * qs_k2v(const char * key, char * qs_kv[], int qs_kv_size);
 char * qs_k2v(const char * key, char * const * qs_kv, int qs_kv_size, int nth);


/*  Non-destructive lookup of value, based on key.  User provides the
 *  destinaton string and length.  */
char * qs_scanvalue(const char * key, const char * qs, char * val, size_t val_len);

// TODO: implement sorting of the qs_kv array; for now ensure it's not compiled
#undef _qsSORTING

// isxdigit _is_ available in <ctype.h>, but let's avoid another header instead
#define CROW_QS_ISHEX(x)    ((((x)>='0'&&(x)<='9') || ((x)>='A'&&(x)<='F') || ((x)>='a'&&(x)<='f')) ? 1 : 0)
#define CROW_QS_HEX2DEC(x)  (((x)>='0'&&(x)<='9') ? (x)-48 : ((x)>='A'&&(x)<='F') ? (x)-55 : ((x)>='a'&&(x)<='f') ? (x)-87 : 0)
#define CROW_QS_ISQSCHR(x) ((((x)=='=')||((x)=='#')||((x)=='&')||((x)=='\0')) ? 0 : 1)

inline int qs_strncmp(const char * s, const char * qs, size_t n)
{
    int i=0;
    unsigned char u1, u2, unyb, lnyb;

    while(n-- > 0)
    {
        u1 = (unsigned char) *s++;
        u2 = (unsigned char) *qs++;

        if ( ! CROW_QS_ISQSCHR(u1) ) {  u1 = '\0';  }
        if ( ! CROW_QS_ISQSCHR(u2) ) {  u2 = '\0';  }

        if ( u1 == '+' ) {  u1 = ' ';  }
        if ( u1 == '%' ) // easier/safer than scanf
        {
            unyb = (unsigned char) *s++;
            lnyb = (unsigned char) *s++;
            if ( CROW_QS_ISHEX(unyb) && CROW_QS_ISHEX(lnyb) )
                u1 = (CROW_QS_HEX2DEC(unyb) * 16) + CROW_QS_HEX2DEC(lnyb);
            else
                u1 = '\0';
        }

        if ( u2 == '+' ) {  u2 = ' ';  }
        if ( u2 == '%' ) // easier/safer than scanf
        {
            unyb = (unsigned char) *qs++;
            lnyb = (unsigned char) *qs++;
            if ( CROW_QS_ISHEX(unyb) && CROW_QS_ISHEX(lnyb) )
                u2 = (CROW_QS_HEX2DEC(unyb) * 16) + CROW_QS_HEX2DEC(lnyb);
            else
                u2 = '\0';
        }

        if ( u1 != u2 )
            return u1 - u2;
        if ( u1 == '\0' )
            return 0;
        i++;
    }
    if ( CROW_QS_ISQSCHR(*qs) )
        return -1;
    else
        return 0;
}


inline int qs_parse(char * qs, char * qs_kv[], int qs_kv_size)
{
    int i, j;
    char * substr_ptr;

    for(i=0; i<qs_kv_size; i++)  qs_kv[i] = NULL;

    // find the beginning of the k/v substrings or the fragment
    substr_ptr = qs + strcspn(qs, "?#");
    if (substr_ptr[0] != '\0')
        substr_ptr++;
    else
        return 0; // no query or fragment

    i=0;
    while(i<qs_kv_size)
    {
        qs_kv[i] = substr_ptr;
        j = strcspn(substr_ptr, "&");
        if ( substr_ptr[j] == '\0' ) {  break;  }
        substr_ptr += j + 1;
        i++;
    }
    i++;  // x &'s -> means x iterations of this loop -> means *x+1* k/v pairs

    // we only decode the values in place, the keys could have '='s in them
    // which will hose our ability to distinguish keys from values later
    for(j=0; j<i; j++)
    {
        substr_ptr = qs_kv[j] + strcspn(qs_kv[j], "=&#");
        if ( substr_ptr[0] == '&' || substr_ptr[0] == '\0')  // blank value: skip decoding
            substr_ptr[0] = '\0';
        else
            qs_decode(++substr_ptr);
    }

#ifdef _qsSORTING
// TODO: qsort qs_kv, using qs_strncmp() for the comparison
#endif

    return i;
}


inline int qs_decode(char * qs)
{
    int i=0, j=0;

    while( CROW_QS_ISQSCHR(qs[j]) )
    {
        if ( qs[j] == '+' ) {  qs[i] = ' ';  }
        else if ( qs[j] == '%' ) // easier/safer than scanf
        {
            if ( ! CROW_QS_ISHEX(qs[j+1]) || ! CROW_QS_ISHEX(qs[j+2]) )
            {
                qs[i] = '\0';
                return i;
            }
            qs[i] = (CROW_QS_HEX2DEC(qs[j+1]) * 16) + CROW_QS_HEX2DEC(qs[j+2]);
            j+=2;
        }
        else
        {
            qs[i] = qs[j];
        }
        i++;  j++;
    }
    qs[i] = '\0';

    return i;
}


inline char * qs_k2v(const char * key, char * const * qs_kv, int qs_kv_size, int nth = 0)
{
    int i;
    size_t key_len, skip;

    key_len = strlen(key);

#ifdef _qsSORTING
// TODO: binary search for key in the sorted qs_kv
#else  // _qsSORTING
    for(i=0; i<qs_kv_size; i++)
    {
        // we rely on the unambiguous '=' to find the value in our k/v pair
        if ( qs_strncmp(key, qs_kv[i], key_len) == 0 )
        {
            skip = strcspn(qs_kv[i], "=");
            if ( qs_kv[i][skip] == '=' )
                skip++;
            // return (zero-char value) ? ptr to trailing '\0' : ptr to value
            if(nth == 0)
                return qs_kv[i] + skip;
            else 
                --nth;
        }
    }
#endif  // _qsSORTING

    return NULL;
}


inline char * qs_scanvalue(const char * key, const char * qs, char * val, size_t val_len)
{
    size_t i, key_len;
    const char * tmp;

    // find the beginning of the k/v substrings
    if ( (tmp = strchr(qs, '?')) != NULL )
        qs = tmp + 1;

    key_len = strlen(key);
    while(qs[0] != '#' && qs[0] != '\0')
    {
        if ( qs_strncmp(key, qs, key_len) == 0 )
            break;
        qs += strcspn(qs, "&") + 1;
    }

    if ( qs[0] == '\0' ) return NULL;

    qs += strcspn(qs, "=&#");
    if ( qs[0] == '=' )
    {
        qs++;
        i = strcspn(qs, "&=#");
        strncpy(val, qs, (val_len-1)<(i+1) ? (val_len-1) : (i+1));
        qs_decode(val);
    }
    else
    {
        if ( val_len > 0 )
            val[0] = '\0';
    }

    return val;
}
// ----------------------------------------------------------------------------


namespace crow 
{
    class query_string
    {
    public:
        static const int MAX_KEY_VALUE_PAIRS_COUNT = 256;

        query_string()
        {

        }

        query_string(const query_string& qs)
            : url_(qs.url_)
        {
            for(auto p:qs.key_value_pairs_)
            {
                key_value_pairs_.push_back((char*)(p-qs.url_.c_str()+url_.c_str()));
            }
        }

        query_string& operator = (const query_string& qs)
        {
            url_ = qs.url_;
            key_value_pairs_.clear();
            for(auto p:qs.key_value_pairs_)
            {
                key_value_pairs_.push_back((char*)(p-qs.url_.c_str()+url_.c_str()));
            }
            return *this;
        }

        query_string& operator = (query_string&& qs)
        {
            key_value_pairs_ = std::move(qs.key_value_pairs_);
            char* old_data = (char*)qs.url_.c_str();
            url_ = std::move(qs.url_);
            for(auto& p:key_value_pairs_)
            {
                p += (char*)url_.c_str() - old_data;
            }
            return *this;
        }


        query_string(std::string url)
            : url_(std::move(url))
        {
            if (url_.empty())
                return;

            key_value_pairs_.resize(MAX_KEY_VALUE_PAIRS_COUNT);

            int count = qs_parse(&url_[0], &key_value_pairs_[0], MAX_KEY_VALUE_PAIRS_COUNT);
            key_value_pairs_.resize(count);
        }

        void clear() 
        {
            key_value_pairs_.clear();
            url_.clear();
        }

        friend std::ostream& operator<<(std::ostream& os, const query_string& qs)
        {
            os << "[ ";
            for(size_t i = 0; i < qs.key_value_pairs_.size(); ++i) {
                if (i)
                    os << ", ";
                os << qs.key_value_pairs_[i];
            }
            os << " ]";
            return os;

        }

        char* get (const std::string& name) const
        {
            char* ret = qs_k2v(name.c_str(), key_value_pairs_.data(), key_value_pairs_.size());
            return ret;
        }

        std::vector<char*> get_list (const std::string& name) const
        {
            std::vector<char*> ret;
            std::string plus = name + "[]";            
            char* element = nullptr;

            int count = 0;
            while(1)
            {
                element = qs_k2v(plus.c_str(), key_value_pairs_.data(), key_value_pairs_.size(), count++);
                if (!element)
                    break;
                ret.push_back(element);
            }
            return ret;
        }


    private:
        std::string url_;
        std::vector<char*> key_value_pairs_;
    };

} // end namespace



/* merged revision: 5b951d74bd66ec9d38448e0a85b1cf8b85d97db3 */
/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#ifndef CROW_http_parser_h
#define CROW_http_parser_h
#ifdef __cplusplus
extern "C" {
#endif

/* Also update SONAME in the Makefile whenever you change these. */
#define CROW_HTTP_PARSER_VERSION_MAJOR 2
#define CROW_HTTP_PARSER_VERSION_MINOR 3
#define CROW_HTTP_PARSER_VERSION_PATCH 0

#include <sys/types.h>
#if defined(_WIN32) && !defined(__MINGW32__) && (!defined(_MSC_VER) || _MSC_VER<1600)
#include <BaseTsd.h>
#include <stddef.h>
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

/* Compile with -DHTTP_PARSER_STRICT=0 to make less checks, but run
 * faster
 */
#ifndef CROW_HTTP_PARSER_STRICT
# define CROW_HTTP_PARSER_STRICT 1
#endif

/* Maximium header size allowed. If the macro is not defined
 * before including this header then the default is used. To
 * change the maximum header size, define the macro in the build
 * environment (e.g. -DHTTP_MAX_HEADER_SIZE=<value>). To remove
 * the effective limit on the size of the header, define the macro
 * to a very large number (e.g. -DHTTP_MAX_HEADER_SIZE=0x7fffffff)
 */
#ifndef CROW_HTTP_MAX_HEADER_SIZE
# define CROW_HTTP_MAX_HEADER_SIZE (80*1024)
#endif

typedef struct http_parser http_parser;
typedef struct http_parser_settings http_parser_settings;


/* Callbacks should return non-zero to indicate an error. The parser will
 * then halt execution.
 *
 * The one exception is on_headers_complete. In a HTTP_RESPONSE parser
 * returning '1' from on_headers_complete will tell the parser that it
 * should not expect a body. This is used when receiving a response to a
 * HEAD request which may contain 'Content-Length' or 'Transfer-Encoding:
 * chunked' headers that indicate the presence of a body.
 *
 * http_data_cb does not return data chunks. It will be call arbitrarally
 * many times for each string. E.G. you might get 10 callbacks for "on_url"
 * each providing just a few characters more data.
 */
typedef int (*http_data_cb) (http_parser*, const char *at, size_t length);
typedef int (*http_cb) (http_parser*);


/* Request Methods */
#define CROW_HTTP_METHOD_MAP(CROW_XX)         \
  CROW_XX(0,  DELETE,      DELETE)       \
  CROW_XX(1,  GET,         GET)          \
  CROW_XX(2,  HEAD,        HEAD)         \
  CROW_XX(3,  POST,        POST)         \
  CROW_XX(4,  PUT,         PUT)          \
  /* pathological */                \
  CROW_XX(5,  CONNECT,     CONNECT)      \
  CROW_XX(6,  OPTIONS,     OPTIONS)      \
  CROW_XX(7,  TRACE,       TRACE)        \
  /* webdav */                      \
  CROW_XX(8,  COPY,        COPY)         \
  CROW_XX(9,  LOCK,        LOCK)         \
  CROW_XX(10, MKCOL,       MKCOL)        \
  CROW_XX(11, MOVE,        MOVE)         \
  CROW_XX(12, PROPFIND,    PROPFIND)     \
  CROW_XX(13, PROPPATCH,   PROPPATCH)    \
  CROW_XX(14, SEARCH,      SEARCH)       \
  CROW_XX(15, UNLOCK,      UNLOCK)       \
  /* subversion */                  \
  CROW_XX(16, REPORT,      REPORT)       \
  CROW_XX(17, MKACTIVITY,  MKACTIVITY)   \
  CROW_XX(18, CHECKOUT,    CHECKOUT)     \
  CROW_XX(19, MERGE,       MERGE)        \
  /* upnp */                        \
  CROW_XX(20, MSEARCH,     M-SEARCH)     \
  CROW_XX(21, NOTIFY,      NOTIFY)       \
  CROW_XX(22, SUBSCRIBE,   SUBSCRIBE)    \
  CROW_XX(23, UNSUBSCRIBE, UNSUBSCRIBE)  \
  /* RFC-5789 */                    \
  CROW_XX(24, PATCH,       PATCH)        \
  CROW_XX(25, PURGE,       PURGE)        \
  /* CalDAV */                      \
  CROW_XX(26, MKCALENDAR,  MKCALENDAR)   \

enum http_method
  {
#define CROW_XX(num, name, string) HTTP_##name = num,
  CROW_HTTP_METHOD_MAP(CROW_XX)
#undef CROW_XX
  };


enum http_parser_type { HTTP_REQUEST, HTTP_RESPONSE, HTTP_BOTH };


/* Flag values for http_parser.flags field */
enum flags
  { F_CHUNKED               = 1 << 0
  , F_CONNECTION_KEEP_ALIVE = 1 << 1
  , F_CONNECTION_CLOSE      = 1 << 2
  , F_TRAILING              = 1 << 3
  , F_UPGRADE               = 1 << 4
  , F_SKIPBODY              = 1 << 5
  };


/* Map for errno-related constants
 * 
 * The provided argument should be a macro that takes 2 arguments.
 */
#define CROW_HTTP_ERRNO_MAP(CROW_XX)                                           \
  /* No error */                                                     \
  CROW_XX(OK, "success")                                                  \
                                                                     \
  /* Callback-related errors */                                      \
  CROW_XX(CB_message_begin, "the on_message_begin callback failed")       \
  CROW_XX(CB_url, "the on_url callback failed")                           \
  CROW_XX(CB_header_field, "the on_header_field callback failed")         \
  CROW_XX(CB_header_value, "the on_header_value callback failed")         \
  CROW_XX(CB_headers_complete, "the on_headers_complete callback failed") \
  CROW_XX(CB_body, "the on_body callback failed")                         \
  CROW_XX(CB_message_complete, "the on_message_complete callback failed") \
  CROW_XX(CB_status, "the on_status callback failed")                     \
                                                                     \
  /* Parsing-related errors */                                       \
  CROW_XX(INVALID_EOF_STATE, "stream ended at an unexpected time")        \
  CROW_XX(HEADER_OVERFLOW,                                                \
     "too many header bytes seen; overflow detected")                \
  CROW_XX(CLOSED_CONNECTION,                                              \
     "data received after completed connection: close message")      \
  CROW_XX(INVALID_VERSION, "invalid HTTP version")                        \
  CROW_XX(INVALID_STATUS, "invalid HTTP status code")                     \
  CROW_XX(INVALID_METHOD, "invalid HTTP method")                          \
  CROW_XX(INVALID_URL, "invalid URL")                                     \
  CROW_XX(INVALID_HOST, "invalid host")                                   \
  CROW_XX(INVALID_PORT, "invalid port")                                   \
  CROW_XX(INVALID_PATH, "invalid path")                                   \
  CROW_XX(INVALID_QUERY_STRING, "invalid query string")                   \
  CROW_XX(INVALID_FRAGMENT, "invalid fragment")                           \
  CROW_XX(LF_EXPECTED, "CROW_LF character expected")                           \
  CROW_XX(INVALID_HEADER_TOKEN, "invalid character in header")            \
  CROW_XX(INVALID_CONTENT_LENGTH,                                         \
     "invalid character in content-length header")                   \
  CROW_XX(INVALID_CHUNK_SIZE,                                             \
     "invalid character in chunk size header")                       \
  CROW_XX(INVALID_CONSTANT, "invalid constant string")                    \
  CROW_XX(INVALID_INTERNAL_STATE, "encountered unexpected internal state")\
  CROW_XX(STRICT, "strict mode assertion failed")                         \
  CROW_XX(PAUSED, "parser is paused")                                     \
  CROW_XX(UNKNOWN, "an unknown error occurred")


/* Define HPE_* values for each errno value above */
#define CROW_HTTP_ERRNO_GEN(n, s) HPE_##n,
enum http_errno {
  CROW_HTTP_ERRNO_MAP(CROW_HTTP_ERRNO_GEN)
};
#undef CROW_HTTP_ERRNO_GEN


/* Get an http_errno value from an http_parser */
#define CROW_HTTP_PARSER_ERRNO(p)            ((enum http_errno) (p)->http_errno)


struct http_parser {
  /** PRIVATE **/
  unsigned int type : 2;         /* enum http_parser_type */
  unsigned int flags : 6;        /* F_* values from 'flags' enum; semi-public */
  unsigned int state : 8;        /* enum state from http_parser.c */
  unsigned int header_state : 8; /* enum header_state from http_parser.c */
  unsigned int index : 8;        /* index into current matcher */

  uint32_t nread;          /* # bytes read in various scenarios */
  uint64_t content_length; /* # bytes in body (0 if no Content-Length header) */

  /** READ-ONLY **/
  unsigned short http_major;
  unsigned short http_minor;
  unsigned int status_code : 16; /* responses only */
  unsigned int method : 8;       /* requests only */
  unsigned int http_errno : 7;

  /* 1 = Upgrade header was present and the parser has exited because of that.
   * 0 = No upgrade header present.
   * Should be checked when http_parser_execute() returns in addition to
   * error checking.
   */
  unsigned int upgrade : 1;

  /** PUBLIC **/
  void *data; /* A pointer to get hook to the "connection" or "socket" object */
};


struct http_parser_settings {
  http_cb      on_message_begin;
  http_data_cb on_url;
  http_data_cb on_status;
  http_data_cb on_header_field;
  http_data_cb on_header_value;
  http_cb      on_headers_complete;
  http_data_cb on_body;
  http_cb      on_message_complete;
};


enum http_parser_url_fields
  { UF_SCHEMA           = 0
  , UF_HOST             = 1
  , UF_PORT             = 2
  , UF_PATH             = 3
  , UF_QUERY            = 4
  , UF_FRAGMENT         = 5
  , UF_USERINFO         = 6
  , UF_MAX              = 7
  };


/* Result structure for http_parser_parse_url().
 *
 * Callers should index into field_data[] with UF_* values iff field_set
 * has the relevant (1 << UF_*) bit set. As a courtesy to clients (and
 * because we probably have padding left over), we convert any port to
 * a uint16_t.
 */
struct http_parser_url {
  uint16_t field_set;           /* Bitmask of (1 << UF_*) values */
  uint16_t port;                /* Converted UF_PORT string */

  struct {
    uint16_t off;               /* Offset into buffer in which field starts */
    uint16_t len;               /* Length of run in buffer */
  } field_data[UF_MAX];
};


/* Returns the library version. Bits 16-23 contain the major version number,
 * bits 8-15 the minor version number and bits 0-7 the patch level.
 * Usage example:
 *
 *   unsigned long version = http_parser_version();
 *   unsigned major = (version >> 16) & 255;
 *   unsigned minor = (version >> 8) & 255;
 *   unsigned patch = version & 255;
 *   printf("http_parser v%u.%u.%u\n", major, minor, version);
 */
unsigned long http_parser_version(void);

void http_parser_init(http_parser *parser, enum http_parser_type type);


size_t http_parser_execute(http_parser *parser,
                           const http_parser_settings *settings,
                           const char *data,
                           size_t len);


/* If http_should_keep_alive() in the on_headers_complete or
 * on_message_complete callback returns 0, then this should be
 * the last message on the connection.
 * If you are the server, respond with the "Connection: close" header.
 * If you are the client, close the connection.
 */
int http_should_keep_alive(const http_parser *parser);

/* Returns a string version of the HTTP method. */
const char *http_method_str(enum http_method m);

/* Return a string name of the given error */
const char *http_errno_name(enum http_errno err);

/* Return a string description of the given error */
const char *http_errno_description(enum http_errno err);

/* Parse a URL; return nonzero on failure */
int http_parser_parse_url(const char *buf, size_t buflen,
                          int is_connect,
                          struct http_parser_url *u);

/* Pause or un-pause the parser; a nonzero value pauses */
void http_parser_pause(http_parser *parser, int paused);

/* Checks if this is the final chunk of the body. */
int http_body_is_final(const http_parser *parser);

/*#include "http_parser.h"*/
/* Based on src/http/ngx_http_parse.c from NGINX copyright Igor Sysoev
 *
 * Additional changes are licensed under the same terms as NGINX and
 * copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <assert.h>
#include <stddef.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifndef CROW_ULLONG_MAX
# define CROW_ULLONG_MAX ((uint64_t) -1) /* 2^64-1 */
#endif

#ifndef CROW_MIN
# define CROW_MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef CROW_ARRAY_SIZE
# define CROW_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef CROW_BIT_AT
# define CROW_BIT_AT(a, i)                                                \
  (!!((unsigned int) (a)[(unsigned int) (i) >> 3] &                  \
   (1 << ((unsigned int) (i) & 7))))
#endif

#ifndef CROW_ELEM_AT
# define CROW_ELEM_AT(a, i, v) ((unsigned int) (i) < CROW_ARRAY_SIZE(a) ? (a)[(i)] : (v))
#endif

#define CROW_SET_ERRNO(e)                                                 \
do {                                                                 \
  parser->http_errno = (e);                                          \
} while(0)


/* Run the notify callback FOR, returning ER if it fails */
#define CROW_CALLBACK_NOTIFY_(FOR, ER)                                    \
do {                                                                 \
  assert(CROW_HTTP_PARSER_ERRNO(parser) == HPE_OK);                       \
                                                                     \
  if (settings->on_##FOR) {                                          \
    if (0 != settings->on_##FOR(parser)) {                           \
      CROW_SET_ERRNO(HPE_CB_##FOR);                                       \
    }                                                                \
                                                                     \
    /* We either errored above or got paused; get out */             \
    if (CROW_HTTP_PARSER_ERRNO(parser) != HPE_OK) {                       \
      return (ER);                                                   \
    }                                                                \
  }                                                                  \
} while (0)

/* Run the notify callback FOR and consume the current byte */
#define CROW_CALLBACK_NOTIFY(FOR)            CROW_CALLBACK_NOTIFY_(FOR, p - data + 1)

/* Run the notify callback FOR and don't consume the current byte */
#define CROW_CALLBACK_NOTIFY_NOADVANCE(FOR)  CROW_CALLBACK_NOTIFY_(FOR, p - data)

/* Run data callback FOR with LEN bytes, returning ER if it fails */
#define CROW_CALLBACK_DATA_(FOR, LEN, ER)                                 \
do {                                                                 \
  assert(CROW_HTTP_PARSER_ERRNO(parser) == HPE_OK);                       \
                                                                     \
  if (FOR##_mark) {                                                  \
    if (settings->on_##FOR) {                                        \
      if (0 != settings->on_##FOR(parser, FOR##_mark, (LEN))) {      \
        CROW_SET_ERRNO(HPE_CB_##FOR);                                     \
      }                                                              \
                                                                     \
      /* We either errored above or got paused; get out */           \
      if (CROW_HTTP_PARSER_ERRNO(parser) != HPE_OK) {                     \
        return (ER);                                                 \
      }                                                              \
    }                                                                \
    FOR##_mark = NULL;                                               \
  }                                                                  \
} while (0)
  
/* Run the data callback FOR and consume the current byte */
#define CROW_CALLBACK_DATA(FOR)                                           \
    CROW_CALLBACK_DATA_(FOR, p - FOR##_mark, p - data + 1)

/* Run the data callback FOR and don't consume the current byte */
#define CROW_CALLBACK_DATA_NOADVANCE(FOR)                                 \
    CROW_CALLBACK_DATA_(FOR, p - FOR##_mark, p - data)

/* Set the mark FOR; non-destructive if mark is already set */
#define CROW_MARK(FOR)                                                    \
do {                                                                 \
  if (!FOR##_mark) {                                                 \
    FOR##_mark = p;                                                  \
  }                                                                  \
} while (0)


#define CROW_PROXY_CONNECTION "proxy-connection"
#define CROW_CONNECTION "connection"
#define CROW_CONTENT_LENGTH "content-length"
#define CROW_TRANSFER_ENCODING "transfer-encoding"
#define CROW_UPGRADE "upgrade"
#define CROW_CHUNKED "chunked"
#define CROW_KEEP_ALIVE "keep-alive"
#define CROW_CLOSE "close"




enum state
  { s_dead = 1 /* important that this is > 0 */

  , s_start_req_or_res
  , s_res_or_resp_H
  , s_start_res
  , s_res_H
  , s_res_HT
  , s_res_HTT
  , s_res_HTTP
  , s_res_first_http_major
  , s_res_http_major
  , s_res_first_http_minor
  , s_res_http_minor
  , s_res_first_status_code
  , s_res_status_code
  , s_res_status_start
  , s_res_status
  , s_res_line_almost_done

  , s_start_req

  , s_req_method
  , s_req_spaces_before_url
  , s_req_schema
  , s_req_schema_slash
  , s_req_schema_slash_slash
  , s_req_server_start
  , s_req_server
  , s_req_server_with_at
  , s_req_path
  , s_req_query_string_start
  , s_req_query_string
  , s_req_fragment_start
  , s_req_fragment
  , s_req_http_start
  , s_req_http_H
  , s_req_http_HT
  , s_req_http_HTT
  , s_req_http_HTTP
  , s_req_first_http_major
  , s_req_http_major
  , s_req_first_http_minor
  , s_req_http_minor
  , s_req_line_almost_done

  , s_header_field_start
  , s_header_field
  , s_header_value_discard_ws
  , s_header_value_discard_ws_almost_done
  , s_header_value_discard_lws
  , s_header_value_start
  , s_header_value
  , s_header_value_lws

  , s_header_almost_done

  , s_chunk_size_start
  , s_chunk_size
  , s_chunk_parameters
  , s_chunk_size_almost_done

  , s_headers_almost_done
  , s_headers_done

  /* Important: 's_headers_done' must be the last 'header' state. All
   * states beyond this must be 'body' states. It is used for overflow
   * checking. See the CROW_PARSING_HEADER() macro.
   */

  , s_chunk_data
  , s_chunk_data_almost_done
  , s_chunk_data_done

  , s_body_identity
  , s_body_identity_eof

  , s_message_done
  };


#define CROW_PARSING_HEADER(state) (state <= s_headers_done)


enum header_states
  { h_general = 0
  , h_C
  , h_CO
  , h_CON

  , h_matching_connection
  , h_matching_proxy_connection
  , h_matching_content_length
  , h_matching_transfer_encoding
  , h_matching_upgrade

  , h_connection
  , h_content_length
  , h_transfer_encoding
  , h_upgrade

  , h_matching_transfer_encoding_chunked
  , h_matching_connection_keep_alive
  , h_matching_connection_close

  , h_transfer_encoding_chunked
  , h_connection_keep_alive
  , h_connection_close
  };

enum http_host_state
  {
    s_http_host_dead = 1
  , s_http_userinfo_start
  , s_http_userinfo
  , s_http_host_start
  , s_http_host_v6_start
  , s_http_host
  , s_http_host_v6
  , s_http_host_v6_end
  , s_http_host_port_start
  , s_http_host_port
};

/* Macros for character classes; depends on strict-mode  */
#define CROW_CR                  '\r'
#define CROW_LF                  '\n'
#define CROW_LOWER(c)            (unsigned char)(c | 0x20)
#define CROW_IS_ALPHA(c)         (CROW_LOWER(c) >= 'a' && CROW_LOWER(c) <= 'z')
#define CROW_IS_NUM(c)           ((c) >= '0' && (c) <= '9')
#define CROW_IS_ALPHANUM(c)      (CROW_IS_ALPHA(c) || CROW_IS_NUM(c))
#define CROW_IS_HEX(c)           (CROW_IS_NUM(c) || (CROW_LOWER(c) >= 'a' && CROW_LOWER(c) <= 'f'))
#define CROW_IS_MARK(c)          ((c) == '-' || (c) == '_' || (c) == '.' || \
  (c) == '!' || (c) == '~' || (c) == '*' || (c) == '\'' || (c) == '(' || \
  (c) == ')')
#define CROW_IS_USERINFO_CHAR(c) (CROW_IS_ALPHANUM(c) || CROW_IS_MARK(c) || (c) == '%' || \
  (c) == ';' || (c) == ':' || (c) == '&' || (c) == '=' || (c) == '+' || \
  (c) == '$' || (c) == ',')

#if CROW_HTTP_PARSER_STRICT
#define CROW_TOKEN(c)            (tokens[(unsigned char)c])
#define CROW_IS_URL_CHAR(c)      (CROW_BIT_AT(normal_url_char, (unsigned char)c))
#define CROW_IS_HOST_CHAR(c)     (CROW_IS_ALPHANUM(c) || (c) == '.' || (c) == '-')
#else
#define CROW_TOKEN(c)            ((c == ' ') ? ' ' : tokens[(unsigned char)c])
#define CROW_IS_URL_CHAR(c)                                                         \
  (CROW_BIT_AT(normal_url_char, (unsigned char)c) || ((c) & 0x80))
#define CROW_IS_HOST_CHAR(c)                                                        \
  (CROW_IS_ALPHANUM(c) || (c) == '.' || (c) == '-' || (c) == '_')
#endif


#define CROW_start_state (parser->type == HTTP_REQUEST ? s_start_req : s_start_res)


#if CROW_HTTP_PARSER_STRICT
# define CROW_STRICT_CHECK(cond)                                          \
do {                                                                 \
  if (cond) {                                                        \
    CROW_SET_ERRNO(HPE_STRICT);                                           \
    goto error;                                                      \
  }                                                                  \
} while (0)
# define CROW_NEW_MESSAGE() (http_should_keep_alive(parser) ? CROW_start_state : s_dead)
#else
# define CROW_STRICT_CHECK(cond)
# define CROW_NEW_MESSAGE() CROW_start_state
#endif



int http_message_needs_eof(const http_parser *parser);

/* Our URL parser.
 *
 * This is designed to be shared by http_parser_execute() for URL validation,
 * hence it has a state transition + byte-for-byte interface. In addition, it
 * is meant to be embedded in http_parser_parse_url(), which does the dirty
 * work of turning state transitions URL components for its API.
 *
 * This function should only be invoked with non-space characters. It is
 * assumed that the caller cares about (and can detect) the transition between
 * URL and non-URL states by looking for these.
 */
inline enum state
parse_url_char(enum state s, const char ch)
{
#if CROW_HTTP_PARSER_STRICT
# define CROW_T(v) 0
#else
# define CROW_T(v) v
#endif


static const uint8_t normal_url_char[32] = {
/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
        0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
        0    | CROW_T(2)   |   0    |   0    | CROW_T(16)  |   0    |   0    |   0,
/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
        0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
        0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
        0    |   2    |   4    |   0    |   16   |   32   |   64   |  128,
/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |   0,
/*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/*  80  P    81  Q    82  R    83  S    84  CROW_T    85  U    86  V    87  W  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |   0, };

#undef CROW_T

  if (ch == ' ' || ch == '\r' || ch == '\n') {
    return s_dead;
  }

#if CROW_HTTP_PARSER_STRICT
  if (ch == '\t' || ch == '\f') {
    return s_dead;
  }
#endif

  switch (s) {
    case s_req_spaces_before_url:
      /* Proxied requests are followed by scheme of an absolute URI (alpha).
       * All methods except CONNECT are followed by '/' or '*'.
       */

      if (ch == '/' || ch == '*') {
        return s_req_path;
      }

      if (CROW_IS_ALPHA(ch)) {
        return s_req_schema;
      }

      break;

    case s_req_schema:
      if (CROW_IS_ALPHA(ch)) {
        return s;
      }

      if (ch == ':') {
        return s_req_schema_slash;
      }

      break;

    case s_req_schema_slash:
      if (ch == '/') {
        return s_req_schema_slash_slash;
      }

      break;

    case s_req_schema_slash_slash:
      if (ch == '/') {
        return s_req_server_start;
      }

      break;

    case s_req_server_with_at:
      if (ch == '@') {
        return s_dead;
      }

    /* FALLTHROUGH */
    case s_req_server_start:
    case s_req_server:
      if (ch == '/') {
        return s_req_path;
      }

      if (ch == '?') {
        return s_req_query_string_start;
      }

      if (ch == '@') {
        return s_req_server_with_at;
      }

      if (CROW_IS_USERINFO_CHAR(ch) || ch == '[' || ch == ']') {
        return s_req_server;
      }

      break;

    case s_req_path:
      if (CROW_IS_URL_CHAR(ch)) {
        return s;
      }

      switch (ch) {
        case '?':
          return s_req_query_string_start;

        case '#':
          return s_req_fragment_start;
      }

      break;

    case s_req_query_string_start:
    case s_req_query_string:
      if (CROW_IS_URL_CHAR(ch)) {
        return s_req_query_string;
      }

      switch (ch) {
        case '?':
          /* allow extra '?' in query string */
          return s_req_query_string;

        case '#':
          return s_req_fragment_start;
      }

      break;

    case s_req_fragment_start:
      if (CROW_IS_URL_CHAR(ch)) {
        return s_req_fragment;
      }

      switch (ch) {
        case '?':
          return s_req_fragment;

        case '#':
          return s;
      }

      break;

    case s_req_fragment:
      if (CROW_IS_URL_CHAR(ch)) {
        return s;
      }

      switch (ch) {
        case '?':
        case '#':
          return s;
      }

      break;

    default:
      break;
  }

  /* We should never fall out of the switch above unless there's an error */
  return s_dead;
}

inline size_t http_parser_execute (http_parser *parser,
                            const http_parser_settings *settings,
                            const char *data,
                            size_t len)
{
static const char *method_strings[] =
  {
#define CROW_XX(num, name, string) #string,
  CROW_HTTP_METHOD_MAP(CROW_XX)
#undef CROW_XX
  };

/* Tokens as defined by rfc 2616. Also lowercases them.
 *        token       = 1*<any CHAR except CTLs or separators>
 *     separators     = "(" | ")" | "<" | ">" | "@"
 *                    | "," | ";" | ":" | "\" | <">
 *                    | "/" | "[" | "]" | "?" | "="
 *                    | "{" | "}" | SP | HT
 */
static const char tokens[256] = {
/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
        0,       0,       0,       0,       0,       0,       0,       0,
/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
        0,      '!',      0,      '#',     '$',     '%',     '&',    '\'',
/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
        0,       0,      '*',     '+',      0,      '-',     '.',      0,
/*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
       '0',     '1',     '2',     '3',     '4',     '5',     '6',     '7',
/*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
       '8',     '9',      0,       0,       0,       0,       0,       0,
/*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
        0,      'a',     'b',     'c',     'd',     'e',     'f',     'g',
/*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
       'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
/*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
       'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
/*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
       'x',     'y',     'z',      0,       0,       0,      '^',     '_',
/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
       '`',     'a',     'b',     'c',     'd',     'e',     'f',     'g',
/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
       'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
       'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
       'x',     'y',     'z',      0,      '|',      0,      '~',       0 };


static const int8_t unhex[256] =
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  , 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1
  ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  };



  char c, ch;
  int8_t unhex_val;
  const char *p = data;
  const char *header_field_mark = 0;
  const char *header_value_mark = 0;
  const char *url_mark = 0;
  const char *body_mark = 0;
  const char *status_mark = 0;

  /* We're in an error state. Don't bother doing anything. */
  if (CROW_HTTP_PARSER_ERRNO(parser) != HPE_OK) {
    return 0;
  }

  if (len == 0) {
    switch (parser->state) {
      case s_body_identity_eof:
        /* Use of CROW_CALLBACK_NOTIFY() here would erroneously return 1 byte read if
         * we got paused.
         */
        CROW_CALLBACK_NOTIFY_NOADVANCE(message_complete);
        return 0;

      case s_dead:
      case s_start_req_or_res:
      case s_start_res:
      case s_start_req:
        return 0;

      default:
        CROW_SET_ERRNO(HPE_INVALID_EOF_STATE);
        return 1;
    }
  }


  if (parser->state == s_header_field)
    header_field_mark = data;
  if (parser->state == s_header_value)
    header_value_mark = data;
  switch (parser->state) {
  case s_req_path:
  case s_req_schema:
  case s_req_schema_slash:
  case s_req_schema_slash_slash:
  case s_req_server_start:
  case s_req_server:
  case s_req_server_with_at:
  case s_req_query_string_start:
  case s_req_query_string:
  case s_req_fragment_start:
  case s_req_fragment:
    url_mark = data;
    break;
  case s_res_status:
    status_mark = data;
    break;
  }

  for (p=data; p != data + len; p++) {
    ch = *p;

    if (CROW_PARSING_HEADER(parser->state)) {
      ++parser->nread;
      /* Don't allow the total size of the HTTP headers (including the status
       * line) to exceed CROW_HTTP_MAX_HEADER_SIZE.  This check is here to protect
       * embedders against denial-of-service attacks where the attacker feeds
       * us a never-ending header that the embedder keeps buffering.
       *
       * This check is arguably the responsibility of embedders but we're doing
       * it on the embedder's behalf because most won't bother and this way we
       * make the web a little safer.  CROW_HTTP_MAX_HEADER_SIZE is still far bigger
       * than any reasonable request or response so this should never affect
       * day-to-day operation.
       */
      if (parser->nread > (CROW_HTTP_MAX_HEADER_SIZE)) {
        CROW_SET_ERRNO(HPE_HEADER_OVERFLOW);
        goto error;
      }
    }

    reexecute_byte:
    switch (parser->state) {

      case s_dead:
        /* this state is used after a 'Connection: close' message
         * the parser will error out if it reads another message
         */
        if (ch == CROW_CR || ch == CROW_LF)
          break;

        CROW_SET_ERRNO(HPE_CLOSED_CONNECTION);
        goto error;

      case s_start_req_or_res:
      {
        if (ch == CROW_CR || ch == CROW_LF)
          break;
        parser->flags = 0;
        parser->content_length = CROW_ULLONG_MAX;

        if (ch == 'H') {
          parser->state = s_res_or_resp_H;

          CROW_CALLBACK_NOTIFY(message_begin);
        } else {
          parser->type = HTTP_REQUEST;
          parser->state = s_start_req;
          goto reexecute_byte;
        }

        break;
      }

      case s_res_or_resp_H:
        if (ch == 'T') {
          parser->type = HTTP_RESPONSE;
          parser->state = s_res_HT;
        } else {
          if (ch != 'E') {
            CROW_SET_ERRNO(HPE_INVALID_CONSTANT);
            goto error;
          }

          parser->type = HTTP_REQUEST;
          parser->method = HTTP_HEAD;
          parser->index = 2;
          parser->state = s_req_method;
        }
        break;

      case s_start_res:
      {
        parser->flags = 0;
        parser->content_length = CROW_ULLONG_MAX;

        switch (ch) {
          case 'H':
            parser->state = s_res_H;
            break;

          case CROW_CR:
          case CROW_LF:
            break;

          default:
            CROW_SET_ERRNO(HPE_INVALID_CONSTANT);
            goto error;
        }

        CROW_CALLBACK_NOTIFY(message_begin);
        break;
      }

      case s_res_H:
        CROW_STRICT_CHECK(ch != 'T');
        parser->state = s_res_HT;
        break;

      case s_res_HT:
        CROW_STRICT_CHECK(ch != 'T');
        parser->state = s_res_HTT;
        break;

      case s_res_HTT:
        CROW_STRICT_CHECK(ch != 'P');
        parser->state = s_res_HTTP;
        break;

      case s_res_HTTP:
        CROW_STRICT_CHECK(ch != '/');
        parser->state = s_res_first_http_major;
        break;

      case s_res_first_http_major:
        if (ch < '0' || ch > '9') {
          CROW_SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_major = ch - '0';
        parser->state = s_res_http_major;
        break;

      /* major HTTP version or dot */
      case s_res_http_major:
      {
        if (ch == '.') {
          parser->state = s_res_first_http_minor;
          break;
        }

        if (!CROW_IS_NUM(ch)) {
          CROW_SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_major *= 10;
        parser->http_major += ch - '0';

        if (parser->http_major > 999) {
          CROW_SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        break;
      }

      /* first digit of minor HTTP version */
      case s_res_first_http_minor:
        if (!CROW_IS_NUM(ch)) {
          CROW_SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_minor = ch - '0';
        parser->state = s_res_http_minor;
        break;

      /* minor HTTP version or end of request line */
      case s_res_http_minor:
      {
        if (ch == ' ') {
          parser->state = s_res_first_status_code;
          break;
        }

        if (!CROW_IS_NUM(ch)) {
          CROW_SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_minor *= 10;
        parser->http_minor += ch - '0';

        if (parser->http_minor > 999) {
          CROW_SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        break;
      }

      case s_res_first_status_code:
      {
        if (!CROW_IS_NUM(ch)) {
          if (ch == ' ') {
            break;
          }

          CROW_SET_ERRNO(HPE_INVALID_STATUS);
          goto error;
        }
        parser->status_code = ch - '0';
        parser->state = s_res_status_code;
        break;
      }

      case s_res_status_code:
      {
        if (!CROW_IS_NUM(ch)) {
          switch (ch) {
            case ' ':
              parser->state = s_res_status_start;
              break;
            case CROW_CR:
              parser->state = s_res_line_almost_done;
              break;
            case CROW_LF:
              parser->state = s_header_field_start;
              break;
            default:
              CROW_SET_ERRNO(HPE_INVALID_STATUS);
              goto error;
          }
          break;
        }

        parser->status_code *= 10;
        parser->status_code += ch - '0';

        if (parser->status_code > 999) {
          CROW_SET_ERRNO(HPE_INVALID_STATUS);
          goto error;
        }

        break;
      }

      case s_res_status_start:
      {
        if (ch == CROW_CR) {
          parser->state = s_res_line_almost_done;
          break;
        }

        if (ch == CROW_LF) {
          parser->state = s_header_field_start;
          break;
        }

        CROW_MARK(status);
        parser->state = s_res_status;
        parser->index = 0;
        break;
      }

      case s_res_status:
        if (ch == CROW_CR) {
          parser->state = s_res_line_almost_done;
          CROW_CALLBACK_DATA(status);
          break;
        }

        if (ch == CROW_LF) {
          parser->state = s_header_field_start;
          CROW_CALLBACK_DATA(status);
          break;
        }

        break;

      case s_res_line_almost_done:
        CROW_STRICT_CHECK(ch != CROW_LF);
        parser->state = s_header_field_start;
        break;

      case s_start_req:
      {
        if (ch == CROW_CR || ch == CROW_LF)
          break;
        parser->flags = 0;
        parser->content_length = CROW_ULLONG_MAX;

        if (!CROW_IS_ALPHA(ch)) {
          CROW_SET_ERRNO(HPE_INVALID_METHOD);
          goto error;
        }

        parser->method = (enum http_method) 0;
        parser->index = 1;
        switch (ch) {
          case 'C': parser->method = HTTP_CONNECT; /* or COPY, CHECKOUT */ break;
          case 'D': parser->method = HTTP_DELETE; break;
          case 'G': parser->method = HTTP_GET; break;
          case 'H': parser->method = HTTP_HEAD; break;
          case 'L': parser->method = HTTP_LOCK; break;
          case 'M': parser->method = HTTP_MKCOL; /* or MOVE, MKACTIVITY, MERGE, M-SEARCH, MKCALENDAR */ break;
          case 'N': parser->method = HTTP_NOTIFY; break;
          case 'O': parser->method = HTTP_OPTIONS; break;
          case 'P': parser->method = HTTP_POST;
            /* or PROPFIND|PROPPATCH|PUT|PATCH|PURGE */
            break;
          case 'R': parser->method = HTTP_REPORT; break;
          case 'S': parser->method = HTTP_SUBSCRIBE; /* or SEARCH */ break;
          case 'T': parser->method = HTTP_TRACE; break;
          case 'U': parser->method = HTTP_UNLOCK; /* or UNSUBSCRIBE */ break;
          default:
            CROW_SET_ERRNO(HPE_INVALID_METHOD);
            goto error;
        }
        parser->state = s_req_method;

        CROW_CALLBACK_NOTIFY(message_begin);

        break;
      }

      case s_req_method:
      {
        const char *matcher;
        if (ch == '\0') {
          CROW_SET_ERRNO(HPE_INVALID_METHOD);
          goto error;
        }

        matcher = method_strings[parser->method];
        if (ch == ' ' && matcher[parser->index] == '\0') {
          parser->state = s_req_spaces_before_url;
        } else if (ch == matcher[parser->index]) {
          ; /* nada */
        } else if (parser->method == HTTP_CONNECT) {
          if (parser->index == 1 && ch == 'H') {
            parser->method = HTTP_CHECKOUT;
          } else if (parser->index == 2  && ch == 'P') {
            parser->method = HTTP_COPY;
          } else {
            CROW_SET_ERRNO(HPE_INVALID_METHOD);
            goto error;
          }
        } else if (parser->method == HTTP_MKCOL) {
          if (parser->index == 1 && ch == 'O') {
            parser->method = HTTP_MOVE;
          } else if (parser->index == 1 && ch == 'E') {
            parser->method = HTTP_MERGE;
          } else if (parser->index == 1 && ch == '-') {
            parser->method = HTTP_MSEARCH;
          } else if (parser->index == 2 && ch == 'A') {
            parser->method = HTTP_MKACTIVITY;
          } else if (parser->index == 3 && ch == 'A') {
            parser->method = HTTP_MKCALENDAR;
          } else {
            CROW_SET_ERRNO(HPE_INVALID_METHOD);
            goto error;
          }
        } else if (parser->method == HTTP_SUBSCRIBE) {
          if (parser->index == 1 && ch == 'E') {
            parser->method = HTTP_SEARCH;
          } else {
            CROW_SET_ERRNO(HPE_INVALID_METHOD);
            goto error;
          }
        } else if (parser->index == 1 && parser->method == HTTP_POST) {
          if (ch == 'R') {
            parser->method = HTTP_PROPFIND; /* or HTTP_PROPPATCH */
          } else if (ch == 'U') {
            parser->method = HTTP_PUT; /* or HTTP_PURGE */
          } else if (ch == 'A') {
            parser->method = HTTP_PATCH;
          } else {
            CROW_SET_ERRNO(HPE_INVALID_METHOD);
            goto error;
          }
        } else if (parser->index == 2) {
          if (parser->method == HTTP_PUT) {
            if (ch == 'R') {
              parser->method = HTTP_PURGE;
            } else {
              CROW_SET_ERRNO(HPE_INVALID_METHOD);
              goto error;
            }
          } else if (parser->method == HTTP_UNLOCK) {
            if (ch == 'S') {
              parser->method = HTTP_UNSUBSCRIBE;
            } else {
              CROW_SET_ERRNO(HPE_INVALID_METHOD);
              goto error;
            }
          } else {
            CROW_SET_ERRNO(HPE_INVALID_METHOD);
            goto error;
          }
        } else if (parser->index == 4 && parser->method == HTTP_PROPFIND && ch == 'P') {
          parser->method = HTTP_PROPPATCH;
        } else {
          CROW_SET_ERRNO(HPE_INVALID_METHOD);
          goto error;
        }

        ++parser->index;
        break;
      }

      case s_req_spaces_before_url:
      {
        if (ch == ' ') break;

        CROW_MARK(url);
        if (parser->method == HTTP_CONNECT) {
          parser->state = s_req_server_start;
        }

        parser->state = parse_url_char((enum state)parser->state, ch);
        if (parser->state == s_dead) {
          CROW_SET_ERRNO(HPE_INVALID_URL);
          goto error;
        }

        break;
      }

      case s_req_schema:
      case s_req_schema_slash:
      case s_req_schema_slash_slash:
      case s_req_server_start:
      {
        switch (ch) {
          /* No whitespace allowed here */
          case ' ':
          case CROW_CR:
          case CROW_LF:
            CROW_SET_ERRNO(HPE_INVALID_URL);
            goto error;
          default:
            parser->state = parse_url_char((enum state)parser->state, ch);
            if (parser->state == s_dead) {
              CROW_SET_ERRNO(HPE_INVALID_URL);
              goto error;
            }
        }

        break;
      }

      case s_req_server:
      case s_req_server_with_at:
      case s_req_path:
      case s_req_query_string_start:
      case s_req_query_string:
      case s_req_fragment_start:
      case s_req_fragment:
      {
        switch (ch) {
          case ' ':
            parser->state = s_req_http_start;
            CROW_CALLBACK_DATA(url);
            break;
          case CROW_CR:
          case CROW_LF:
            parser->http_major = 0;
            parser->http_minor = 9;
            parser->state = (ch == CROW_CR) ?
              s_req_line_almost_done :
              s_header_field_start;
            CROW_CALLBACK_DATA(url);
            break;
          default:
            parser->state = parse_url_char((enum state)parser->state, ch);
            if (parser->state == s_dead) {
              CROW_SET_ERRNO(HPE_INVALID_URL);
              goto error;
            }
        }
        break;
      }

      case s_req_http_start:
        switch (ch) {
          case 'H':
            parser->state = s_req_http_H;
            break;
          case ' ':
            break;
          default:
            CROW_SET_ERRNO(HPE_INVALID_CONSTANT);
            goto error;
        }
        break;

      case s_req_http_H:
        CROW_STRICT_CHECK(ch != 'T');
        parser->state = s_req_http_HT;
        break;

      case s_req_http_HT:
        CROW_STRICT_CHECK(ch != 'T');
        parser->state = s_req_http_HTT;
        break;

      case s_req_http_HTT:
        CROW_STRICT_CHECK(ch != 'P');
        parser->state = s_req_http_HTTP;
        break;

      case s_req_http_HTTP:
        CROW_STRICT_CHECK(ch != '/');
        parser->state = s_req_first_http_major;
        break;

      /* first digit of major HTTP version */
      case s_req_first_http_major:
        if (ch < '1' || ch > '9') {
          CROW_SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_major = ch - '0';
        parser->state = s_req_http_major;
        break;

      /* major HTTP version or dot */
      case s_req_http_major:
      {
        if (ch == '.') {
          parser->state = s_req_first_http_minor;
          break;
        }

        if (!CROW_IS_NUM(ch)) {
          CROW_SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_major *= 10;
        parser->http_major += ch - '0';

        if (parser->http_major > 999) {
          CROW_SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        break;
      }

      /* first digit of minor HTTP version */
      case s_req_first_http_minor:
        if (!CROW_IS_NUM(ch)) {
          CROW_SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_minor = ch - '0';
        parser->state = s_req_http_minor;
        break;

      /* minor HTTP version or end of request line */
      case s_req_http_minor:
      {
        if (ch == CROW_CR) {
          parser->state = s_req_line_almost_done;
          break;
        }

        if (ch == CROW_LF) {
          parser->state = s_header_field_start;
          break;
        }

        /* XXX allow spaces after digit? */

        if (!CROW_IS_NUM(ch)) {
          CROW_SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_minor *= 10;
        parser->http_minor += ch - '0';

        if (parser->http_minor > 999) {
          CROW_SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        break;
      }

      /* end of request line */
      case s_req_line_almost_done:
      {
        if (ch != CROW_LF) {
          CROW_SET_ERRNO(HPE_LF_EXPECTED);
          goto error;
        }

        parser->state = s_header_field_start;
        break;
      }

      case s_header_field_start:
      {
        if (ch == CROW_CR) {
          parser->state = s_headers_almost_done;
          break;
        }

        if (ch == CROW_LF) {
          /* they might be just sending \n instead of \r\n so this would be
           * the second \n to denote the end of headers*/
          parser->state = s_headers_almost_done;
          goto reexecute_byte;
        }

        c = CROW_TOKEN(ch);

        if (!c) {
          CROW_SET_ERRNO(HPE_INVALID_HEADER_TOKEN);
          goto error;
        }

        CROW_MARK(header_field);

        parser->index = 0;
        parser->state = s_header_field;

        switch (c) {
          case 'c':
            parser->header_state = h_C;
            break;

          case 'p':
            parser->header_state = h_matching_proxy_connection;
            break;

          case 't':
            parser->header_state = h_matching_transfer_encoding;
            break;

          case 'u':
            parser->header_state = h_matching_upgrade;
            break;

          default:
            parser->header_state = h_general;
            break;
        }
        break;
      }

      case s_header_field:
      {
        c = CROW_TOKEN(ch);

        if (c) {
          switch (parser->header_state) {
            case h_general:
              break;

            case h_C:
              parser->index++;
              parser->header_state = (c == 'o' ? h_CO : h_general);
              break;

            case h_CO:
              parser->index++;
              parser->header_state = (c == 'n' ? h_CON : h_general);
              break;

            case h_CON:
              parser->index++;
              switch (c) {
                case 'n':
                  parser->header_state = h_matching_connection;
                  break;
                case 't':
                  parser->header_state = h_matching_content_length;
                  break;
                default:
                  parser->header_state = h_general;
                  break;
              }
              break;

            /* connection */

            case h_matching_connection:
              parser->index++;
              if (parser->index > sizeof(CROW_CONNECTION)-1
                  || c != CROW_CONNECTION[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(CROW_CONNECTION)-2) {
                parser->header_state = h_connection;
              }
              break;

            /* proxy-connection */

            case h_matching_proxy_connection:
              parser->index++;
              if (parser->index > sizeof(CROW_PROXY_CONNECTION)-1
                  || c != CROW_PROXY_CONNECTION[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(CROW_PROXY_CONNECTION)-2) {
                parser->header_state = h_connection;
              }
              break;

            /* content-length */

            case h_matching_content_length:
              parser->index++;
              if (parser->index > sizeof(CROW_CONTENT_LENGTH)-1
                  || c != CROW_CONTENT_LENGTH[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(CROW_CONTENT_LENGTH)-2) {
                parser->header_state = h_content_length;
              }
              break;

            /* transfer-encoding */

            case h_matching_transfer_encoding:
              parser->index++;
              if (parser->index > sizeof(CROW_TRANSFER_ENCODING)-1
                  || c != CROW_TRANSFER_ENCODING[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(CROW_TRANSFER_ENCODING)-2) {
                parser->header_state = h_transfer_encoding;
              }
              break;

            /* upgrade */

            case h_matching_upgrade:
              parser->index++;
              if (parser->index > sizeof(CROW_UPGRADE)-1
                  || c != CROW_UPGRADE[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(CROW_UPGRADE)-2) {
                parser->header_state = h_upgrade;
              }
              break;

            case h_connection:
            case h_content_length:
            case h_transfer_encoding:
            case h_upgrade:
              if (ch != ' ') parser->header_state = h_general;
              break;

            default:
              assert(0 && "Unknown header_state");
              break;
          }
          break;
        }

        if (ch == ':') {
          parser->state = s_header_value_discard_ws;
          CROW_CALLBACK_DATA(header_field);
          break;
        }

        if (ch == CROW_CR) {
          parser->state = s_header_almost_done;
          CROW_CALLBACK_DATA(header_field);
          break;
        }

        if (ch == CROW_LF) {
          parser->state = s_header_field_start;
          CROW_CALLBACK_DATA(header_field);
          break;
        }

        CROW_SET_ERRNO(HPE_INVALID_HEADER_TOKEN);
        goto error;
      }

      case s_header_value_discard_ws:
        if (ch == ' ' || ch == '\t') break;

        if (ch == CROW_CR) {
          parser->state = s_header_value_discard_ws_almost_done;
          break;
        }

        if (ch == CROW_LF) {
          parser->state = s_header_value_discard_lws;
          break;
        }

        /* FALLTHROUGH */

      case s_header_value_start:
      {
        CROW_MARK(header_value);

        parser->state = s_header_value;
        parser->index = 0;

        c = CROW_LOWER(ch);

        switch (parser->header_state) {
          case h_upgrade:
            parser->flags |= F_UPGRADE;
            parser->header_state = h_general;
            break;

          case h_transfer_encoding:
            /* looking for 'Transfer-Encoding: chunked' */
            if ('c' == c) {
              parser->header_state = h_matching_transfer_encoding_chunked;
            } else {
              parser->header_state = h_general;
            }
            break;

          case h_content_length:
            if (!CROW_IS_NUM(ch)) {
              CROW_SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
              goto error;
            }

            parser->content_length = ch - '0';
            break;

          case h_connection:
            /* looking for 'Connection: keep-alive' */
            if (c == 'k') {
              parser->header_state = h_matching_connection_keep_alive;
            /* looking for 'Connection: close' */
            } else if (c == 'c') {
              parser->header_state = h_matching_connection_close;
            } else {
              parser->header_state = h_general;
            }
            break;

          default:
            parser->header_state = h_general;
            break;
        }
        break;
      }

      case s_header_value:
      {

        if (ch == CROW_CR) {
          parser->state = s_header_almost_done;
          CROW_CALLBACK_DATA(header_value);
          break;
        }

        if (ch == CROW_LF) {
          parser->state = s_header_almost_done;
          CROW_CALLBACK_DATA_NOADVANCE(header_value);
          goto reexecute_byte;
        }

        c = CROW_LOWER(ch);

        switch (parser->header_state) {
          case h_general:
            break;

          case h_connection:
          case h_transfer_encoding:
            assert(0 && "Shouldn't get here.");
            break;

          case h_content_length:
          {
            uint64_t t;

            if (ch == ' ') break;

            if (!CROW_IS_NUM(ch)) {
              CROW_SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
              goto error;
            }

            t = parser->content_length;
            t *= 10;
            t += ch - '0';

            /* Overflow? Test against a conservative limit for simplicity. */
            if ((CROW_ULLONG_MAX - 10) / 10 < parser->content_length) {
              CROW_SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
              goto error;
            }

            parser->content_length = t;
            break;
          }

          /* Transfer-Encoding: chunked */
          case h_matching_transfer_encoding_chunked:
            parser->index++;
            if (parser->index > sizeof(CROW_CHUNKED)-1
                || c != CROW_CHUNKED[parser->index]) {
              parser->header_state = h_general;
            } else if (parser->index == sizeof(CROW_CHUNKED)-2) {
              parser->header_state = h_transfer_encoding_chunked;
            }
            break;

          /* looking for 'Connection: keep-alive' */
          case h_matching_connection_keep_alive:
            parser->index++;
            if (parser->index > sizeof(CROW_KEEP_ALIVE)-1
                || c != CROW_KEEP_ALIVE[parser->index]) {
              parser->header_state = h_general;
            } else if (parser->index == sizeof(CROW_KEEP_ALIVE)-2) {
              parser->header_state = h_connection_keep_alive;
            }
            break;

          /* looking for 'Connection: close' */
          case h_matching_connection_close:
            parser->index++;
            if (parser->index > sizeof(CROW_CLOSE)-1 || c != CROW_CLOSE[parser->index]) {
              parser->header_state = h_general;
            } else if (parser->index == sizeof(CROW_CLOSE)-2) {
              parser->header_state = h_connection_close;
            }
            break;

          case h_transfer_encoding_chunked:
          case h_connection_keep_alive:
          case h_connection_close:
            if (ch != ' ') parser->header_state = h_general;
            break;

          default:
            parser->state = s_header_value;
            parser->header_state = h_general;
            break;
        }
        break;
      }

      case s_header_almost_done:
      {
        CROW_STRICT_CHECK(ch != CROW_LF);

        parser->state = s_header_value_lws;
        break;
      }

      case s_header_value_lws:
      {
        if (ch == ' ' || ch == '\t') {
          parser->state = s_header_value_start;
          goto reexecute_byte;
        }

        /* finished the header */
        switch (parser->header_state) {
          case h_connection_keep_alive:
            parser->flags |= F_CONNECTION_KEEP_ALIVE;
            break;
          case h_connection_close:
            parser->flags |= F_CONNECTION_CLOSE;
            break;
          case h_transfer_encoding_chunked:
            parser->flags |= F_CHUNKED;
            break;
          default:
            break;
        }

        parser->state = s_header_field_start;
        goto reexecute_byte;
      }

      case s_header_value_discard_ws_almost_done:
      {
        CROW_STRICT_CHECK(ch != CROW_LF);
        parser->state = s_header_value_discard_lws;
        break;
      }

      case s_header_value_discard_lws:
      {
        if (ch == ' ' || ch == '\t') {
          parser->state = s_header_value_discard_ws;
          break;
        } else {
          /* header value was empty */
          CROW_MARK(header_value);
          parser->state = s_header_field_start;
          CROW_CALLBACK_DATA_NOADVANCE(header_value);
          goto reexecute_byte;
        }
      }

      case s_headers_almost_done:
      {
        CROW_STRICT_CHECK(ch != CROW_LF);

        if (parser->flags & F_TRAILING) {
          /* End of a chunked request */
          parser->state = CROW_NEW_MESSAGE();
          CROW_CALLBACK_NOTIFY(message_complete);
          break;
        }

        parser->state = s_headers_done;

        /* Set this here so that on_headers_complete() callbacks can see it */
        parser->upgrade =
          (parser->flags & F_UPGRADE || parser->method == HTTP_CONNECT);

        /* Here we call the headers_complete callback. This is somewhat
         * different than other callbacks because if the user returns 1, we
         * will interpret that as saying that this message has no body. This
         * is needed for the annoying case of recieving a response to a HEAD
         * request.
         *
         * We'd like to use CROW_CALLBACK_NOTIFY_NOADVANCE() here but we cannot, so
         * we have to simulate it by handling a change in errno below.
         */
        if (settings->on_headers_complete) {
          switch (settings->on_headers_complete(parser)) {
            case 0:
              break;

            case 1:
              parser->flags |= F_SKIPBODY;
              break;

            default:
              CROW_SET_ERRNO(HPE_CB_headers_complete);
              return p - data; /* Error */
          }
        }

        if (CROW_HTTP_PARSER_ERRNO(parser) != HPE_OK) {
          return p - data;
        }

        goto reexecute_byte;
      }

      case s_headers_done:
      {
        CROW_STRICT_CHECK(ch != CROW_LF);

        parser->nread = 0;

        /* Exit, the rest of the connect is in a different protocol. */
        if (parser->upgrade) {
          parser->state = CROW_NEW_MESSAGE();
          CROW_CALLBACK_NOTIFY(message_complete);
          return (p - data) + 1;
        }

        if (parser->flags & F_SKIPBODY) {
          parser->state = CROW_NEW_MESSAGE();
          CROW_CALLBACK_NOTIFY(message_complete);
        } else if (parser->flags & F_CHUNKED) {
          /* chunked encoding - ignore Content-Length header */
          parser->state = s_chunk_size_start;
        } else {
          if (parser->content_length == 0) {
            /* Content-Length header given but zero: Content-Length: 0\r\n */
            parser->state = CROW_NEW_MESSAGE();
            CROW_CALLBACK_NOTIFY(message_complete);
          } else if (parser->content_length != CROW_ULLONG_MAX) {
            /* Content-Length header given and non-zero */
            parser->state = s_body_identity;
          } else {
            if (parser->type == HTTP_REQUEST ||
                !http_message_needs_eof(parser)) {
              /* Assume content-length 0 - read the next */
              parser->state = CROW_NEW_MESSAGE();
              CROW_CALLBACK_NOTIFY(message_complete);
            } else {
              /* Read body until EOF */
              parser->state = s_body_identity_eof;
            }
          }
        }

        break;
      }

      case s_body_identity:
      {
        uint64_t to_read = CROW_MIN(parser->content_length,
                               (uint64_t) ((data + len) - p));

        assert(parser->content_length != 0
            && parser->content_length != CROW_ULLONG_MAX);

        /* The difference between advancing content_length and p is because
         * the latter will automaticaly advance on the next loop iteration.
         * Further, if content_length ends up at 0, we want to see the last
         * byte again for our message complete callback.
         */
        CROW_MARK(body);
        parser->content_length -= to_read;
        p += to_read - 1;

        if (parser->content_length == 0) {
          parser->state = s_message_done;

          /* Mimic CROW_CALLBACK_DATA_NOADVANCE() but with one extra byte.
           *
           * The alternative to doing this is to wait for the next byte to
           * trigger the data callback, just as in every other case. The
           * problem with this is that this makes it difficult for the test
           * harness to distinguish between complete-on-EOF and
           * complete-on-length. It's not clear that this distinction is
           * important for applications, but let's keep it for now.
           */
          CROW_CALLBACK_DATA_(body, p - body_mark + 1, p - data);
          goto reexecute_byte;
        }

        break;
      }

      /* read until EOF */
      case s_body_identity_eof:
        CROW_MARK(body);
        p = data + len - 1;

        break;

      case s_message_done:
        parser->state = CROW_NEW_MESSAGE();
        CROW_CALLBACK_NOTIFY(message_complete);
        break;

      case s_chunk_size_start:
      {
        assert(parser->nread == 1);
        assert(parser->flags & F_CHUNKED);

        unhex_val = unhex[(unsigned char)ch];
        if (unhex_val == -1) {
          CROW_SET_ERRNO(HPE_INVALID_CHUNK_SIZE);
          goto error;
        }

        parser->content_length = unhex_val;
        parser->state = s_chunk_size;
        break;
      }

      case s_chunk_size:
      {
        uint64_t t;

        assert(parser->flags & F_CHUNKED);

        if (ch == CROW_CR) {
          parser->state = s_chunk_size_almost_done;
          break;
        }

        unhex_val = unhex[(unsigned char)ch];

        if (unhex_val == -1) {
          if (ch == ';' || ch == ' ') {
            parser->state = s_chunk_parameters;
            break;
          }

          CROW_SET_ERRNO(HPE_INVALID_CHUNK_SIZE);
          goto error;
        }

        t = parser->content_length;
        t *= 16;
        t += unhex_val;

        /* Overflow? Test against a conservative limit for simplicity. */
        if ((CROW_ULLONG_MAX - 16) / 16 < parser->content_length) {
          CROW_SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
          goto error;
        }

        parser->content_length = t;
        break;
      }

      case s_chunk_parameters:
      {
        assert(parser->flags & F_CHUNKED);
        /* just ignore this shit. TODO check for overflow */
        if (ch == CROW_CR) {
          parser->state = s_chunk_size_almost_done;
          break;
        }
        break;
      }

      case s_chunk_size_almost_done:
      {
        assert(parser->flags & F_CHUNKED);
        CROW_STRICT_CHECK(ch != CROW_LF);

        parser->nread = 0;

        if (parser->content_length == 0) {
          parser->flags |= F_TRAILING;
          parser->state = s_header_field_start;
        } else {
          parser->state = s_chunk_data;
        }
        break;
      }

      case s_chunk_data:
      {
        uint64_t to_read = CROW_MIN(parser->content_length,
                               (uint64_t) ((data + len) - p));

        assert(parser->flags & F_CHUNKED);
        assert(parser->content_length != 0
            && parser->content_length != CROW_ULLONG_MAX);

        /* See the explanation in s_body_identity for why the content
         * length and data pointers are managed this way.
         */
        CROW_MARK(body);
        parser->content_length -= to_read;
        p += to_read - 1;

        if (parser->content_length == 0) {
          parser->state = s_chunk_data_almost_done;
        }

        break;
      }

      case s_chunk_data_almost_done:
        assert(parser->flags & F_CHUNKED);
        assert(parser->content_length == 0);
        CROW_STRICT_CHECK(ch != CROW_CR);
        parser->state = s_chunk_data_done;
        CROW_CALLBACK_DATA(body);
        break;

      case s_chunk_data_done:
        assert(parser->flags & F_CHUNKED);
        CROW_STRICT_CHECK(ch != CROW_LF);
        parser->nread = 0;
        parser->state = s_chunk_size_start;
        break;

      default:
        assert(0 && "unhandled state");
        CROW_SET_ERRNO(HPE_INVALID_INTERNAL_STATE);
        goto error;
    }
  }

  /* Run callbacks for any marks that we have leftover after we ran our of
   * bytes. There should be at most one of these set, so it's OK to invoke
   * them in series (unset marks will not result in callbacks).
   *
   * We use the NOADVANCE() variety of callbacks here because 'p' has already
   * overflowed 'data' and this allows us to correct for the off-by-one that
   * we'd otherwise have (since CROW_CALLBACK_DATA() is meant to be run with a 'p'
   * value that's in-bounds).
   */

  assert(((header_field_mark ? 1 : 0) +
          (header_value_mark ? 1 : 0) +
          (url_mark ? 1 : 0)  +
          (body_mark ? 1 : 0) +
          (status_mark ? 1 : 0)) <= 1);

  CROW_CALLBACK_DATA_NOADVANCE(header_field);
  CROW_CALLBACK_DATA_NOADVANCE(header_value);
  CROW_CALLBACK_DATA_NOADVANCE(url);
  CROW_CALLBACK_DATA_NOADVANCE(body);
  CROW_CALLBACK_DATA_NOADVANCE(status);

  return len;

error:
  if (CROW_HTTP_PARSER_ERRNO(parser) == HPE_OK) {
    CROW_SET_ERRNO(HPE_UNKNOWN);
  }

  return (p - data);
}


/* Does the parser need to see an EOF to find the end of the message? */
inline int
http_message_needs_eof (const http_parser *parser)
{
  if (parser->type == HTTP_REQUEST) {
    return 0;
  }

  /* See RFC 2616 section 4.4 */
  if (parser->status_code / 100 == 1 || /* 1xx e.g. Continue */
      parser->status_code == 204 ||     /* No Content */
      parser->status_code == 304 ||     /* Not Modified */
      parser->flags & F_SKIPBODY) {     /* response to a HEAD request */
    return 0;
  }

  if ((parser->flags & F_CHUNKED) || parser->content_length != CROW_ULLONG_MAX) {
    return 0;
  }

  return 1;
}


inline int
http_should_keep_alive (const http_parser *parser)
{
  if (parser->http_major > 0 && parser->http_minor > 0) {
    /* HTTP/1.1 */
    if (parser->flags & F_CONNECTION_CLOSE) {
      return 0;
    }
  } else {
    /* HTTP/1.0 or earlier */
    if (!(parser->flags & F_CONNECTION_KEEP_ALIVE)) {
      return 0;
    }
  }

  return !http_message_needs_eof(parser);
}


inline const char *
http_method_str (enum http_method m)
{
static const char *method_strings[] =
  {
#define CROW_XX(num, name, string) #string,
  CROW_HTTP_METHOD_MAP(CROW_XX)
#undef CROW_XX
  };
  return CROW_ELEM_AT(method_strings, m, "<unknown>");
}


inline void
http_parser_init (http_parser *parser, enum http_parser_type t)
{
  void *data = parser->data; /* preserve application data */
  memset(parser, 0, sizeof(*parser));
  parser->data = data;
  parser->type = t;
  parser->state = (t == HTTP_REQUEST ? s_start_req : (t == HTTP_RESPONSE ? s_start_res : s_start_req_or_res));
  parser->http_errno = HPE_OK;
}

inline const char *
http_errno_name(enum http_errno err) {
/* Map errno values to strings for human-readable output */
#define CROW_HTTP_STRERROR_GEN(n, s) { "HPE_" #n, s },
static struct {
  const char *name;
  const char *description;
} http_strerror_tab[] = {
  CROW_HTTP_ERRNO_MAP(CROW_HTTP_STRERROR_GEN)
};
#undef CROW_HTTP_STRERROR_GEN
  assert(err < (sizeof(http_strerror_tab)/sizeof(http_strerror_tab[0])));
  return http_strerror_tab[err].name;
}

inline const char *
http_errno_description(enum http_errno err) {
/* Map errno values to strings for human-readable output */
#define CROW_HTTP_STRERROR_GEN(n, s) { "HPE_" #n, s },
static struct {
  const char *name;
  const char *description;
} http_strerror_tab[] = {
  CROW_HTTP_ERRNO_MAP(CROW_HTTP_STRERROR_GEN)
};
#undef CROW_HTTP_STRERROR_GEN
  assert(err < (sizeof(http_strerror_tab)/sizeof(http_strerror_tab[0])));
  return http_strerror_tab[err].description;
}

inline static enum http_host_state
http_parse_host_char(enum http_host_state s, const char ch) {
  switch(s) {
    case s_http_userinfo:
    case s_http_userinfo_start:
      if (ch == '@') {
        return s_http_host_start;
      }

      if (CROW_IS_USERINFO_CHAR(ch)) {
        return s_http_userinfo;
      }
      break;

    case s_http_host_start:
      if (ch == '[') {
        return s_http_host_v6_start;
      }

      if (CROW_IS_HOST_CHAR(ch)) {
        return s_http_host;
      }

      break;

    case s_http_host:
      if (CROW_IS_HOST_CHAR(ch)) {
        return s_http_host;
      }

    /* FALLTHROUGH */
    case s_http_host_v6_end:
      if (ch == ':') {
        return s_http_host_port_start;
      }

      break;

    case s_http_host_v6:
      if (ch == ']') {
        return s_http_host_v6_end;
      }

    /* FALLTHROUGH */
    case s_http_host_v6_start:
      if (CROW_IS_HEX(ch) || ch == ':' || ch == '.') {
        return s_http_host_v6;
      }

      break;

    case s_http_host_port:
    case s_http_host_port_start:
      if (CROW_IS_NUM(ch)) {
        return s_http_host_port;
      }

      break;

    default:
      break;
  }
  return s_http_host_dead;
}

inline int
http_parse_host(const char * buf, struct http_parser_url *u, int found_at) {
  enum http_host_state s;

  const char *p;
  size_t buflen = u->field_data[UF_HOST].off + u->field_data[UF_HOST].len;

  u->field_data[UF_HOST].len = 0;

  s = found_at ? s_http_userinfo_start : s_http_host_start;

  for (p = buf + u->field_data[UF_HOST].off; p < buf + buflen; p++) {
    enum http_host_state new_s = http_parse_host_char(s, *p);

    if (new_s == s_http_host_dead) {
      return 1;
    }

    switch(new_s) {
      case s_http_host:
        if (s != s_http_host) {
          u->field_data[UF_HOST].off = p - buf;
        }
        u->field_data[UF_HOST].len++;
        break;

      case s_http_host_v6:
        if (s != s_http_host_v6) {
          u->field_data[UF_HOST].off = p - buf;
        }
        u->field_data[UF_HOST].len++;
        break;

      case s_http_host_port:
        if (s != s_http_host_port) {
          u->field_data[UF_PORT].off = p - buf;
          u->field_data[UF_PORT].len = 0;
          u->field_set |= (1 << UF_PORT);
        }
        u->field_data[UF_PORT].len++;
        break;

      case s_http_userinfo:
        if (s != s_http_userinfo) {
          u->field_data[UF_USERINFO].off = p - buf ;
          u->field_data[UF_USERINFO].len = 0;
          u->field_set |= (1 << UF_USERINFO);
        }
        u->field_data[UF_USERINFO].len++;
        break;

      default:
        break;
    }
    s = new_s;
  }

  /* Make sure we don't end somewhere unexpected */
  switch (s) {
    case s_http_host_start:
    case s_http_host_v6_start:
    case s_http_host_v6:
    case s_http_host_port_start:
    case s_http_userinfo:
    case s_http_userinfo_start:
      return 1;
    default:
      break;
  }

  return 0;
}

inline int
http_parser_parse_url(const char *buf, size_t buflen, int is_connect,
                      struct http_parser_url *u)
{
  enum state s;
  const char *p;
  enum http_parser_url_fields uf, old_uf;
  int found_at = 0;

  u->port = u->field_set = 0;
  s = is_connect ? s_req_server_start : s_req_spaces_before_url;
  old_uf = UF_MAX;

  for (p = buf; p < buf + buflen; p++) {
    s = parse_url_char(s, *p);

    /* Figure out the next field that we're operating on */
    switch (s) {
      case s_dead:
        return 1;

      /* Skip delimeters */
      case s_req_schema_slash:
      case s_req_schema_slash_slash:
      case s_req_server_start:
      case s_req_query_string_start:
      case s_req_fragment_start:
        continue;

      case s_req_schema:
        uf = UF_SCHEMA;
        break;

      case s_req_server_with_at:
        found_at = 1;

      /* FALLTROUGH */
      case s_req_server:
        uf = UF_HOST;
        break;

      case s_req_path:
        uf = UF_PATH;
        break;

      case s_req_query_string:
        uf = UF_QUERY;
        break;

      case s_req_fragment:
        uf = UF_FRAGMENT;
        break;

      default:
        assert(!"Unexpected state");
        return 1;
    }

    /* Nothing's changed; soldier on */
    if (uf == old_uf) {
      u->field_data[uf].len++;
      continue;
    }

    u->field_data[uf].off = p - buf;
    u->field_data[uf].len = 1;

    u->field_set |= (1 << uf);
    old_uf = uf;
  }

  /* host must be present if there is a schema */
  /* parsing http:///toto will fail */
  if ((u->field_set & ((1 << UF_SCHEMA) | (1 << UF_HOST))) != 0) {
    if (http_parse_host(buf, u, found_at) != 0) {
      return 1;
    }
  }

  /* CONNECT requests can only contain "hostname:port" */
  if (is_connect && u->field_set != ((1 << UF_HOST)|(1 << UF_PORT))) {
    return 1;
  }

  if (u->field_set & (1 << UF_PORT)) {
    /* Don't bother with endp; we've already validated the string */
    unsigned long v = strtoul(buf + u->field_data[UF_PORT].off, NULL, 10);

    /* Ports have a max value of 2^16 */
    if (v > 0xffff) {
      return 1;
    }

    u->port = (uint16_t) v;
  }

  return 0;
}

inline void
http_parser_pause(http_parser *parser, int paused) {
  /* Users should only be pausing/unpausing a parser that is not in an error
   * state. In non-debug builds, there's not much that we can do about this
   * other than ignore it.
   */
  if (CROW_HTTP_PARSER_ERRNO(parser) == HPE_OK ||
      CROW_HTTP_PARSER_ERRNO(parser) == HPE_PAUSED) {
    CROW_SET_ERRNO((paused) ? HPE_PAUSED : HPE_OK);
  } else {
    assert(0 && "Attempting to pause parser in error state");
  }
}

inline int
http_body_is_final(const struct http_parser *parser) {
    return parser->state == s_message_done;
}

inline unsigned long
http_parser_version(void) {
  return CROW_HTTP_PARSER_VERSION_MAJOR * 0x10000 |
         CROW_HTTP_PARSER_VERSION_MINOR * 0x00100 |
         CROW_HTTP_PARSER_VERSION_PATCH * 0x00001;
}

#undef CROW_HTTP_METHOD_MAP
#undef CROW_HTTP_ERRNO_MAP
#undef CROW_SET_ERRNO
#undef CROW_CALLBACK_NOTIFY_
#undef CROW_CALLBACK_NOTIFY
#undef CROW_CALLBACK_NOTIFY_NOADVANCE
#undef CROW_CALLBACK_DATA_
#undef CROW_CALLBACK_DATA
#undef CROW_CALLBACK_DATA_NOADVANCE
#undef CROW_MARK
#undef CROW_PROXY_CONNECTION
#undef CROW_CONNECTION
#undef CROW_CONTENT_LENGTH
#undef CROW_TRANSFER_ENCODING
#undef CROW_UPGRADE
#undef CROW_CHUNKED
#undef CROW_KEEP_ALIVE
#undef CROW_CLOSE
#undef CROW_PARSING_HEADER
#undef CROW_CR
#undef CROW_LF
#undef CROW_LOWER
#undef CROW_IS_ALPHA
#undef CROW_IS_NUM
#undef CROW_IS_ALPHANUM
#undef CROW_IS_HEX
#undef CROW_IS_MARK
#undef CROW_IS_USERINFO_CHAR
#undef CROW_TOKEN
#undef CROW_IS_URL_CHAR
#undef CROW_IS_HOST_CHAR
#undef CROW_start_state
#undef CROW_STRICT_CHECK
#undef CROW_NEW_MESSAGE

#ifdef __cplusplus
}
#endif
#endif



#pragma once

#include <boost/algorithm/string/predicate.hpp>
#include <boost/functional/hash.hpp>
#include <unordered_map>

namespace crow
{
    struct ci_hash
    {
        size_t operator()(const std::string& key) const
        {
            std::size_t seed = 0;
            std::locale locale;

            for(auto c : key)
            {
                boost::hash_combine(seed, std::toupper(c, locale));
            }

            return seed;
        }
    };

    struct ci_key_eq
    {
        bool operator()(const std::string& l, const std::string& r) const
        {
            return boost::iequals(l, r);
        }
    };

    using ci_map = std::unordered_multimap<std::string, std::string, ci_hash, ci_key_eq>;
}



/* 
 *
 * TinySHA1 - a header only implementation of the SHA1 algorithm in C++. Based
 * on the implementation in boost::uuid::details.
 * 
 * SHA1 Wikipedia Page: http://en.wikipedia.org/wiki/SHA-1
 * 
 * Copyright (c) 2012-22 SAURAV MOHAPATRA <mohaps@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef _TINY_SHA1_HPP_
#define _TINY_SHA1_HPP_
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
namespace sha1
{
	class SHA1
	{
	public:
		typedef uint32_t digest32_t[5];
		typedef uint8_t digest8_t[20];
		inline static uint32_t LeftRotate(uint32_t value, size_t count) {
			return (value << count) ^ (value >> (32-count));
		}
		SHA1(){ reset(); }
		virtual ~SHA1() {}
		SHA1(const SHA1& s) { *this = s; }
		const SHA1& operator = (const SHA1& s) {
			memcpy(m_digest, s.m_digest, 5 * sizeof(uint32_t));
			memcpy(m_block, s.m_block, 64);
			m_blockByteIndex = s.m_blockByteIndex;
			m_byteCount = s.m_byteCount;
			return *this;
		}
		SHA1& reset() {
			m_digest[0] = 0x67452301;
			m_digest[1] = 0xEFCDAB89;
			m_digest[2] = 0x98BADCFE;
			m_digest[3] = 0x10325476;
			m_digest[4] = 0xC3D2E1F0;
			m_blockByteIndex = 0;
			m_byteCount = 0;
			return *this;
		}
		SHA1& processByte(uint8_t octet) {
			this->m_block[this->m_blockByteIndex++] = octet;
			++this->m_byteCount;
			if(m_blockByteIndex == 64) {
				this->m_blockByteIndex = 0;
				processBlock();
			}
			return *this;
		}
		SHA1& processBlock(const void* const start, const void* const end) {
			const uint8_t* begin = static_cast<const uint8_t*>(start);
			const uint8_t* finish = static_cast<const uint8_t*>(end);
			while(begin != finish) {
				processByte(*begin);
				begin++;
			}
			return *this;
		}
		SHA1& processBytes(const void* const data, size_t len) {
			const uint8_t* block = static_cast<const uint8_t*>(data);
			processBlock(block, block + len);
			return *this;
		}
		const uint32_t* getDigest(digest32_t digest) {
			size_t bitCount = this->m_byteCount * 8;
			processByte(0x80);
			if (this->m_blockByteIndex > 56) {
				while (m_blockByteIndex != 0) {
					processByte(0);
				}
				while (m_blockByteIndex < 56) {
					processByte(0);
				}
			} else {
				while (m_blockByteIndex < 56) {
					processByte(0);
				}
			}
			processByte(0);
			processByte(0);
			processByte(0);
			processByte(0);
			processByte( static_cast<unsigned char>((bitCount>>24) & 0xFF));
			processByte( static_cast<unsigned char>((bitCount>>16) & 0xFF));
			processByte( static_cast<unsigned char>((bitCount>>8 ) & 0xFF));
			processByte( static_cast<unsigned char>((bitCount)     & 0xFF));
	
			memcpy(digest, m_digest, 5 * sizeof(uint32_t));
			return digest;
		}
		const uint8_t* getDigestBytes(digest8_t digest) {
			digest32_t d32;
			getDigest(d32);
			size_t di = 0;
			digest[di++] = ((d32[0] >> 24) & 0xFF);
			digest[di++] = ((d32[0] >> 16) & 0xFF);
			digest[di++] = ((d32[0] >> 8) & 0xFF);
			digest[di++] = ((d32[0]) & 0xFF);
			
			digest[di++] = ((d32[1] >> 24) & 0xFF);
			digest[di++] = ((d32[1] >> 16) & 0xFF);
			digest[di++] = ((d32[1] >> 8) & 0xFF);
			digest[di++] = ((d32[1]) & 0xFF);
			
			digest[di++] = ((d32[2] >> 24) & 0xFF);
			digest[di++] = ((d32[2] >> 16) & 0xFF);
			digest[di++] = ((d32[2] >> 8) & 0xFF);
			digest[di++] = ((d32[2]) & 0xFF);
			
			digest[di++] = ((d32[3] >> 24) & 0xFF);
			digest[di++] = ((d32[3] >> 16) & 0xFF);
			digest[di++] = ((d32[3] >> 8) & 0xFF);
			digest[di++] = ((d32[3]) & 0xFF);
			
			digest[di++] = ((d32[4] >> 24) & 0xFF);
			digest[di++] = ((d32[4] >> 16) & 0xFF);
			digest[di++] = ((d32[4] >> 8) & 0xFF);
			digest[di++] = ((d32[4]) & 0xFF);
			return digest;
		}
	
	protected:
		void processBlock() {
			uint32_t w[80];
			for (size_t i = 0; i < 16; i++) {
				w[i]  = (m_block[i*4 + 0] << 24);
				w[i] |= (m_block[i*4 + 1] << 16);
				w[i] |= (m_block[i*4 + 2] << 8);
				w[i] |= (m_block[i*4 + 3]);
			}
			for (size_t i = 16; i < 80; i++) {
				w[i] = LeftRotate((w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16]), 1);
			}
	
			uint32_t a = m_digest[0];
			uint32_t b = m_digest[1];
			uint32_t c = m_digest[2];
			uint32_t d = m_digest[3];
			uint32_t e = m_digest[4];
	
			for (std::size_t i=0; i<80; ++i) {
				uint32_t f = 0;
				uint32_t k = 0;
	
				if (i<20) {
					f = (b & c) | (~b & d);
					k = 0x5A827999;
				} else if (i<40) {
					f = b ^ c ^ d;
					k = 0x6ED9EBA1;
				} else if (i<60) {
					f = (b & c) | (b & d) | (c & d);
					k = 0x8F1BBCDC;
				} else {
					f = b ^ c ^ d;
					k = 0xCA62C1D6;
				}
				uint32_t temp = LeftRotate(a, 5) + f + e + k + w[i];
				e = d;
				d = c;
				c = LeftRotate(b, 30);
				b = a;
				a = temp;
			}
	
			m_digest[0] += a;
			m_digest[1] += b;
			m_digest[2] += c;
			m_digest[3] += d;
			m_digest[4] += e;
		}
	private:
		digest32_t m_digest;
		uint8_t m_block[64];
		size_t m_blockByteIndex;
		size_t m_byteCount;
	};
}
#endif



#pragma once
// settings for crow
// TODO - replace with runtime config. libucl?

/* #ifdef - enables debug mode */
#define CROW_ENABLE_DEBUG

/* #ifdef - enables logging */
#define CROW_ENABLE_LOGGING

/* #ifdef - enables ssl */
//#define CROW_ENABLE_SSL

/* #define - specifies log level */
/*
    Debug       = 0
    Info        = 1
    Warning     = 2
    Error       = 3
    Critical    = 4

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



#pragma once
#include <boost/asio.hpp>
#ifdef CROW_ENABLE_SSL
#include <boost/asio/ssl.hpp>
#endif


namespace crow
{
    using namespace boost;
    using tcp = asio::ip::tcp;

    struct SocketAdaptor
    {
        using context = void;
        SocketAdaptor(boost::asio::io_service& io_service, context*)
            : socket_(io_service)
        {
        }

        boost::asio::io_service& get_io_service()
        {
            return socket_.get_io_service();
        }

        tcp::socket& raw_socket()
        {
            return socket_;
        }

        tcp::socket& socket()
        {
            return socket_;
        }

        tcp::endpoint remote_endpoint()
        {
            return socket_.remote_endpoint();
        }

        bool is_open()
        {
            return socket_.is_open();
        }

        void close()
        {
            socket_.close();
        }

        template <typename F> 
        void start(F f)
        {
            f(boost::system::error_code());
        }

        tcp::socket socket_;
    };

#ifdef CROW_ENABLE_SSL
    struct SSLAdaptor
    {
        using context = boost::asio::ssl::context;
        using ssl_socket_t = boost::asio::ssl::stream<tcp::socket>;
        SSLAdaptor(boost::asio::io_service& io_service, context* ctx)
            : ssl_socket_(new ssl_socket_t(io_service, *ctx))
        {
        }

        boost::asio::ssl::stream<tcp::socket>& socket()
        {
            return *ssl_socket_;
        }

        tcp::socket::lowest_layer_type&
        raw_socket()
        {
            return ssl_socket_->lowest_layer();
        }

        tcp::endpoint remote_endpoint()
        {
            return raw_socket().remote_endpoint();
        }

        bool is_open()
        {
            return raw_socket().is_open();
        }

        void close()
        {
            raw_socket().close();
        }

        boost::asio::io_service& get_io_service()
        {
            return raw_socket().get_io_service();
        }

        template <typename F> 
        void start(F f)
        {
            ssl_socket_->async_handshake(boost::asio::ssl::stream_base::server,
                    [f](const boost::system::error_code& ec) {
                        f(ec);
                    });
        }

        std::unique_ptr<boost::asio::ssl::stream<tcp::socket>> ssl_socket_;
    };
#endif
}



#pragma once

//#define CROW_JSON_NO_ERROR_CHECK

#include <string>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <memory>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/operators.hpp>
#include <vector>




#if defined(__GNUG__) || defined(__clang__)
#define crow_json_likely(x) __builtin_expect(x, 1)
#define crow_json_unlikely(x) __builtin_expect(x, 0)
#else
#define crow_json_likely(x) x
#define crow_json_unlikely(x) x
#endif


namespace crow
{
    namespace mustache
    {
        class template_t;
    }

    namespace json
    {
        inline void escape(const std::string& str, std::string& ret)
        {
            ret.reserve(ret.size() + str.size()+str.size()/4);
            for(char c:str)
            {
                switch(c)
                {
                    case '"': ret += "\\\""; break;
                    case '\\': ret += "\\\\"; break;
                    case '\n': ret += "\\n"; break;
                    case '\b': ret += "\\b"; break;
                    case '\f': ret += "\\f"; break;
                    case '\r': ret += "\\r"; break;
                    case '\t': ret += "\\t"; break;
                    default:
                        if (0 <= c && c < 0x20)
                        {
                            ret += "\\u00";
                            auto to_hex = [](char c)
                            {
                                c = c&0xf;
                                if (c < 10)
                                    return '0' + c;
                                return 'a'+c-10;
                            };
                            ret += to_hex(c/16);
                            ret += to_hex(c%16);
                        }
                        else
                            ret += c;
                        break;
                }
            }
        }
        inline std::string escape(const std::string& str)
        {
            std::string ret;
            escape(str, ret);
            return ret;
        }

        enum class type : char
        {
            Null,
            False,
            True,
            Number,
            String,
            List,
            Object,
        };

        inline const char* get_type_str(type t) {
            switch(t){
                case type::Number: return "Number";
                case type::False: return "False";
                case type::True: return "True";
                case type::List: return "List";
                case type::String: return "String";
                case type::Object: return "Object";
                default: return "Unknown";
            }
        }

        class rvalue;
        rvalue load(const char* data, size_t size);

        namespace detail 
        {

            struct r_string 
                : boost::less_than_comparable<r_string>,
                boost::less_than_comparable<r_string, std::string>,
                boost::equality_comparable<r_string>,
                boost::equality_comparable<r_string, std::string>
            {
                r_string() {};
                r_string(char* s, char* e)
                    : s_(s), e_(e)
                {};
                ~r_string()
                {
                    if (owned_)
                        delete[] s_;
                }

                r_string(const r_string& r)
                {
                    *this = r;
                }

                r_string(r_string&& r)
                {
                    *this = r;
                }

                r_string& operator = (r_string&& r)
                {
                    s_ = r.s_;
                    e_ = r.e_;
                    owned_ = r.owned_;
                    if (r.owned_)
                        r.owned_ = 0;
                    return *this;
                }

                r_string& operator = (const r_string& r)
                {
                    s_ = r.s_;
                    e_ = r.e_;
                    owned_ = 0;
                    return *this;
                }

                operator std::string () const
                {
                    return std::string(s_, e_);
                }


                const char* begin() const { return s_; }
                const char* end() const { return e_; }
                size_t size() const { return end() - begin(); }

                using iterator = const char*;
                using const_iterator = const char*;

                char* s_;
                mutable char* e_;
                uint8_t owned_{0};
                friend std::ostream& operator << (std::ostream& os, const r_string& s)
                {
                    os << (std::string)s;
                    return os;
                }
            private:
                void force(char* s, uint32_t /*length*/)
                {
                    s_ = s;
                    owned_ = 1;
                }
                friend rvalue crow::json::load(const char* data, size_t size);
            };

            inline bool operator < (const r_string& l, const r_string& r)
            {
                return boost::lexicographical_compare(l,r);
            }

            inline bool operator < (const r_string& l, const std::string& r)
            {
                return boost::lexicographical_compare(l,r);
            }

            inline bool operator > (const r_string& l, const std::string& r)
            {
                return boost::lexicographical_compare(r,l);
            }

            inline bool operator == (const r_string& l, const r_string& r)
            {
                return boost::equals(l,r);
            }

            inline bool operator == (const r_string& l, const std::string& r)
            {
                return boost::equals(l,r);
            }
        }

        class rvalue
        {
            static const int cached_bit = 2;
            static const int error_bit = 4;
        public:
            rvalue() noexcept : option_{error_bit} 
            {}
            rvalue(type t) noexcept
                : lsize_{}, lremain_{}, t_{t}
            {}
            rvalue(type t, char* s, char* e)  noexcept
                : start_{s},
                end_{e},
                t_{t}
            {}

            rvalue(const rvalue& r)
            : start_(r.start_),
                end_(r.end_),
                key_(r.key_),
                t_(r.t_),
                option_(r.option_)
            {
                copy_l(r);
            }

            rvalue(rvalue&& r) noexcept
            {
                *this = std::move(r);
            }

            rvalue& operator = (const rvalue& r)
            {
                start_ = r.start_;
                end_ = r.end_;
                key_ = r.key_;
                copy_l(r);
                t_ = r.t_;
                option_ = r.option_;
                return *this;
            }
            rvalue& operator = (rvalue&& r) noexcept
            {
                start_ = r.start_;
                end_ = r.end_;
                key_ = std::move(r.key_);
                l_ = std::move(r.l_);
                lsize_ = r.lsize_;
                lremain_ = r.lremain_;
                t_ = r.t_;
                option_ = r.option_;
                return *this;
            }

            explicit operator bool() const noexcept
            {
                return (option_ & error_bit) == 0;
            }

            explicit operator int64_t() const
            {
                return i();
            }

            explicit operator uint64_t() const
            {
                return u();
            }

            explicit operator int() const
            {
                return (int)i();
            }

            type t() const
            {
#ifndef CROW_JSON_NO_ERROR_CHECK
                if (option_ & error_bit)
                {
                    throw std::runtime_error("invalid json object");
                }
#endif
                return t_;
            }

            int64_t i() const
            {
#ifndef CROW_JSON_NO_ERROR_CHECK
                switch (t()) {
                    case type::Number:
                    case type::String:
                        return boost::lexical_cast<int64_t>(start_, end_-start_);
                    default:
                        const std::string msg = "expected number, got: "
                            + std::string(get_type_str(t()));
                        throw std::runtime_error(msg);
                }
#endif
                return boost::lexical_cast<int64_t>(start_, end_-start_);
            }

            uint64_t u() const
            {
#ifndef CROW_JSON_NO_ERROR_CHECK
                switch (t()) {
                    case type::Number:
                    case type::String:
                        return boost::lexical_cast<uint64_t>(start_, end_-start_);
                    default:
                        throw std::runtime_error(std::string("expected number, got: ") + get_type_str(t()));
                }
#endif
                return boost::lexical_cast<uint64_t>(start_, end_-start_);
            }

            double d() const
            {
#ifndef CROW_JSON_NO_ERROR_CHECK
                if (t() != type::Number)
                    throw std::runtime_error("value is not number");
#endif
                return boost::lexical_cast<double>(start_, end_-start_);
            }

            bool b() const
            {
#ifndef CROW_JSON_NO_ERROR_CHECK
                if (t() != type::True && t() != type::False)
                    throw std::runtime_error("value is not boolean");
#endif
                return t() == type::True;
            }

            void unescape() const
            {
                if (*(start_-1))
                {
                    char* head = start_;
                    char* tail = start_;
                    while(head != end_)
                    {
                        if (*head == '\\')
                        {
                            switch(*++head)
                            {
                                case '"':  *tail++ = '"'; break;
                                case '\\': *tail++ = '\\'; break;
                                case '/':  *tail++ = '/'; break;
                                case 'b':  *tail++ = '\b'; break;
                                case 'f':  *tail++ = '\f'; break;
                                case 'n':  *tail++ = '\n'; break;
                                case 'r':  *tail++ = '\r'; break;
                                case 't':  *tail++ = '\t'; break;
                                case 'u':
                                    {
                                        auto from_hex = [](char c)
                                        {
                                            if (c >= 'a')
                                                return c - 'a' + 10;
                                            if (c >= 'A')
                                                return c - 'A' + 10;
                                            return c - '0';
                                        };
                                        unsigned int code = 
                                            (from_hex(head[1])<<12) + 
                                            (from_hex(head[2])<< 8) + 
                                            (from_hex(head[3])<< 4) + 
                                            from_hex(head[4]);
                                        if (code >= 0x800)
                                        {
                                            *tail++ = 0xE0 | (code >> 12);
                                            *tail++ = 0x80 | ((code >> 6) & 0x3F);
                                            *tail++ = 0x80 | (code & 0x3F);
                                        }
                                        else if (code >= 0x80)
                                        {
                                            *tail++ = 0xC0 | (code >> 6);
                                            *tail++ = 0x80 | (code & 0x3F);
                                        }
                                        else
                                        {
                                            *tail++ = code;
                                        }
                                        head += 4;
                                    }
                                    break;
                            }
                        }
                        else
                            *tail++ = *head;
                        head++;
                    }
                    end_ = tail;
                    *end_ = 0;
                    *(start_-1) = 0;
                }
            }

            detail::r_string s() const
            {
#ifndef CROW_JSON_NO_ERROR_CHECK
                if (t() != type::String)
                    throw std::runtime_error("value is not string");
#endif
                unescape();
                return detail::r_string{start_, end_};
            }

            bool has(const char* str) const
            {
                return has(std::string(str));
            }

            bool has(const std::string& str) const
            {
                struct Pred 
                {
                    bool operator()(const rvalue& l, const rvalue& r) const
                    {
                        return l.key_ < r.key_;
                    };
                    bool operator()(const rvalue& l, const std::string& r) const
                    {
                        return l.key_ < r;
                    };
                    bool operator()(const std::string& l, const rvalue& r) const
                    {
                        return l < r.key_;
                    };
                };
                if (!is_cached())
                {
                    std::sort(begin(), end(), Pred());
                    set_cached();
                }
                auto it = lower_bound(begin(), end(), str, Pred());
                return it != end() && it->key_ == str;
            }

            int count(const std::string& str)
            {
                return has(str) ? 1 : 0;
            }

            rvalue* begin() const 
            { 
#ifndef CROW_JSON_NO_ERROR_CHECK
                if (t() != type::Object && t() != type::List)
                    throw std::runtime_error("value is not a container");
#endif
                return l_.get(); 
            }
            rvalue* end() const 
            { 
#ifndef CROW_JSON_NO_ERROR_CHECK
                if (t() != type::Object && t() != type::List)
                    throw std::runtime_error("value is not a container");
#endif
                return l_.get()+lsize_; 
            }

            const detail::r_string& key() const
            {
                return key_;
            }

            size_t size() const
            {
                if (t() == type::String)
                    return s().size();
#ifndef CROW_JSON_NO_ERROR_CHECK
                if (t() != type::Object && t() != type::List)
                    throw std::runtime_error("value is not a container");
#endif
                return lsize_;
            }

            const rvalue& operator[](int index) const
            {
#ifndef CROW_JSON_NO_ERROR_CHECK
                if (t() != type::List)
                    throw std::runtime_error("value is not a list");
                if (index >= (int)lsize_ || index < 0)
                    throw std::runtime_error("list out of bound");
#endif
                return l_[index];
            }

            const rvalue& operator[](size_t index) const
            {
#ifndef CROW_JSON_NO_ERROR_CHECK
                if (t() != type::List)
                    throw std::runtime_error("value is not a list");
                if (index >= lsize_)
                    throw std::runtime_error("list out of bound");
#endif
                return l_[index];
            }

            const rvalue& operator[](const char* str) const
            {
                return this->operator[](std::string(str));
            }

            const rvalue& operator[](const std::string& str) const
            {
#ifndef CROW_JSON_NO_ERROR_CHECK
                if (t() != type::Object)
                    throw std::runtime_error("value is not an object");
#endif
                struct Pred 
                {
                    bool operator()(const rvalue& l, const rvalue& r) const
                    {
                        return l.key_ < r.key_;
                    };
                    bool operator()(const rvalue& l, const std::string& r) const
                    {
                        return l.key_ < r;
                    };
                    bool operator()(const std::string& l, const rvalue& r) const
                    {
                        return l < r.key_;
                    };
                };
                if (!is_cached())
                {
                    std::sort(begin(), end(), Pred());
                    set_cached();
                }
                auto it = lower_bound(begin(), end(), str, Pred());
                if (it != end() && it->key_ == str)
                    return *it;
#ifndef CROW_JSON_NO_ERROR_CHECK
                throw std::runtime_error("cannot find key");
#else
                static rvalue nullValue;
                return nullValue;
#endif
            }

            void set_error()
            {
                option_|=error_bit;
            }

            bool error() const
            {
                return (option_&error_bit)!=0;
            }
        private:
            bool is_cached() const
            {
                return (option_&cached_bit)!=0;
            }
            void set_cached() const
            {
                option_ |= cached_bit;
            }
            void copy_l(const rvalue& r)
            {
                if (r.t() != type::Object && r.t() != type::List)
                    return;
                lsize_ = r.lsize_;
                lremain_ = 0;
                l_.reset(new rvalue[lsize_]);
                std::copy(r.begin(), r.end(), begin());
            }

            void emplace_back(rvalue&& v)
            {
                if (!lremain_)
                {
                    int new_size = lsize_ + lsize_;
                    if (new_size - lsize_ > 60000)
                        new_size = lsize_ + 60000;
                    if (new_size < 4)
                        new_size = 4;
                    rvalue* p = new rvalue[new_size];
                    rvalue* p2 = p;
                    for(auto& x : *this)
                        *p2++ = std::move(x);
                    l_.reset(p);
                    lremain_ = new_size - lsize_;
                }
                l_[lsize_++] = std::move(v);
                lremain_ --;
            }

            mutable char* start_;
            mutable char* end_;
            detail::r_string key_;
            std::unique_ptr<rvalue[]> l_;
            uint32_t lsize_;
            uint16_t lremain_;
            type t_;
            mutable uint8_t option_{0};

            friend rvalue load_nocopy_internal(char* data, size_t size);
            friend rvalue load(const char* data, size_t size);
            friend std::ostream& operator <<(std::ostream& os, const rvalue& r)
            {
                switch(r.t_)
                {

                case type::Null: os << "null"; break;
                case type::False: os << "false"; break;
                case type::True: os << "true"; break;
                case type::Number: os << r.d(); break;
                case type::String: os << '"' << r.s() << '"'; break;
                case type::List: 
                    {
                        os << '['; 
                        bool first = true;
                        for(auto& x : r)
                        {
                            if (!first)
                                os << ',';
                            first = false;
                            os << x;
                        }
                        os << ']'; 
                    }
                    break;
                case type::Object:
                    {
                        os << '{'; 
                        bool first = true;
                        for(auto& x : r)
                        {
                            if (!first)
                                os << ',';
                            os << '"' << escape(x.key_) << "\":";
                            first = false;
                            os << x;
                        }
                        os << '}'; 
                    }
                    break;
                }
                return os;
            }
        };
        namespace detail {
        }

        inline bool operator == (const rvalue& l, const std::string& r)
        {
            return l.s() == r;
        }

        inline bool operator == (const std::string& l, const rvalue& r)
        {
            return l == r.s();
        }

        inline bool operator != (const rvalue& l, const std::string& r)
        {
            return l.s() != r;
        }

        inline bool operator != (const std::string& l, const rvalue& r)
        {
            return l != r.s();
        }

        inline bool operator == (const rvalue& l, double r)
        {
            return l.d() == r;
        }

        inline bool operator == (double l, const rvalue& r)
        {
            return l == r.d();
        }

        inline bool operator != (const rvalue& l, double r)
        {
            return l.d() != r;
        }

        inline bool operator != (double l, const rvalue& r)
        {
            return l != r.d();
        }


        inline rvalue load_nocopy_internal(char* data, size_t size)
        {
            //static const char* escaped = "\"\\/\b\f\n\r\t";
            struct Parser
            {
                Parser(char* data, size_t /*size*/)
                    : data(data)
                {
                }

                bool consume(char c)
                {
                    if (crow_json_unlikely(*data != c))
                        return false;
                    data++;
                    return true;
                }

                void ws_skip()
                {
                    while(*data == ' ' || *data == '\t' || *data == '\r' || *data == '\n') ++data;
                };

                rvalue decode_string()
                {
                    if (crow_json_unlikely(!consume('"')))
                        return {};
                    char* start = data;
                    uint8_t has_escaping = 0;
                    while(1)
                    {
                        if (crow_json_likely(*data != '"' && *data != '\\' && *data != '\0'))
                        {
                            data ++;
                        }
                        else if (*data == '"')
                        {
                            *data = 0;
                            *(start-1) = has_escaping;
                            data++;
                            return {type::String, start, data-1};
                        }
                        else if (*data == '\\')
                        {
                            has_escaping = 1;
                            data++;
                            switch(*data)
                            {
                                case 'u':
                                    {
                                        auto check = [](char c)
                                        {
                                            return 
                                                ('0' <= c && c <= '9') ||
                                                ('a' <= c && c <= 'f') ||
                                                ('A' <= c && c <= 'F');
                                        };
                                        if (!(check(*(data+1)) && 
                                            check(*(data+2)) && 
                                            check(*(data+3)) && 
                                            check(*(data+4))))
                                            return {};
                                    }
                                    data += 5;
                                    break;
                                case '"':
                                case '\\':
                                case '/':
                                case 'b':
                                case 'f':
                                case 'n':
                                case 'r':
                                case 't':
                                    data ++;
                                    break;
                                default:
                                    return {};
                            }
                        }
                        else
                            return {};
                    }
                    return {};
                }

                rvalue decode_list()
                {
                    rvalue ret(type::List);
                    if (crow_json_unlikely(!consume('[')))
                    {
                        ret.set_error();
                        return ret;
                    }
                    ws_skip();
                    if (crow_json_unlikely(*data == ']'))
                    {
                        data++;
                        return ret;
                    }

                    while(1)
                    {
                        auto v = decode_value();
                        if (crow_json_unlikely(!v))
                        {
                            ret.set_error();
                            break;
                        }
                        ws_skip();
                        ret.emplace_back(std::move(v));
                        if (*data == ']')
                        {
                            data++;
                            break;
                        }
                        if (crow_json_unlikely(!consume(',')))
                        {
                            ret.set_error();
                            break;
                        }
                        ws_skip();
                    }
                    return ret;
                }

                rvalue decode_number()
                {
                    char* start = data;

                    enum NumberParsingState
                    {
                        Minus,
                        AfterMinus,
                        ZeroFirst,
                        Digits,
                        DigitsAfterPoints,
                        E,
                        DigitsAfterE,
                        Invalid,
                    } state{Minus};
                    while(crow_json_likely(state != Invalid))
                    {
                        switch(*data)
                        {
                            case '0':
                                state = (NumberParsingState)"\2\2\7\3\4\6\6"[state];
                                /*if (state == NumberParsingState::Minus || state == NumberParsingState::AfterMinus)
                                {
                                    state = NumberParsingState::ZeroFirst;
                                }
                                else if (state == NumberParsingState::Digits || 
                                    state == NumberParsingState::DigitsAfterE || 
                                    state == NumberParsingState::DigitsAfterPoints)
                                {
                                    // ok; pass
                                }
                                else if (state == NumberParsingState::E)
                                {
                                    state = NumberParsingState::DigitsAfterE;
                                }
                                else
                                    return {};*/
                                break;
                            case '1': case '2': case '3': 
                            case '4': case '5': case '6': 
                            case '7': case '8': case '9':
                                state = (NumberParsingState)"\3\3\7\3\4\6\6"[state];
                                while(*(data+1) >= '0' && *(data+1) <= '9') data++;
                                /*if (state == NumberParsingState::Minus || state == NumberParsingState::AfterMinus)
                                {
                                    state = NumberParsingState::Digits;
                                }
                                else if (state == NumberParsingState::Digits || 
                                    state == NumberParsingState::DigitsAfterE || 
                                    state == NumberParsingState::DigitsAfterPoints)
                                {
                                    // ok; pass
                                }
                                else if (state == NumberParsingState::E)
                                {
                                    state = NumberParsingState::DigitsAfterE;
                                }
                                else
                                    return {};*/
                                break;
                            case '.':
                                state = (NumberParsingState)"\7\7\4\4\7\7\7"[state];
                                /*
                                if (state == NumberParsingState::Digits || state == NumberParsingState::ZeroFirst)
                                {
                                    state = NumberParsingState::DigitsAfterPoints;
                                }
                                else
                                    return {};
                                */
                                break;
                            case '-':
                                state = (NumberParsingState)"\1\7\7\7\7\6\7"[state];
                                /*if (state == NumberParsingState::Minus)
                                {
                                    state = NumberParsingState::AfterMinus;
                                }
                                else if (state == NumberParsingState::E)
                                {
                                    state = NumberParsingState::DigitsAfterE;
                                }
                                else
                                    return {};*/
                                break;
                            case '+':
                                state = (NumberParsingState)"\7\7\7\7\7\6\7"[state];
                                /*if (state == NumberParsingState::E)
                                {
                                    state = NumberParsingState::DigitsAfterE;
                                }
                                else
                                    return {};*/
                                break;
                            case 'e': case 'E':
                                state = (NumberParsingState)"\7\7\7\5\5\7\7"[state];
                                /*if (state == NumberParsingState::Digits || 
                                    state == NumberParsingState::DigitsAfterPoints)
                                {
                                    state = NumberParsingState::E;
                                }
                                else 
                                    return {};*/
                                break;
                            default:
                                if (crow_json_likely(state == NumberParsingState::ZeroFirst || 
                                        state == NumberParsingState::Digits || 
                                        state == NumberParsingState::DigitsAfterPoints || 
                                        state == NumberParsingState::DigitsAfterE))
                                    return {type::Number, start, data};
                                else
                                    return {};
                        }
                        data++;
                    }

                    return {};
                }

                rvalue decode_value()
                {
                    switch(*data)
                    {
                        case '[':
                            return decode_list();
                        case '{':
                            return decode_object();
                        case '"':
                            return decode_string();
                        case 't':
                            if (//e-data >= 4 &&
                                    data[1] == 'r' &&
                                    data[2] == 'u' &&
                                    data[3] == 'e')
                            {
                                data += 4;
                                return {type::True};
                            }
                            else
                                return {};
                        case 'f':
                            if (//e-data >= 5 &&
                                    data[1] == 'a' &&
                                    data[2] == 'l' &&
                                    data[3] == 's' &&
                                    data[4] == 'e')
                            {
                                data += 5;
                                return {type::False};
                            }
                            else
                                return {};
                        case 'n':
                            if (//e-data >= 4 &&
                                    data[1] == 'u' &&
                                    data[2] == 'l' &&
                                    data[3] == 'l')
                            {
                                data += 4;
                                return {type::Null};
                            }
                            else
                                return {};
                        //case '1': case '2': case '3': 
                        //case '4': case '5': case '6': 
                        //case '7': case '8': case '9':
                        //case '0': case '-':
                        default:
                            return decode_number();
                    }
                    return {};
                }

                rvalue decode_object()
                {
                    rvalue ret(type::Object);
                    if (crow_json_unlikely(!consume('{')))
                    {
                        ret.set_error();
                        return ret;
                    }

                    ws_skip();

                    if (crow_json_unlikely(*data == '}'))
                    {
                        data++;
                        return ret;
                    }

                    while(1)
                    {
                        auto t = decode_string();
                        if (crow_json_unlikely(!t))
                        {
                            ret.set_error();
                            break;
                        }

                        ws_skip();
                        if (crow_json_unlikely(!consume(':')))
                        {
                            ret.set_error();
                            break;
                        }

                        // TODO caching key to speed up (flyweight?)
                        auto key = t.s();

                        ws_skip();
                        auto v = decode_value();
                        if (crow_json_unlikely(!v))
                        {
                            ret.set_error();
                            break;
                        }
                        ws_skip();

                        v.key_ = std::move(key);
                        ret.emplace_back(std::move(v));
                        if (crow_json_unlikely(*data == '}'))
                        {
                            data++;
                            break;
                        }
                        if (crow_json_unlikely(!consume(',')))
                        {
                            ret.set_error();
                            break;
                        }
                        ws_skip();
                    }
                    return ret;
                }

                rvalue parse()
                {
                    ws_skip();
                    auto ret = decode_value(); // or decode object?
                    ws_skip();
                    if (ret && *data != '\0')
                        ret.set_error();
                    return ret;
                }

                char* data;
            };
            return Parser(data, size).parse();
        }
        inline rvalue load(const char* data, size_t size)
        {
            char* s = new char[size+1];
            memcpy(s, data, size);
            s[size] = 0;
            auto ret = load_nocopy_internal(s, size);
            if (ret)
                ret.key_.force(s, size);
            else
                delete[] s;
            return ret;
        }

        inline rvalue load(const char* data)
        {
            return load(data, strlen(data));
        }

        inline rvalue load(const std::string& str)
        {
            return load(str.data(), str.size());
        }

        class wvalue
        {
            friend class crow::mustache::template_t;
        public:
            type t() const { return t_; }
        private:
            type t_{type::Null};
            double d {};
            std::string s;
            std::unique_ptr<std::vector<wvalue>> l;
            std::unique_ptr<std::unordered_map<std::string, wvalue>> o;
        public:

            wvalue() {}

            wvalue(const rvalue& r)
            {
                t_ = r.t();
                switch(r.t())
                {
                    case type::Null:
                    case type::False:
                    case type::True:
                        return;
                    case type::Number:
                        d = r.d();
                        return;
                    case type::String:
                        s = r.s();
                        return;
                    case type::List:
                        l = std::unique_ptr<std::vector<wvalue>>(new std::vector<wvalue>{});
                        l->reserve(r.size());
                        for(auto it = r.begin(); it != r.end(); ++it)
                            l->emplace_back(*it);
                        return;
                    case type::Object:
                        o = std::unique_ptr<
                                    std::unordered_map<std::string, wvalue>
                                >(
                                new std::unordered_map<std::string, wvalue>{});
                        for(auto it = r.begin(); it != r.end(); ++it)
                            o->emplace(it->key(), *it);
                        return;
                }
            }

            wvalue(wvalue&& r)
            {
                *this = std::move(r);
            }

            wvalue& operator = (wvalue&& r)
            {
                t_ = r.t_;
                d = r.d;
                s = std::move(r.s);
                l = std::move(r.l);
                o = std::move(r.o);
                return *this;
            }

            void clear()
            {
                t_ = type::Null;
                l.reset();
                o.reset();
            }

            void reset()
            {
                t_ = type::Null;
                l.reset();
                o.reset();
            }

            wvalue& operator = (std::nullptr_t)
            {
                reset();
                return *this;
            }
            wvalue& operator = (bool value)
            {
                reset();
                if (value)
                    t_ = type::True;
                else
                    t_ = type::False;
                return *this;
            }

            wvalue& operator = (double value)
            {
                reset();
                t_ = type::Number;
                d = value;
                return *this;
            }

            wvalue& operator = (unsigned short value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (short value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (long long value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (long value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (int value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (unsigned long long value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (unsigned long value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (unsigned int value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator=(const char* str)
            {
                reset();
                t_ = type::String;
                s = str;
                return *this;
            }

            wvalue& operator=(const std::string& str)
            {
                reset();
                t_ = type::String;
                s = str;
                return *this;
            }

            template <typename T>
            wvalue& operator=(const std::vector<T>& v)
            {
                if (t_ != type::List)
                    reset();
                t_ = type::List;
                if (!l)
                    l = std::unique_ptr<std::vector<wvalue>>(new std::vector<wvalue>{});
                l->clear();
                l->resize(v.size());
                size_t idx = 0;
                for(auto& x:v)
                {
                    (*l)[idx++] = x;
                }
                return *this;
            }

            wvalue& operator[](unsigned index)
            {
                if (t_ != type::List)
                    reset();
                t_ = type::List;
                if (!l)
                    l = std::unique_ptr<std::vector<wvalue>>(new std::vector<wvalue>{});
                if (l->size() < index+1)
                    l->resize(index+1);
                return (*l)[index];
            }

            int count(const std::string& str)
            {
                if (t_ != type::Object)
                    return 0;
                if (!o)
                    return 0;
                return o->count(str);
            }

            wvalue& operator[](const std::string& str)
            {
                if (t_ != type::Object)
                    reset();
                t_ = type::Object;
                if (!o)
                    o = std::unique_ptr<
                                std::unordered_map<std::string, wvalue>
                            >(
                            new std::unordered_map<std::string, wvalue>{});
                return (*o)[str];
            }

            size_t estimate_length() const
            {
                switch(t_)
                {
                    case type::Null: return 4;
                    case type::False: return 5;
                    case type::True: return 4;
                    case type::Number: return 30;
                    case type::String: return 2+s.size()+s.size()/2;
                    case type::List: 
                        {
                            size_t sum{};
                            if (l)
                            {
                                for(auto& x:*l)
                                {
                                    sum += 1;
                                    sum += x.estimate_length();
                                }
                            }
                            return sum+2;
                        }
                    case type::Object:
                        {
                            size_t sum{};
                            if (o)
                            {
                                for(auto& kv:*o)
                                {
                                    sum += 2;
                                    sum += 2+kv.first.size()+kv.first.size()/2;
                                    sum += kv.second.estimate_length();
                                }
                            }
                            return sum+2;
                        }
                }
                return 1;
            }


            friend void dump_internal(const wvalue& v, std::string& out);
            friend std::string dump(const wvalue& v);
        };

        inline void dump_string(const std::string& str, std::string& out)
        {
            out.push_back('"');
            escape(str, out);
            out.push_back('"');
        }
        inline void dump_internal(const wvalue& v, std::string& out)
        {
            switch(v.t_)
            {
                case type::Null: out += "null"; break;
                case type::False: out += "false"; break;
                case type::True: out += "true"; break;
                case type::Number: 
                    {
                        char outbuf[128];
                        sprintf(outbuf, "%g", v.d);
                        out += outbuf;
                    }
                    break;
                case type::String: dump_string(v.s, out); break;
                case type::List: 
                     {
                         out.push_back('[');
                         if (v.l)
                         {
                             bool first = true;
                             for(auto& x:*v.l)
                             {
                                 if (!first)
                                 {
                                     out.push_back(',');
                                 }
                                 first = false;
                                 dump_internal(x, out);
                             }
                         }
                         out.push_back(']');
                     }
                     break;
                case type::Object:
                     {
                         out.push_back('{');
                         if (v.o)
                         {
                             bool first = true;
                             for(auto& kv:*v.o)
                             {
                                 if (!first)
                                 {
                                     out.push_back(',');
                                 }
                                 first = false;
                                 dump_string(kv.first, out);
                                 out.push_back(':');
                                 dump_internal(kv.second, out);
                             }
                         }
                         out.push_back('}');
                     }
                     break;
            }
        }

        inline std::string dump(const wvalue& v)
        {
            std::string ret;
            ret.reserve(v.estimate_length());
            dump_internal(v, ret);
            return ret;
        }

        //std::vector<boost::asio::const_buffer> dump_ref(wvalue& v)
        //{
        //}
    }
}

#undef crow_json_likely
#undef crow_json_unlikely



#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <functional>


namespace crow
{
    namespace mustache
    {
        using context = json::wvalue;

        template_t load(const std::string& filename);

        class invalid_template_exception : public std::exception
        {
            public:
            invalid_template_exception(const std::string& msg)
                : msg("crow::mustache error: " + msg)
            {
            }
            virtual const char* what() const throw()
            {
                return msg.c_str();
            }
            std::string msg;
        };

        enum class ActionType
        {
            Ignore,
            Tag,
            UnescapeTag,
            OpenBlock,
            CloseBlock,
            ElseBlock,
            Partial,
        };

        struct Action
        {
            int start;
            int end;
            int pos;
            ActionType t;
            Action(ActionType t, int start, int end, int pos = 0) 
                : start(start), end(end), pos(pos), t(t)
            {}
        };

        class template_t 
        {
        public:
            template_t(std::string body)
                : body_(std::move(body))
            {
                // {{ {{# {{/ {{^ {{! {{> {{=
                parse();
            }

        private:
            std::string tag_name(const Action& action)
            {
                return body_.substr(action.start, action.end - action.start);
            }
            auto find_context(const std::string& name, const std::vector<context*>& stack)->std::pair<bool, context&>
            {
                if (name == ".")
                {
                    return {true, *stack.back()};
                }
                int dotPosition = name.find(".");
                if (dotPosition == (int)name.npos)
                {
                    for(auto it = stack.rbegin(); it != stack.rend(); ++it)
                    {
                        if ((*it)->t() == json::type::Object)
                        {
                            if ((*it)->count(name))
                                return {true, (**it)[name]};
                        }
                    }
                }
                else
                {
                    std::vector<int> dotPositions;
                    dotPositions.push_back(-1);
                    while(dotPosition != (int)name.npos)
                    {
                        dotPositions.push_back(dotPosition);
                        dotPosition = name.find(".", dotPosition+1);
                    }
                    dotPositions.push_back(name.size());
                    std::vector<std::string> names;
                    names.reserve(dotPositions.size()-1);
                    for(int i = 1; i < (int)dotPositions.size(); i ++)
                        names.emplace_back(name.substr(dotPositions[i-1]+1, dotPositions[i]-dotPositions[i-1]-1));

                    for(auto it = stack.rbegin(); it != stack.rend(); ++it)
                    {
                        context* view = *it;
                        bool found = true;
                        for(auto jt = names.begin(); jt != names.end(); ++jt)
                        {
                            if (view->t() == json::type::Object &&
                                view->count(*jt))
                            {
                                view = &(*view)[*jt];
                            }
                            else
                            {
                                found = false;
                                break;
                            }
                        }
                        if (found)
                            return {true, *view};
                    }

                }

                static json::wvalue empty_str;
                empty_str = "";
                return {false, empty_str};
            }

            void escape(const std::string& in, std::string& out)
            {
                out.reserve(out.size() + in.size());
                for(auto it = in.begin(); it != in.end(); ++it)
                {
                    switch(*it)
                    {
                        case '&': out += "&amp;"; break;
                        case '<': out += "&lt;"; break;
                        case '>': out += "&gt;"; break;
                        case '"': out += "&quot;"; break;
                        case '\'': out += "&#39;"; break;
                        case '/': out += "&#x2F;"; break;
                        default: out += *it; break;
                    }
                }
            }

            void render_internal(int actionBegin, int actionEnd, std::vector<context*>& stack, std::string& out, int indent)
            {
                int current = actionBegin;

                if (indent)
                    out.insert(out.size(), indent, ' ');

                while(current < actionEnd)
                {
                    auto& fragment = fragments_[current];
                    auto& action = actions_[current];
                    render_fragment(fragment, indent, out);
                    switch(action.t)
                    {
                        case ActionType::Ignore:
                            // do nothing
                            break;
                        case ActionType::Partial:
                            {
                                std::string partial_name = tag_name(action);
                                auto partial_templ = load(partial_name);
                                int partial_indent = action.pos;
                                partial_templ.render_internal(0, partial_templ.fragments_.size()-1, stack, out, partial_indent?indent+partial_indent:0);
                            }
                            break;
                        case ActionType::UnescapeTag:
                        case ActionType::Tag:
                            {
                                auto optional_ctx = find_context(tag_name(action), stack);
                                auto& ctx = optional_ctx.second;
                                switch(ctx.t())
                                {
                                    case json::type::Number:
                                        out += json::dump(ctx);
                                        break;
                                    case json::type::String:
                                        if (action.t == ActionType::Tag)
                                            escape(ctx.s, out);
                                        else
                                            out += ctx.s;
                                        break;
                                    default:
                                        throw std::runtime_error("not implemented tag type" + boost::lexical_cast<std::string>((int)ctx.t()));
                                }
                            }
                            break;
                        case ActionType::ElseBlock:
                            {
                                static context nullContext;
                                auto optional_ctx = find_context(tag_name(action), stack);
                                if (!optional_ctx.first)
                                {
                                    stack.emplace_back(&nullContext);
                                    break;
                                }

                                auto& ctx = optional_ctx.second;
                                switch(ctx.t())
                                {
                                    case json::type::List:
                                        if (ctx.l && !ctx.l->empty())
                                            current = action.pos;
                                        else
                                            stack.emplace_back(&nullContext);
                                        break;
                                    case json::type::False:
                                    case json::type::Null:
                                        stack.emplace_back(&nullContext);
                                        break;
                                    default:
                                        current = action.pos;
                                        break;
                                }
                                break;
                            }
                        case ActionType::OpenBlock:
                            {
                                auto optional_ctx = find_context(tag_name(action), stack);
                                if (!optional_ctx.first)
                                {
                                    current = action.pos;
                                    break;
                                }

                                auto& ctx = optional_ctx.second;
                                switch(ctx.t())
                                {
                                    case json::type::List:
                                        if (ctx.l)
                                            for(auto it = ctx.l->begin(); it != ctx.l->end(); ++it)
                                            {
                                                stack.push_back(&*it);
                                                render_internal(current+1, action.pos, stack, out, indent);
                                                stack.pop_back();
                                            }
                                        current = action.pos;
                                        break;
                                    case json::type::Number:
                                    case json::type::String:
                                    case json::type::Object:
                                    case json::type::True:
                                        stack.push_back(&ctx);
                                        break;
                                    case json::type::False:
                                    case json::type::Null:
                                        current = action.pos;
                                        break;
                                    default:
                                        throw std::runtime_error("{{#: not implemented context type: " + boost::lexical_cast<std::string>((int)ctx.t()));
                                        break;
                                }
                                break;
                            }
                        case ActionType::CloseBlock:
                            stack.pop_back();
                            break;
                        default:
                            throw std::runtime_error("not implemented " + boost::lexical_cast<std::string>((int)action.t));
                    }
                    current++;
                }
                auto& fragment = fragments_[actionEnd];
                render_fragment(fragment, indent, out);
            }
            void render_fragment(const std::pair<int, int> fragment, int indent, std::string& out)
            {
                if (indent)
                {
                    for(int i = fragment.first; i < fragment.second; i ++)
                    {
                        out += body_[i];
                        if (body_[i] == '\n' && i+1 != (int)body_.size())
                            out.insert(out.size(), indent, ' ');
                    }
                }
                else
                    out.insert(out.size(), body_, fragment.first, fragment.second-fragment.first);
            }
        public:
            std::string render()
            {
                context empty_ctx;
                std::vector<context*> stack;
                stack.emplace_back(&empty_ctx);

                std::string ret;
                render_internal(0, fragments_.size()-1, stack, ret, 0);
                return ret;
            }
            std::string render(context& ctx)
            {
                std::vector<context*> stack;
                stack.emplace_back(&ctx);

                std::string ret;
                render_internal(0, fragments_.size()-1, stack, ret, 0);
                return ret;
            }

        private:

            void parse()
            {
                std::string tag_open = "{{";
                std::string tag_close = "}}";

                std::vector<int> blockPositions;
                
                size_t current = 0;
                while(1)
                {
                    size_t idx = body_.find(tag_open, current);
                    if (idx == body_.npos)
                    {
                        fragments_.emplace_back(current, body_.size());
                        actions_.emplace_back(ActionType::Ignore, 0, 0);
                        break;
                    }
                    fragments_.emplace_back(current, idx);

                    idx += tag_open.size();
                    size_t endIdx = body_.find(tag_close, idx);
                    if (endIdx == idx)
                    {
                        throw invalid_template_exception("empty tag is not allowed");
                    }
                    if (endIdx == body_.npos)
                    {
                        // error, no matching tag
                        throw invalid_template_exception("not matched opening tag");
                    }
                    current = endIdx + tag_close.size();
                    switch(body_[idx])
                    {
                        case '#':
                            idx++;
                            while(body_[idx] == ' ') idx++;
                            while(body_[endIdx-1] == ' ') endIdx--;
                            blockPositions.emplace_back(actions_.size());
                            actions_.emplace_back(ActionType::OpenBlock, idx, endIdx);
                            break;
                        case '/':
                            idx++;
                            while(body_[idx] == ' ') idx++;
                            while(body_[endIdx-1] == ' ') endIdx--;
                            {
                                auto& matched = actions_[blockPositions.back()];
                                if (body_.compare(idx, endIdx-idx, 
                                        body_, matched.start, matched.end - matched.start) != 0)
                                {
                                    throw invalid_template_exception("not matched {{# {{/ pair: " + 
                                        body_.substr(matched.start, matched.end - matched.start) + ", " + 
                                        body_.substr(idx, endIdx-idx));
                                }
                                matched.pos = actions_.size();
                            }
                            actions_.emplace_back(ActionType::CloseBlock, idx, endIdx, blockPositions.back());
                            blockPositions.pop_back();
                            break;
                        case '^':
                            idx++;
                            while(body_[idx] == ' ') idx++;
                            while(body_[endIdx-1] == ' ') endIdx--;
                            blockPositions.emplace_back(actions_.size());
                            actions_.emplace_back(ActionType::ElseBlock, idx, endIdx);
                            break;
                        case '!':
                            // do nothing action
                            actions_.emplace_back(ActionType::Ignore, idx+1, endIdx);
                            break;
                        case '>': // partial
                            idx++;
                            while(body_[idx] == ' ') idx++;
                            while(body_[endIdx-1] == ' ') endIdx--;
                            actions_.emplace_back(ActionType::Partial, idx, endIdx);
                            break;
                        case '{':
                            if (tag_open != "{{" || tag_close != "}}")
                                throw invalid_template_exception("cannot use triple mustache when delimiter changed");

                            idx ++;
                            if (body_[endIdx+2] != '}')
                            {
                                throw invalid_template_exception("{{{: }}} not matched");
                            }
                            while(body_[idx] == ' ') idx++;
                            while(body_[endIdx-1] == ' ') endIdx--;
                            actions_.emplace_back(ActionType::UnescapeTag, idx, endIdx);
                            current++;
                            break;
                        case '&':
                            idx ++;
                            while(body_[idx] == ' ') idx++;
                            while(body_[endIdx-1] == ' ') endIdx--;
                            actions_.emplace_back(ActionType::UnescapeTag, idx, endIdx);
                            break;
                        case '=':
                            // tag itself is no-op
                            idx ++;
                            actions_.emplace_back(ActionType::Ignore, idx, endIdx);
                            endIdx --;
                            if (body_[endIdx] != '=')
                                throw invalid_template_exception("{{=: not matching = tag: "+body_.substr(idx, endIdx-idx));
                            endIdx --;
                            while(body_[idx] == ' ') idx++;
                            while(body_[endIdx] == ' ') endIdx--;
                            endIdx++;
                            {
                                bool succeeded = false;
                                for(size_t i = idx; i < endIdx; i++)
                                {
                                    if (body_[i] == ' ')
                                    {
                                        tag_open = body_.substr(idx, i-idx);
                                        while(body_[i] == ' ') i++;
                                        tag_close = body_.substr(i, endIdx-i);
                                        if (tag_open.empty())
                                            throw invalid_template_exception("{{=: empty open tag");
                                        if (tag_close.empty())
                                            throw invalid_template_exception("{{=: empty close tag");

                                        if (tag_close.find(" ") != tag_close.npos)
                                            throw invalid_template_exception("{{=: invalid open/close tag: "+tag_open+" " + tag_close);
                                        succeeded = true;
                                        break;
                                    }
                                }
                                if (!succeeded)
                                    throw invalid_template_exception("{{=: cannot find space between new open/close tags");
                            }
                            break;
                        default:
                            // normal tag case;
                            while(body_[idx] == ' ') idx++;
                            while(body_[endIdx-1] == ' ') endIdx--;
                            actions_.emplace_back(ActionType::Tag, idx, endIdx);
                            break;
                    }
                }

                // removing standalones
                for(int i = actions_.size()-2; i >= 0; i --)
                {
                    if (actions_[i].t == ActionType::Tag || actions_[i].t == ActionType::UnescapeTag)
                        continue;
                    auto& fragment_before = fragments_[i];
                    auto& fragment_after = fragments_[i+1];
                    bool is_last_action = i == (int)actions_.size()-2;
                    bool all_space_before = true;
                    int j, k;
                    for(j = fragment_before.second-1;j >= fragment_before.first;j--)
                    {
                        if (body_[j] != ' ')
                        {
                            all_space_before = false;
                            break;
                        }
                    }
                    if (all_space_before && i > 0)
                        continue;
                    if (!all_space_before && body_[j] != '\n')
                        continue;
                    bool all_space_after = true;
                    for(k = fragment_after.first; k < (int)body_.size() && k < fragment_after.second; k ++)
                    {
                        if (body_[k] != ' ')
                        {
                            all_space_after = false;
                            break;
                        }
                    }
                    if (all_space_after && !is_last_action)
                        continue;
                    if (!all_space_after && 
                            !(
                                body_[k] == '\n' 
                            || 
                                (body_[k] == '\r' && 
                                k + 1 < (int)body_.size() && 
                                body_[k+1] == '\n')))
                        continue;
                    if (actions_[i].t == ActionType::Partial)
                    {
                        actions_[i].pos = fragment_before.second - j - 1;
                    }
                    fragment_before.second = j+1;
                    if (!all_space_after)
                    {
                        if (body_[k] == '\n')
                            k++;
                        else 
                            k += 2;
                        fragment_after.first = k;
                    }
                }
            }
            
            std::vector<std::pair<int,int>> fragments_;
            std::vector<Action> actions_;
            std::string body_;
        };

        inline template_t compile(const std::string& body)
        {
            return template_t(body);
        }
        namespace detail
        {
            inline std::string& get_template_base_directory_ref()
            {
                static std::string template_base_directory = "templates";
                return template_base_directory;
            }
        }

        inline std::string default_loader(const std::string& filename)
        {
            std::string path = detail::get_template_base_directory_ref();
            if (!(path.back() == '/' || path.back() == '\\'))
                path += '/';
            path += filename;
            std::ifstream inf(path);
            if (!inf)
                return {};
            return {std::istreambuf_iterator<char>(inf), std::istreambuf_iterator<char>()};
        }

        namespace detail
        {
            inline std::function<std::string (std::string)>& get_loader_ref()
            {
                static std::function<std::string (std::string)> loader = default_loader;
                return loader;
            }
        }

        inline void set_base(const std::string& path)
        {
            auto& base = detail::get_template_base_directory_ref();
            base = path;
            if (base.back() != '\\' && 
                base.back() != '/')
            {
                base += '/';
            }
        }

        inline void set_loader(std::function<std::string(std::string)> loader)
        {
            detail::get_loader_ref() = std::move(loader);
        }

        inline std::string load_text(const std::string& filename)
        {
            return detail::get_loader_ref()(filename);
        }

        inline template_t load(const std::string& filename)
        {
            return compile(detail::get_loader_ref()(filename));
        }
    }
}



#pragma once

#include <string>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>




namespace crow
{
    enum class LogLevel
    {
#ifndef ERROR
        DEBUG = 0,
        INFO,
        WARNING,
        ERROR,
        CRITICAL,
#endif

        Debug = 0,
        Info,
        Warning,
        Error,
        Critical,
    };

    class ILogHandler {
        public:
            virtual void log(std::string message, LogLevel level) = 0;
    };

    class CerrLogHandler : public ILogHandler {
        public:
            void log(std::string message, LogLevel /*level*/) override {
                std::cerr << message;
            }
    };

    class logger {

        private:
            //
            static std::string timestamp()
            {
                char date[32];
                time_t t = time(0);

                tm my_tm;

#ifdef _MSC_VER
                gmtime_s(&my_tm, &t);
#else
                gmtime_r(&t, &my_tm);
#endif

                size_t sz = strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &my_tm);
                return std::string(date, date+sz);
            }

        public:


            logger(std::string prefix, LogLevel level) : level_(level) {
    #ifdef CROW_ENABLE_LOGGING
                    stringstream_ << "(" << timestamp() << ") [" << prefix << "] ";
    #endif

            }
            ~logger() {
    #ifdef CROW_ENABLE_LOGGING
                if(level_ >= get_current_log_level()) {
                    stringstream_ << std::endl;
                    get_handler_ref()->log(stringstream_.str(), level_);
                }
    #endif
            }

            //
            template <typename T>
            logger& operator<<(T const &value) {

    #ifdef CROW_ENABLE_LOGGING
                if(level_ >= get_current_log_level()) {
                    stringstream_ << value;
                }
    #endif
                return *this;
            }

            //
            static void setLogLevel(LogLevel level) {
                get_log_level_ref() = level;
            }

            static void setHandler(ILogHandler* handler) {
                get_handler_ref() = handler;
            }

            static LogLevel get_current_log_level() {
                return get_log_level_ref();
            }

        private:
            //
            static LogLevel& get_log_level_ref()
            {
                static LogLevel current_level = (LogLevel)CROW_LOG_LEVEL;
                return current_level;
            }
            static ILogHandler*& get_handler_ref()
            {
                static CerrLogHandler default_handler;
                static ILogHandler* current_handler = &default_handler;
                return current_handler;
            }

            //
            std::ostringstream stringstream_;
            LogLevel level_;
    };
}

#define CROW_LOG_CRITICAL   \
        if (crow::logger::get_current_log_level() <= crow::LogLevel::Critical) \
            crow::logger("CRITICAL", crow::LogLevel::Critical)
#define CROW_LOG_ERROR      \
        if (crow::logger::get_current_log_level() <= crow::LogLevel::Error) \
            crow::logger("ERROR   ", crow::LogLevel::Error)
#define CROW_LOG_WARNING    \
        if (crow::logger::get_current_log_level() <= crow::LogLevel::Warning) \
            crow::logger("WARNING ", crow::LogLevel::Warning)
#define CROW_LOG_INFO       \
        if (crow::logger::get_current_log_level() <= crow::LogLevel::Info) \
            crow::logger("INFO    ", crow::LogLevel::Info)
#define CROW_LOG_DEBUG      \
        if (crow::logger::get_current_log_level() <= crow::LogLevel::Debug) \
            crow::logger("DEBUG   ", crow::LogLevel::Debug)




#pragma once

#include <boost/asio.hpp>
#include <deque>
#include <functional>
#include <chrono>
#include <thread>




namespace crow
{
    namespace detail 
    {
        // fast timer queue for fixed tick value.
        class dumb_timer_queue
        {
        public:
            using key = std::pair<dumb_timer_queue*, int>;

            void cancel(key& k)
            {
                auto self = k.first;
                k.first = nullptr;
                if (!self)
                    return;

                unsigned int index = (unsigned int)(k.second - self->step_);
                if (index < self->dq_.size())
                    self->dq_[index].second = nullptr;
            }

            key add(std::function<void()> f)
            {
                dq_.emplace_back(std::chrono::steady_clock::now(), std::move(f));
                int ret = step_+dq_.size()-1;

                CROW_LOG_DEBUG << "timer add inside: " << this << ' ' << ret ;
                return {this, ret};
            }

            void process()
            {
                if (!io_service_)
                    return;

                auto now = std::chrono::steady_clock::now();
                while(!dq_.empty())
                {
                    auto& x = dq_.front();
                    if (now - x.first < std::chrono::seconds(tick))
                        break;
                    if (x.second)
                    {
                        CROW_LOG_DEBUG << "timer call: " << this << ' ' << step_;
                        // we know that timer handlers are very simple currenty; call here
                        x.second();
                    }
                    dq_.pop_front();
                    step_++;
                }
            }

            void set_io_service(boost::asio::io_service& io_service)
            {
                io_service_ = &io_service;
            }

            dumb_timer_queue() noexcept
            {
            }

        private:

            int tick{5};
            boost::asio::io_service* io_service_{};
            std::deque<std::pair<decltype(std::chrono::steady_clock::now()), std::function<void()>>> dq_;
            int step_{};
        };
    }
}



#pragma once

#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <cstring>
#include <functional>
#include <string>




namespace crow
{
    namespace black_magic
    {
#ifndef CROW_MSVC_WORKAROUND
        struct OutOfRange
        {
            OutOfRange(unsigned /*pos*/, unsigned /*length*/) {}
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
#endif
        template <typename T> 
        struct parameter_tag
        {
            static const int value = 0;
        };
#define CROW_INTERNAL_PARAMETER_TAG(t, i) \
template <> \
struct parameter_tag<t> \
{ \
    static const int value = i; \
}
        CROW_INTERNAL_PARAMETER_TAG(int, 1);
        CROW_INTERNAL_PARAMETER_TAG(char, 1);
        CROW_INTERNAL_PARAMETER_TAG(short, 1);
        CROW_INTERNAL_PARAMETER_TAG(long, 1);
        CROW_INTERNAL_PARAMETER_TAG(long long, 1);
        CROW_INTERNAL_PARAMETER_TAG(unsigned int, 2);
        CROW_INTERNAL_PARAMETER_TAG(unsigned char, 2);
        CROW_INTERNAL_PARAMETER_TAG(unsigned short, 2);
        CROW_INTERNAL_PARAMETER_TAG(unsigned long, 2);
        CROW_INTERNAL_PARAMETER_TAG(unsigned long long, 2);
        CROW_INTERNAL_PARAMETER_TAG(double, 3);
        CROW_INTERNAL_PARAMETER_TAG(std::string, 4);
#undef CROW_INTERNAL_PARAMETER_TAG
        template <typename ... Args>
        struct compute_parameter_tag_from_args_list;

        template <>
        struct compute_parameter_tag_from_args_list<>
        {
            static const int value = 0;
        };

        template <typename Arg, typename ... Args>
        struct compute_parameter_tag_from_args_list<Arg, Args...>
        {
            static const int sub_value = 
                compute_parameter_tag_from_args_list<Args...>::value;
            static const int value = 
                parameter_tag<typename std::decay<Arg>::type>::value
                ? sub_value* 6 + parameter_tag<typename std::decay<Arg>::type>::value
                : sub_value;
        };

        static inline bool is_parameter_tag_compatible(uint64_t a, uint64_t b)
        {
            if (a == 0)
                return b == 0;
            if (b == 0)
                return a == 0;
            int sa = a%6;
            int sb = a%6;
            if (sa == 5) sa = 4;
            if (sb == 5) sb = 4;
            if (sa != sb)
                return false;
            return is_parameter_tag_compatible(a/6, b/6);
        }

        static inline unsigned find_closing_tag_runtime(const char* s, unsigned p)
        {
            return
                s[p] == 0
                ? throw std::runtime_error("unmatched tag <") :
                s[p] == '>'
                ? p : find_closing_tag_runtime(s, p + 1);
        }
        
        static inline uint64_t get_parameter_tag_runtime(const char* s, unsigned p = 0)
        {
            return
                s[p] == 0
                    ?  0 :
                s[p] == '<' ? (
                    std::strncmp(s+p, "<int>", 5) == 0
                        ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 1 :
                    std::strncmp(s+p, "<uint>", 6) == 0
                        ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 2 :
                    (std::strncmp(s+p, "<float>", 7) == 0 ||
                    std::strncmp(s+p, "<double>", 8) == 0)
                        ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 3 :
                    (std::strncmp(s+p, "<str>", 5) == 0 ||
                    std::strncmp(s+p, "<string>", 8) == 0)
                        ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 4 :
                    std::strncmp(s+p, "<path>", 6) == 0
                        ? get_parameter_tag_runtime(s, find_closing_tag_runtime(s, p)) * 6 + 5 :
                    throw std::runtime_error("invalid parameter type")
                    ) :
                get_parameter_tag_runtime(s, p+1);
        }
#ifndef CROW_MSVC_WORKAROUND
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
#endif

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

        template <typename T>
        struct promote
        {
            using type = T;
        };

#define CROW_INTERNAL_PROMOTE_TYPE(t1, t2) \
        template<> \
        struct promote<t1> \
        {  \
            using type = t2; \
        }

        CROW_INTERNAL_PROMOTE_TYPE(char, int64_t);
        CROW_INTERNAL_PROMOTE_TYPE(short, int64_t);
        CROW_INTERNAL_PROMOTE_TYPE(int, int64_t);
        CROW_INTERNAL_PROMOTE_TYPE(long, int64_t);
        CROW_INTERNAL_PROMOTE_TYPE(long long, int64_t);
        CROW_INTERNAL_PROMOTE_TYPE(unsigned char, uint64_t);
        CROW_INTERNAL_PROMOTE_TYPE(unsigned short, uint64_t);
        CROW_INTERNAL_PROMOTE_TYPE(unsigned int, uint64_t);
        CROW_INTERNAL_PROMOTE_TYPE(unsigned long, uint64_t);
        CROW_INTERNAL_PROMOTE_TYPE(unsigned long long, uint64_t);
        CROW_INTERNAL_PROMOTE_TYPE(float, double);
#undef CROW_INTERNAL_PROMOTE_TYPE

        template <typename T>
        using promote_t = typename promote<T>::type;

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

        template<typename T> 
        struct function_traits;  

#ifndef CROW_MSVC_WORKAROUND
        template<typename T> 
        struct function_traits : public function_traits<decltype(&T::operator())>
        {
            using parent_t = function_traits<decltype(&T::operator())>;
            static const size_t arity = parent_t::arity;
            using result_type = typename parent_t::result_type;
            template <size_t i>
            using arg = typename parent_t::template arg<i>;
        
        };  
#endif

        template<typename ClassType, typename R, typename ...Args> 
        struct function_traits<R(ClassType::*)(Args...) const>
        {
            static const size_t arity = sizeof...(Args);

            typedef R result_type;

            template <size_t i>
            using arg = typename std::tuple_element<i, std::tuple<Args...>>::type;
        };

        template<typename ClassType, typename R, typename ...Args> 
        struct function_traits<R(ClassType::*)(Args...)>
        {
            static const size_t arity = sizeof...(Args);

            typedef R result_type;

            template <size_t i>
            using arg = typename std::tuple_element<i, std::tuple<Args...>>::type;
        };

        template<typename R, typename ...Args> 
        struct function_traits<std::function<R(Args...)>>
        {
            static const size_t arity = sizeof...(Args);

            typedef R result_type;

            template <size_t i>
            using arg = typename std::tuple_element<i, std::tuple<Args...>>::type;
        };

        inline static std::string base64encode(const char* data, size_t size, const char* key = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")
        {
            std::string ret;
            ret.resize((size+2) / 3 * 4);
            auto it = ret.begin();
            while(size >= 3)
            {
                *it++ = key[(((unsigned char)*data)&0xFC)>>2];
                unsigned char h = (((unsigned char)*data++) & 0x03) << 4;
                *it++ = key[h|((((unsigned char)*data)&0xF0)>>4)];
                h = (((unsigned char)*data++) & 0x0F) << 2;
                *it++ = key[h|((((unsigned char)*data)&0xC0)>>6)];
                *it++ = key[((unsigned char)*data++)&0x3F];

                size -= 3;
            }
            if (size == 1)
            {
                *it++ = key[(((unsigned char)*data)&0xFC)>>2];
                unsigned char h = (((unsigned char)*data++) & 0x03) << 4;
                *it++ = key[h];
                *it++ = '=';
                *it++ = '=';
            }
            else if (size == 2)
            {
                *it++ = key[(((unsigned char)*data)&0xFC)>>2];
                unsigned char h = (((unsigned char)*data++) & 0x03) << 4;
                *it++ = key[h|((((unsigned char)*data)&0xF0)>>4)];
                h = (((unsigned char)*data++) & 0x0F) << 2;
                *it++ = key[h];
                *it++ = '=';
            }
            return ret;
        }

        inline static std::string base64encode_urlsafe(const char* data, size_t size)
        {
            return base64encode(data, size, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
        }


    } // namespace utility
}



#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>



namespace crow
{
    enum class HTTPMethod
    {
#ifndef DELETE
        DELETE = 0,
        GET,
        HEAD,
        POST,
        PUT,
        CONNECT,
        OPTIONS,
        TRACE,
#endif

        Delete = 0,
        Get,
        Head,
        Post,
        Put,
        Connect,
        Options,
        Trace,
    };

    inline std::string method_name(HTTPMethod method)
    {
        switch(method)
        {
            case HTTPMethod::Delete:
                return "DELETE";
            case HTTPMethod::Get:
                return "GET";
            case HTTPMethod::Head:
                return "HEAD";
            case HTTPMethod::Post:
                return "POST";
            case HTTPMethod::Put:
                return "PUT";
            case HTTPMethod::Connect:
                return "CONNECT";
            case HTTPMethod::Options:
                return "OPTIONS";
            case HTTPMethod::Trace:
                return "TRACE";
        }
        return "invalid";
    }

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
    inline int64_t routing_params::get<int64_t>(unsigned index) const
    {
        return int_params[index];
    }

    template<>
    inline uint64_t routing_params::get<uint64_t>(unsigned index) const
    {
        return uint_params[index];
    }

    template<>
    inline double routing_params::get<double>(unsigned index) const
    {
        return double_params[index];
    }

    template<>
    inline std::string routing_params::get<std::string>(unsigned index) const
    {
        return string_params[index];
    }
}

#ifndef CROW_MSVC_WORKAROUND
constexpr crow::HTTPMethod operator "" _method(const char* str, size_t /*len*/)
{
    return
        crow::black_magic::is_equ_p(str, "GET", 3) ? crow::HTTPMethod::Get :
        crow::black_magic::is_equ_p(str, "DELETE", 6) ? crow::HTTPMethod::Delete :
        crow::black_magic::is_equ_p(str, "HEAD", 4) ? crow::HTTPMethod::Head :
        crow::black_magic::is_equ_p(str, "POST", 4) ? crow::HTTPMethod::Post :
        crow::black_magic::is_equ_p(str, "PUT", 3) ? crow::HTTPMethod::Put :
        crow::black_magic::is_equ_p(str, "OPTIONS", 7) ? crow::HTTPMethod::Options :
        crow::black_magic::is_equ_p(str, "CONNECT", 7) ? crow::HTTPMethod::Connect :
        crow::black_magic::is_equ_p(str, "TRACE", 5) ? crow::HTTPMethod::Trace :
        throw std::runtime_error("invalid http method");
}
#endif



#pragma once

#include <boost/asio.hpp>








namespace crow
{
    template <typename T>
    inline const std::string& get_header_value(const T& headers, const std::string& key)
    {
        if (headers.count(key))
        {
            return headers.find(key)->second;
        }
        static std::string empty;
        return empty;
    }

	struct DetachHelper;

    struct request
    {
        HTTPMethod method;
        std::string raw_url;
        std::string url;
        query_string url_params;
        ci_map headers;
        std::string body;

        void* middleware_context{};
        boost::asio::io_service* io_service{};

        request()
            : method(HTTPMethod::Get)
        {
        }

        request(HTTPMethod method, std::string raw_url, std::string url, query_string url_params, ci_map headers, std::string body)
            : method(method), raw_url(std::move(raw_url)), url(std::move(url)), url_params(std::move(url_params)), headers(std::move(headers)), body(std::move(body))
        {
        }

        void add_header(std::string key, std::string value)
        {
            headers.emplace(std::move(key), std::move(value));
        }

        const std::string& get_header_value(const std::string& key) const
        {
            return crow::get_header_value(headers, key);
        }

        template<typename CompletionHandler>
        void post(CompletionHandler handler)
        {
            io_service->post(handler);
        }

        template<typename CompletionHandler>
        void dispatch(CompletionHandler handler)
        {
            io_service->dispatch(handler);
        }

    };
}



#pragma once
#include <boost/algorithm/string/predicate.hpp>







namespace crow
{
    namespace websocket
    {
        enum class WebSocketReadState
        {
            MiniHeader,
            Len16,
            Len64,
            Mask,
            Payload,
        };

		struct connection
		{
            virtual void send_binary(const std::string& msg) = 0;
            virtual void send_text(const std::string& msg) = 0;
            virtual void close(const std::string& msg = "quit") = 0;
            virtual ~connection(){}

            void userdata(void* u) { userdata_ = u; }
            void* userdata() { return userdata_; }

        private:
            void* userdata_;
		};

		template <typename Adaptor>
        class Connection : public connection
        {
			public:
				Connection(const crow::request& req, Adaptor&& adaptor, 
						std::function<void(crow::websocket::connection&)> open_handler,
						std::function<void(crow::websocket::connection&, const std::string&, bool)> message_handler,
						std::function<void(crow::websocket::connection&, const std::string&)> close_handler,
						std::function<void(crow::websocket::connection&)> error_handler)
					: adaptor_(std::move(adaptor)), open_handler_(std::move(open_handler)), message_handler_(std::move(message_handler)), close_handler_(std::move(close_handler)), error_handler_(std::move(error_handler))
				{
					if (!boost::iequals(req.get_header_value("upgrade"), "websocket"))
					{
						adaptor.close();
						delete this;
						return;
					}
					// Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
					// Sec-WebSocket-Version: 13
                    std::string magic = req.get_header_value("Sec-WebSocket-Key") +  "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
                    sha1::SHA1 s;
                    s.processBytes(magic.data(), magic.size());
                    uint8_t digest[20];
                    s.getDigestBytes(digest);   
                    start(crow::utility::base64encode((char*)digest, 20));
				}

                template<typename CompletionHandler>
                void dispatch(CompletionHandler handler)
                {
                    adaptor_.get_io_service().dispatch(handler);
                }

                template<typename CompletionHandler>
                void post(CompletionHandler handler)
                {
                    adaptor_.get_io_service().post(handler);
                }

                void send_pong(const std::string& msg)
                {
                    dispatch([this, msg]{
                        char buf[3] = "\x8A\x00";
                        buf[1] += msg.size();
                        write_buffers_.emplace_back(buf, buf+2);
                        write_buffers_.emplace_back(msg);
                        do_write();
                    });
                }

                void send_binary(const std::string& msg) override
                {
                    dispatch([this, msg]{
                        auto header = build_header(2, msg.size());
                        write_buffers_.emplace_back(std::move(header));
                        write_buffers_.emplace_back(msg);
                        do_write();
                    });
                }

                void send_text(const std::string& msg) override
                {
                    dispatch([this, msg]{
                        auto header = build_header(1, msg.size());
                        write_buffers_.emplace_back(std::move(header));
                        write_buffers_.emplace_back(msg);
                        do_write();
                    });
                }

                void close(const std::string& msg) override
                {
                    dispatch([this, msg]{
                        has_sent_close_ = true;
                        if (has_recv_close_ && !is_close_handler_called_)
                        {
                            is_close_handler_called_ = true;
                            if (close_handler_)
                                close_handler_(*this, msg);
                        }
                        auto header = build_header(0x8, msg.size());
                        write_buffers_.emplace_back(std::move(header));
                        write_buffers_.emplace_back(msg);
                        do_write();
                    });
                }

            protected:

                std::string build_header(int opcode, size_t size)
                {
                    char buf[2+8] = "\x80\x00";
                    buf[0] += opcode;
                    if (size < 126)
                    {
                        buf[1] += size;
                        return {buf, buf+2};
                    }
                    else if (size < 0x10000)
                    {
                        buf[1] += 126;
                        *(uint16_t*)(buf+2) = htons((uint16_t)size);
                        return {buf, buf+4};
                    }
                    else
                    {
                        buf[1] += 127;
                        *(uint64_t*)(buf+2) = ((1==htonl(1)) ? (uint64_t)size : ((uint64_t)htonl((size) & 0xFFFFFFFF) << 32) | htonl((size) >> 32));
                        return {buf, buf+10};
                    }
                }

                void start(std::string&& hello)
                {
                    static std::string header = "HTTP/1.1 101 Switching Protocols\r\n"
                        "Upgrade: websocket\r\n"
                        "Connection: Upgrade\r\n"
                        "Sec-WebSocket-Accept: ";
                    static std::string crlf = "\r\n";
                    write_buffers_.emplace_back(header);
                    write_buffers_.emplace_back(std::move(hello));
                    write_buffers_.emplace_back(crlf);
                    write_buffers_.emplace_back(crlf);
                    do_write();
                    if (open_handler_)
                        open_handler_(*this);
                    do_read();
                }

                void do_read()
                {
                    is_reading = true;
                    switch(state_)
                    {
                        case WebSocketReadState::MiniHeader:
                            {
                                //boost::asio::async_read(adaptor_.socket(), boost::asio::buffer(&mini_header_, 1), 
                                adaptor_.socket().async_read_some(boost::asio::buffer(&mini_header_, 2), 
                                    [this](const boost::system::error_code& ec, std::size_t bytes_transferred) 
                                    {
                                        is_reading = false;
                                        mini_header_ = htons(mini_header_);
#ifdef CROW_ENABLE_DEBUG
                                        
                                        if (!ec && bytes_transferred != 2)
                                        {
                                            throw std::runtime_error("WebSocket:MiniHeader:async_read fail:asio bug?");
                                        }
#endif

                                        if (!ec && ((mini_header_ & 0x80) == 0x80))
                                        {
                                            if ((mini_header_ & 0x7f) == 127)
                                            {
                                                state_ = WebSocketReadState::Len64;
                                            }
                                            else if ((mini_header_ & 0x7f) == 126)
                                            {
                                                state_ = WebSocketReadState::Len16;
                                            }
                                            else
                                            {
                                                remaining_length_ = mini_header_ & 0x7f;
                                                state_ = WebSocketReadState::Mask;
                                            }
                                            do_read();
                                        }
                                        else
                                        {
                                            close_connection_ = true;
                                            adaptor_.close();
                                            if (error_handler_)
                                                error_handler_(*this);
                                            check_destroy();
                                        }
                                    });
                            }
                            break;
                        case WebSocketReadState::Len16:
                            {
                                remaining_length_ = 0;
                                boost::asio::async_read(adaptor_.socket(), boost::asio::buffer(&remaining_length_, 2), 
                                    [this](const boost::system::error_code& ec, std::size_t bytes_transferred) 
                                    {
                                        is_reading = false;
                                        remaining_length_ = ntohs(*(uint16_t*)&remaining_length_);
#ifdef CROW_ENABLE_DEBUG
                                        if (!ec && bytes_transferred != 2)
                                        {
                                            throw std::runtime_error("WebSocket:Len16:async_read fail:asio bug?");
                                        }
#endif

                                        if (!ec)
                                        {
                                            state_ = WebSocketReadState::Mask;
                                            do_read();
                                        }
                                        else
                                        {
                                            close_connection_ = true;
                                            adaptor_.close();
                                            if (error_handler_)
                                                error_handler_(*this);
                                            check_destroy();
                                        }
                                    });
                            }
                            break;
                        case WebSocketReadState::Len64:
                            {
                                boost::asio::async_read(adaptor_.socket(), boost::asio::buffer(&remaining_length_, 8), 
                                    [this](const boost::system::error_code& ec, std::size_t bytes_transferred) 
                                    {
                                        is_reading = false;
                                        remaining_length_ = ((1==ntohl(1)) ? (remaining_length_) : ((uint64_t)ntohl((remaining_length_) & 0xFFFFFFFF) << 32) | ntohl((remaining_length_) >> 32));
#ifdef CROW_ENABLE_DEBUG
                                        if (!ec && bytes_transferred != 8)
                                        {
                                            throw std::runtime_error("WebSocket:Len16:async_read fail:asio bug?");
                                        }
#endif

                                        if (!ec)
                                        {
                                            state_ = WebSocketReadState::Mask;
                                            do_read();
                                        }
                                        else
                                        {
                                            close_connection_ = true;
                                            adaptor_.close();
                                            if (error_handler_)
                                                error_handler_(*this);
                                            check_destroy();
                                        }
                                    });
                            }
                            break;
                        case WebSocketReadState::Mask:
                                boost::asio::async_read(adaptor_.socket(), boost::asio::buffer((char*)&mask_, 4), 
                                    [this](const boost::system::error_code& ec, std::size_t bytes_transferred)
                                    {
                                        is_reading = false;
#ifdef CROW_ENABLE_DEBUG
                                        if (!ec && bytes_transferred != 4)
                                        {
                                            throw std::runtime_error("WebSocket:Mask:async_read fail:asio bug?");
                                        }
#endif

                                        if (!ec)
                                        {
                                            state_ = WebSocketReadState::Payload;
                                            do_read();
                                        }
                                        else
                                        {
                                            close_connection_ = true;
                                            if (error_handler_)
                                                error_handler_(*this);
                                            adaptor_.close();
                                        }
                                    });
                            break;
                        case WebSocketReadState::Payload:
                            {
                                size_t to_read = buffer_.size();
                                if (remaining_length_ < to_read)
                                    to_read = remaining_length_;
                                adaptor_.socket().async_read_some( boost::asio::buffer(buffer_, to_read), 
                                    [this](const boost::system::error_code& ec, std::size_t bytes_transferred)
                                    {
                                        is_reading = false;

                                        if (!ec)
                                        {
                                            fragment_.insert(fragment_.end(), buffer_.begin(), buffer_.begin() + bytes_transferred);
                                            remaining_length_ -= bytes_transferred;
                                            if (remaining_length_ == 0)
                                            {
                                                handle_fragment();
                                                state_ = WebSocketReadState::MiniHeader;
                                                do_read();
                                            }
                                        }
                                        else
                                        {
                                            close_connection_ = true;
                                            if (error_handler_)
                                                error_handler_(*this);
                                            adaptor_.close();
                                        }
                                    });
                            }
                            break;
                    }
                }

                bool is_FIN()
                {
                    return mini_header_ & 0x8000;
                }

                int opcode()
                {
                    return (mini_header_ & 0x0f00) >> 8;
                }

                void handle_fragment()
                {
                    for(decltype(fragment_.length()) i = 0; i < fragment_.length(); i ++)
                    {
                        fragment_[i] ^= ((char*)&mask_)[i%4];
                    }
                    switch(opcode())
                    {
                        case 0: // Continuation
                            {
                                message_ += fragment_;
                                if (is_FIN())
                                {
                                    if (message_handler_)
                                        message_handler_(*this, message_, is_binary_);
                                    message_.clear();
                                }
                            }
                        case 1: // Text
                            {
                                is_binary_ = false;
                                message_ += fragment_;
                                if (is_FIN())
                                {
                                    if (message_handler_)
                                        message_handler_(*this, message_, is_binary_);
                                    message_.clear();
                                }
                            }
                            break;
                        case 2: // Binary
                            {
                                is_binary_ = true;
                                message_ += fragment_;
                                if (is_FIN())
                                {
                                    if (message_handler_)
                                        message_handler_(*this, message_, is_binary_);
                                    message_.clear();
                                }
                            }
                            break;
                        case 0x8: // Close
                            {
                                has_recv_close_ = true;
                                if (!has_sent_close_)
                                {
                                    close(fragment_);
                                }
                                else
                                {
                                    adaptor_.close();
                                    close_connection_ = true;
                                    if (!is_close_handler_called_)
                                    {
                                        if (close_handler_)
                                            close_handler_(*this, fragment_);
                                        is_close_handler_called_ = true;
                                    }
                                    check_destroy();
                                }
                            }
                            break;
                        case 0x9: // Ping
                            {
                                send_pong(fragment_);
                            }
                            break;
                        case 0xA: // Pong
                            {
                                pong_received_ = true;
                            }
                            break;
                    }

                    fragment_.clear();
                }

                void do_write()
                {
                    if (sending_buffers_.empty())
                    {
                        sending_buffers_.swap(write_buffers_);
                        std::vector<boost::asio::const_buffer> buffers;
                        buffers.reserve(sending_buffers_.size());
                        for(auto& s:sending_buffers_)
                        {
                            buffers.emplace_back(boost::asio::buffer(s));
                        }
                        boost::asio::async_write(adaptor_.socket(), buffers, 
                            [&](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
                            {
                                sending_buffers_.clear();
                                if (!ec && !close_connection_)
                                {
                                    if (!write_buffers_.empty())
                                        do_write();
                                    if (has_sent_close_)
                                        close_connection_ = true;
                                }
                                else
                                {
                                    close_connection_ = true;
                                    check_destroy();
                                }
                            });
                    }
                }

                void check_destroy()
                {
                    //if (has_sent_close_ && has_recv_close_)
                    if (!is_close_handler_called_)
                        if (close_handler_)
                            close_handler_(*this, "uncleanly");
                    if (sending_buffers_.empty() && !is_reading)
                        delete this;
                }
			private:
				Adaptor adaptor_;

                std::vector<std::string> sending_buffers_;
                std::vector<std::string> write_buffers_;

                boost::array<char, 4096> buffer_;
                bool is_binary_;
                std::string message_;
                std::string fragment_;
                WebSocketReadState state_{WebSocketReadState::MiniHeader};
                uint64_t remaining_length_{0};
                bool close_connection_{false};
                bool is_reading{false};
                uint32_t mask_;
                uint16_t mini_header_;
                bool has_sent_close_{false};
                bool has_recv_close_{false};
                bool error_occured_{false};
                bool pong_received_{false};
                bool is_close_handler_called_{false};

				std::function<void(crow::websocket::connection&)> open_handler_;
				std::function<void(crow::websocket::connection&, const std::string&, bool)> message_handler_;
				std::function<void(crow::websocket::connection&, const std::string&)> close_handler_;
				std::function<void(crow::websocket::connection&)> error_handler_;
        };
    }
}



#pragma once

#include <string>
#include <unordered_map>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <algorithm>






namespace crow
{
    template <typename Handler>
    struct HTTPParser : public http_parser
    {
        static int on_message_begin(http_parser* self_)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            self->clear();
            return 0;
        }
        static int on_url(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            self->raw_url.insert(self->raw_url.end(), at, at+length);
            return 0;
        }
        static int on_header_field(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            switch (self->header_building_state)
            {
                case 0:
                    if (!self->header_value.empty())
                    {
                        self->headers.emplace(std::move(self->header_field), std::move(self->header_value));
                    }
                    self->header_field.assign(at, at+length);
                    self->header_building_state = 1;
                    break;
                case 1:
                    self->header_field.insert(self->header_field.end(), at, at+length);
                    break;
            }
            return 0;
        }
        static int on_header_value(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            switch (self->header_building_state)
            {
                case 0:
                    self->header_value.insert(self->header_value.end(), at, at+length);
                    break;
                case 1:
                    self->header_building_state = 0;
                    self->header_value.assign(at, at+length);
                    break;
            }
            return 0;
        }
        static int on_headers_complete(http_parser* self_)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            if (!self->header_field.empty())
            {
                self->headers.emplace(std::move(self->header_field), std::move(self->header_value));
            }
            self->process_header();
            return 0;
        }
        static int on_body(http_parser* self_, const char* at, size_t length)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);
            self->body.insert(self->body.end(), at, at+length);
            return 0;
        }
        static int on_message_complete(http_parser* self_)
        {
            HTTPParser* self = static_cast<HTTPParser*>(self_);

            // url params
            self->url = self->raw_url.substr(0, self->raw_url.find("?"));
            self->url_params = query_string(self->raw_url);

            self->process_message();
            return 0;
        }
        HTTPParser(Handler* handler) :
            handler_(handler)
        {
            http_parser_init(this, HTTP_REQUEST);
        }

        // return false on error
        bool feed(const char* buffer, int length)
        {
            const static http_parser_settings settings_{
                on_message_begin,
                on_url,
                nullptr,
                on_header_field,
                on_header_value,
                on_headers_complete,
                on_body,
                on_message_complete,
            };

            int nparsed = http_parser_execute(this, &settings_, buffer, length);
            return nparsed == length;
        }

        bool done()
        {
            return feed(nullptr, 0);
        }

        void clear()
        {
            url.clear();
            raw_url.clear();
            header_building_state = 0;
            header_field.clear();
            header_value.clear();
            headers.clear();
            url_params.clear();
            body.clear();
        }

        void process_header()
        {
            handler_->handle_header();
        }

        void process_message()
        {
            handler_->handle();
        }

        request to_request() const
        {
            return request{(HTTPMethod)method, std::move(raw_url), std::move(url), std::move(url_params), std::move(headers), std::move(body)};
        }

		bool is_upgrade() const
		{
			return upgrade;
		}

        bool check_version(int major, int minor) const
        {
            return http_major == major && http_minor == minor;
        }

        std::string raw_url;
        std::string url;

        int header_building_state = 0;
        std::string header_field;
        std::string header_value;
        ci_map headers;
        query_string url_params;
        std::string body;

        Handler* handler_;
    };
}



#pragma once
#include <string>
#include <unordered_map>








namespace crow
{
    template <typename Adaptor, typename Handler, typename ... Middlewares>
    class Connection;
    struct response
    {
        template <typename Adaptor, typename Handler, typename ... Middlewares>
        friend class crow::Connection;

        int code{200};
        std::string body;
        json::wvalue json_value;

        // `headers' stores HTTP headers.
        ci_map headers;

        void set_header(std::string key, std::string value)
        {
            headers.erase(key);
            headers.emplace(std::move(key), std::move(value));
        }
        void add_header(std::string key, std::string value)
        {
            headers.emplace(std::move(key), std::move(value));
        }

        const std::string& get_header_value(const std::string& key)
        {
            return crow::get_header_value(headers, key);
        }


        response() {}
        explicit response(int code) : code(code) {}
        response(std::string body) : body(std::move(body)) {}
        response(json::wvalue&& json_value) : json_value(std::move(json_value))
        {
            json_mode();
        }
        response(int code, std::string body) : code(code), body(std::move(body)) {}
        response(const json::wvalue& json_value) : body(json::dump(json_value))
        {
            json_mode();
        }
        response(int code, const json::wvalue& json_value) : code(code), body(json::dump(json_value))
        {
            json_mode();
        }

        response(response&& r)
        {
            *this = std::move(r);
        }

        response& operator = (const response& r) = delete;

        response& operator = (response&& r) noexcept
        {
            body = std::move(r.body);
            json_value = std::move(r.json_value);
            code = r.code;
            headers = std::move(r.headers);
            completed_ = r.completed_;
            return *this;
        }

        bool is_completed() const noexcept
        {
            return completed_;
        }

        void clear()
        {
            body.clear();
            json_value.clear();
            code = 200;
            headers.clear();
            completed_ = false;
        }

        void write(const std::string& body_part)
        {
            body += body_part;
        }

        void end()
        {
            if (!completed_)
            {
                completed_ = true;

                if (complete_request_handler_)
                {
                    complete_request_handler_();
                }
            }
        }

        void end(const std::string& body_part)
        {
            body += body_part;
            end();
        }

        bool is_alive()
        {
            return is_alive_helper_ && is_alive_helper_();
        }

        private:
            bool completed_{};
            std::function<void()> complete_request_handler_;
            std::function<bool()> is_alive_helper_;

            //In case of a JSON object, set the Content-Type header
            void json_mode()
            {
                set_header("Content-Type", "application/json");
            }
    };
}



#pragma once
#include <boost/algorithm/string/trim.hpp>





namespace crow
{
    // Any middleware requires following 3 members:

    // struct context;
    //      storing data for the middleware; can be read from another middleware or handlers

    // before_handle
    //      called before handling the request.
    //      if res.end() is called, the operation is halted. 
    //      (still call after_handle of this middleware)
    //      2 signatures:
    //      void before_handle(request& req, response& res, context& ctx)
    //          if you only need to access this middlewares context.
    //      template <typename AllContext>
    //      void before_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
    //          you can access another middlewares' context by calling `all_ctx.template get<MW>()'
    //          ctx == all_ctx.template get<CurrentMiddleware>()

    // after_handle
    //      called after handling the request.
    //      void after_handle(request& req, response& res, context& ctx)
    //      template <typename AllContext>
    //      void after_handle(request& req, response& res, context& ctx, AllContext& all_ctx)

    struct CookieParser
    {
        struct context
        {
            std::unordered_map<std::string, std::string> jar;
            std::unordered_map<std::string, std::string> cookies_to_add;

            std::string get_cookie(const std::string& key)
            {
                if (jar.count(key))
                    return jar[key];
                return {};
            }

            void set_cookie(const std::string& key, const std::string& value)
            {
                cookies_to_add.emplace(key, value);
            }
        };

        void before_handle(request& req, response& res, context& ctx)
        {
            int count = req.headers.count("Cookie");
            if (!count)
                return;
            if (count > 1)
            {
                res.code = 400;
                res.end();
                return;
            }
            std::string cookies = req.get_header_value("Cookie");
            size_t pos = 0;
            while(pos < cookies.size())
            {
                size_t pos_equal = cookies.find('=', pos);
                if (pos_equal == cookies.npos)
                    break;
                std::string name = cookies.substr(pos, pos_equal-pos);
                boost::trim(name);
                pos = pos_equal+1;
                while(pos < cookies.size() && cookies[pos] == ' ') pos++;
                if (pos == cookies.size())
                    break;

                std::string value;

                if (cookies[pos] == '"')
                {
                    int dquote_meet_count = 0;
                    pos ++;
                    size_t pos_dquote = pos-1;
                    do
                    {
                        pos_dquote = cookies.find('"', pos_dquote+1);
                        dquote_meet_count ++;
                    } while(pos_dquote < cookies.size() && cookies[pos_dquote-1] == '\\');
                    if (pos_dquote == cookies.npos)
                        break;

                    if (dquote_meet_count == 1)
                        value = cookies.substr(pos, pos_dquote - pos);
                    else
                    {
                        value.clear();
                        value.reserve(pos_dquote-pos);
                        for(size_t p = pos; p < pos_dquote; p++)
                        {
                            // FIXME minimal escaping
                            if (cookies[p] == '\\' && p + 1 < pos_dquote)
                            {
                                p++;
                                if (cookies[p] == '\\' || cookies[p] == '"')
                                    value += cookies[p];
                                else
                                {
                                    value += '\\';
                                    value += cookies[p];
                                }
                            }
                            else
                                value += cookies[p];
                        }
                    }

                    ctx.jar.emplace(std::move(name), std::move(value));
                    pos = cookies.find(";", pos_dquote+1);
                    if (pos == cookies.npos)
                        break;
                    pos++;
                    while(pos < cookies.size() && cookies[pos] == ' ') pos++;
                    if (pos == cookies.size())
                        break;
                }
                else
                {
                    size_t pos_semicolon = cookies.find(';', pos);
                    value = cookies.substr(pos, pos_semicolon - pos);
                    boost::trim(value);
                    ctx.jar.emplace(std::move(name), std::move(value));
                    pos = pos_semicolon;
                    if (pos == cookies.npos)
                        break;
                    pos ++;
                    while(pos < cookies.size() && cookies[pos] == ' ') pos++;
                    if (pos == cookies.size())
                        break;
                }
            }
        }

        void after_handle(request& /*req*/, response& res, context& ctx)
        {
            for(auto& cookie:ctx.cookies_to_add)
            {
                res.add_header("Set-Cookie", cookie.first + "=" + cookie.second);
            }
        }
    };

    /*
    App<CookieParser, AnotherJarMW> app;
    A B C
    A::context
        int aa;

    ctx1 : public A::context
    ctx2 : public ctx1, public B::context
    ctx3 : public ctx2, public C::context

    C depends on A

    C::handle
        context.aaa

    App::context : private CookieParser::contetx, ... 
    {
        jar

    }

    SimpleApp
    */
}



#pragma once

#include <cstdint>
#include <utility>
#include <tuple>
#include <unordered_map>
#include <memory>
#include <boost/lexical_cast.hpp>
#include <vector>














namespace crow
{
    class BaseRule
    {
    public:
        BaseRule(std::string rule)
            : rule_(std::move(rule))
        {
        }

        virtual ~BaseRule()
        {
        }
        
        virtual void validate() = 0;
        std::unique_ptr<BaseRule> upgrade()
		{
			if (rule_to_upgrade_)
				return std::move(rule_to_upgrade_);
			return {};
		}

        virtual void handle(const request&, response&, const routing_params&) = 0;
        virtual void handle_upgrade(const request&, response& res, SocketAdaptor&&) 
		{
			res = response(404);
			res.end();
		}
#ifdef CROW_ENABLE_SSL
        virtual void handle_upgrade(const request&, response& res, SSLAdaptor&&) 
		{
			res = response(404);
			res.end();
		}
#endif

        uint32_t get_methods()
        {
            return methods_;
        }

    protected:
        uint32_t methods_{1<<(int)HTTPMethod::Get};

        std::string rule_;
        std::string name_;

		std::unique_ptr<BaseRule> rule_to_upgrade_;

        friend class Router;
        template <typename T>
        friend struct RuleParameterTraits;
    };


    namespace detail
    {
        namespace routing_handler_call_helper
        {
            template <typename T, int Pos>
            struct call_pair
            {
                using type = T;
                static const int pos = Pos;
            };

            template <typename H1>
            struct call_params
            {
                H1& handler;
                const routing_params& params;
                const request& req;
                response& res;
            };

            template <typename F, int NInt, int NUint, int NDouble, int NString, typename S1, typename S2> 
            struct call
            {
            };

            template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
            struct call<F, NInt, NUint, NDouble, NString, black_magic::S<int64_t, Args1...>, black_magic::S<Args2...>>
            {
                void operator()(F cparams)
                {
                    using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<int64_t, NInt>>;
                    call<F, NInt+1, NUint, NDouble, NString,
                        black_magic::S<Args1...>, pushed>()(cparams);
                }
            };

            template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
            struct call<F, NInt, NUint, NDouble, NString, black_magic::S<uint64_t, Args1...>, black_magic::S<Args2...>>
            {
                void operator()(F cparams)
                {
                    using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<uint64_t, NUint>>;
                    call<F, NInt, NUint+1, NDouble, NString,
                        black_magic::S<Args1...>, pushed>()(cparams);
                }
            };

            template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
            struct call<F, NInt, NUint, NDouble, NString, black_magic::S<double, Args1...>, black_magic::S<Args2...>>
            {
                void operator()(F cparams)
                {
                    using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<double, NDouble>>;
                    call<F, NInt, NUint, NDouble+1, NString,
                        black_magic::S<Args1...>, pushed>()(cparams);
                }
            };

            template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1, typename ... Args2> 
            struct call<F, NInt, NUint, NDouble, NString, black_magic::S<std::string, Args1...>, black_magic::S<Args2...>>
            {
                void operator()(F cparams)
                {
                    using pushed = typename black_magic::S<Args2...>::template push_back<call_pair<std::string, NString>>;
                    call<F, NInt, NUint, NDouble, NString+1,
                        black_magic::S<Args1...>, pushed>()(cparams);
                }
            };

            template <typename F, int NInt, int NUint, int NDouble, int NString, typename ... Args1> 
            struct call<F, NInt, NUint, NDouble, NString, black_magic::S<>, black_magic::S<Args1...>>
            {
                void operator()(F cparams)
                {
                    cparams.handler(
                        cparams.req,
                        cparams.res,
                        cparams.params.template get<typename Args1::type>(Args1::pos)... 
                    );
                }
            };

            template <typename Func, typename ... ArgsWrapped>
            struct Wrapped
            {
                template <typename ... Args>
                void set(Func f, typename std::enable_if<
                    !std::is_same<typename std::tuple_element<0, std::tuple<Args..., void>>::type, const request&>::value
                , int>::type = 0)
                {
                    handler_ = (
#ifdef CROW_CAN_USE_CPP14
                        [f = std::move(f)]
#else
                        [f]
#endif
                        (const request&, response& res, Args... args){
                            res = response(f(args...));
                            res.end();
                        });
                }

                template <typename Req, typename ... Args>
                struct req_handler_wrapper
                {
                    req_handler_wrapper(Func f)
                        : f(std::move(f))
                    {
                    }

                    void operator()(const request& req, response& res, Args... args)
                    {
                        res = response(f(req, args...));
                        res.end();
                    }

                    Func f;
                };

                template <typename ... Args>
                void set(Func f, typename std::enable_if<
                        std::is_same<typename std::tuple_element<0, std::tuple<Args..., void>>::type, const request&>::value &&
                        !std::is_same<typename std::tuple_element<1, std::tuple<Args..., void, void>>::type, response&>::value
                        , int>::type = 0)
                {
                    handler_ = req_handler_wrapper<Args...>(std::move(f));
                    /*handler_ = (
                        [f = std::move(f)]
                        (const request& req, response& res, Args... args){
                             res = response(f(req, args...));
                             res.end();
                        });*/
                }

                template <typename ... Args>
                void set(Func f, typename std::enable_if<
                        std::is_same<typename std::tuple_element<0, std::tuple<Args..., void>>::type, const request&>::value &&
                        std::is_same<typename std::tuple_element<1, std::tuple<Args..., void, void>>::type, response&>::value
                        , int>::type = 0)
                {
                    handler_ = std::move(f);
                }

                template <typename ... Args>
                struct handler_type_helper
                {
                    using type = std::function<void(const crow::request&, crow::response&, Args...)>;
                    using args_type = black_magic::S<typename black_magic::promote_t<Args>...>; 
                };

                template <typename ... Args>
                struct handler_type_helper<const request&, Args...>
                {
                    using type = std::function<void(const crow::request&, crow::response&, Args...)>;
                    using args_type = black_magic::S<typename black_magic::promote_t<Args>...>; 
                };

                template <typename ... Args>
                struct handler_type_helper<const request&, response&, Args...>
                {
                    using type = std::function<void(const crow::request&, crow::response&, Args...)>;
                    using args_type = black_magic::S<typename black_magic::promote_t<Args>...>; 
                };

                typename handler_type_helper<ArgsWrapped...>::type handler_;

                void operator()(const request& req, response& res, const routing_params& params)
                {
                    detail::routing_handler_call_helper::call<
                        detail::routing_handler_call_helper::call_params<
                            decltype(handler_)>,
                        0, 0, 0, 0, 
                        typename handler_type_helper<ArgsWrapped...>::args_type,
                        black_magic::S<>
                    >()(
                        detail::routing_handler_call_helper::call_params<
                            decltype(handler_)>
                        {handler_, params, req, res}
                   );
                }
            };

        }
    }

	class WebSocketRule : public BaseRule
	{
        using self_t = WebSocketRule;
	public:
        WebSocketRule(std::string rule)
            : BaseRule(std::move(rule))
        {
        }

		void validate() override
		{
		}

		void handle(const request&, response& res, const routing_params&) override
		{
			res = response(404);
			res.end();
		}

        void handle_upgrade(const request& req, response&, SocketAdaptor&& adaptor) override 
		{
			new crow::websocket::Connection<SocketAdaptor>(req, std::move(adaptor), open_handler_, message_handler_, close_handler_, error_handler_);
		}
#ifdef CROW_ENABLE_SSL
        void handle_upgrade(const request& req, response&, SSLAdaptor&& adaptor) override
		{
			new crow::websocket::Connection<SSLAdaptor>(req, std::move(adaptor), open_handler_, message_handler_, close_handler_, error_handler_);
		}
#endif

		template <typename Func>
		self_t& onopen(Func f)
		{
			open_handler_ = f;
			return *this;
		}

		template <typename Func>
		self_t& onmessage(Func f)
		{
			message_handler_ = f;
			return *this;
		}

		template <typename Func>
		self_t& onclose(Func f)
		{
			close_handler_ = f;
			return *this;
		}

		template <typename Func>
		self_t& onerror(Func f)
		{
			error_handler_ = f;
			return *this;
		}

	protected:
		std::function<void(crow::websocket::connection&)> open_handler_;
		std::function<void(crow::websocket::connection&, const std::string&, bool)> message_handler_;
		std::function<void(crow::websocket::connection&, const std::string&)> close_handler_;
		std::function<void(crow::websocket::connection&)> error_handler_;
	};

    template <typename T>
    struct RuleParameterTraits
    {
        using self_t = T;
		WebSocketRule& websocket() 
		{
			auto p =new WebSocketRule(((self_t*)this)->rule_);
            ((self_t*)this)->rule_to_upgrade_.reset(p);
			return *p;
		}

        self_t& name(std::string name) noexcept
        {
            ((self_t*)this)->name_ = std::move(name);
            return (self_t&)*this;
        }

        self_t& methods(HTTPMethod method)
        {
            ((self_t*)this)->methods_ = 1 << (int)method;
            return (self_t&)*this;
        }

        template <typename ... MethodArgs>
        self_t& methods(HTTPMethod method, MethodArgs ... args_method)
        {
            methods(args_method...);
            ((self_t*)this)->methods_ |= 1 << (int)method;
            return (self_t&)*this;
        }

    };

    class DynamicRule : public BaseRule, public RuleParameterTraits<DynamicRule>
    {
    public:

        DynamicRule(std::string rule)
            : BaseRule(std::move(rule))
        {
        }

        void validate() override
        {
            if (!erased_handler_)
            {
                throw std::runtime_error(name_ + (!name_.empty() ? ": " : "") + "no handler for url " + rule_);
            }
        }

        void handle(const request& req, response& res, const routing_params& params) override
        {
            erased_handler_(req, res, params);
        }

        template <typename Func>
        void operator()(Func f)
        {
#ifdef CROW_MSVC_WORKAROUND
            using function_t = utility::function_traits<decltype(&Func::operator())>;
#else
            using function_t = utility::function_traits<Func>;
#endif
            erased_handler_ = wrap(std::move(f), black_magic::gen_seq<function_t::arity>());
        }

        // enable_if Arg1 == request && Arg2 == response
        // enable_if Arg1 == request && Arg2 != resposne
        // enable_if Arg1 != request
#ifdef CROW_MSVC_WORKAROUND
        template <typename Func, size_t ... Indices>
#else
        template <typename Func, unsigned ... Indices>
#endif
        std::function<void(const request&, response&, const routing_params&)> 
        wrap(Func f, black_magic::seq<Indices...>)
        {
#ifdef CROW_MSVC_WORKAROUND
            using function_t = utility::function_traits<decltype(&Func::operator())>;
#else
            using function_t = utility::function_traits<Func>;
#endif
            if (!black_magic::is_parameter_tag_compatible(
                black_magic::get_parameter_tag_runtime(rule_.c_str()), 
                black_magic::compute_parameter_tag_from_args_list<
                    typename function_t::template arg<Indices>...>::value))
            {
                throw std::runtime_error("route_dynamic: Handler type is mismatched with URL parameters: " + rule_);
            }
            auto ret = detail::routing_handler_call_helper::Wrapped<Func, typename function_t::template arg<Indices>...>();
            ret.template set<
                typename function_t::template arg<Indices>...
            >(std::move(f));
            return ret;
        }

        template <typename Func>
        void operator()(std::string name, Func&& f)
        {
            name_ = std::move(name);
            (*this).template operator()<Func>(std::forward(f));
        }
    private:
        std::function<void(const request&, response&, const routing_params&)> erased_handler_;

    };

    template <typename ... Args>
    class TaggedRule : public BaseRule, public RuleParameterTraits<TaggedRule<Args...>>
    {
    public:
        using self_t = TaggedRule<Args...>;

        TaggedRule(std::string rule)
            : BaseRule(std::move(rule))
        {
        }

        void validate() override
        {
            if (!handler_)
            {
                throw std::runtime_error(name_ + (!name_.empty() ? ": " : "") + "no handler for url " + rule_);
            }
        }

        template <typename Func>
        typename std::enable_if<black_magic::CallHelper<Func, black_magic::S<Args...>>::value, void>::type
        operator()(Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<Args...>>::value ||
                black_magic::CallHelper<Func, black_magic::S<crow::request, Args...>>::value , 
                "Handler type is mismatched with URL parameters");
            static_assert(!std::is_same<void, decltype(f(std::declval<Args>()...))>::value, 
                "Handler function cannot have void return type; valid return types: string, int, crow::resposne, crow::json::wvalue");

                handler_ = [f = std::move(f)](const request&, response& res, Args ... args){
                    res = response(f(args...));
                    res.end();
                };
        }

        template <typename Func>
        typename std::enable_if<
            !black_magic::CallHelper<Func, black_magic::S<Args...>>::value &&
            black_magic::CallHelper<Func, black_magic::S<crow::request, Args...>>::value, 
            void>::type
        operator()(Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<Args...>>::value ||
                black_magic::CallHelper<Func, black_magic::S<crow::request, Args...>>::value, 
                "Handler type is mismatched with URL parameters");
            static_assert(!std::is_same<void, decltype(f(std::declval<crow::request>(), std::declval<Args>()...))>::value, 
                "Handler function cannot have void return type; valid return types: string, int, crow::resposne, crow::json::wvalue");

                handler_ = [f = std::move(f)](const crow::request& req, crow::response& res, Args ... args){
                    res = response(f(req, args...));
                    res.end();
                };
        }

        template <typename Func>
        typename std::enable_if<
            !black_magic::CallHelper<Func, black_magic::S<Args...>>::value &&
            !black_magic::CallHelper<Func, black_magic::S<crow::request, Args...>>::value, 
            void>::type
        operator()(Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<Args...>>::value ||
                black_magic::CallHelper<Func, black_magic::S<crow::request, Args...>>::value ||
                black_magic::CallHelper<Func, black_magic::S<crow::request, crow::response&, Args...>>::value
                , 
                "Handler type is mismatched with URL parameters");
            static_assert(std::is_same<void, decltype(f(std::declval<crow::request>(), std::declval<crow::response&>(), std::declval<Args>()...))>::value, 
                "Handler function with response argument should have void return type");

                handler_ = std::move(f);
        }

        template <typename Func>
        void operator()(std::string name, Func&& f)
        {
            name_ = std::move(name);
            (*this).template operator()<Func>(std::forward(f));
        }

        void handle(const request& req, response& res, const routing_params& params) override
        {
            detail::routing_handler_call_helper::call<
                detail::routing_handler_call_helper::call_params<
                    decltype(handler_)>, 
                0, 0, 0, 0, 
                black_magic::S<Args...>, 
                black_magic::S<>
            >()(
                detail::routing_handler_call_helper::call_params<
                    decltype(handler_)>
                {handler_, params, req, res}
            );
        }

    private:
        std::function<void(const crow::request&, crow::response&, Args...)> handler_;

    };

    const int RULE_SPECIAL_REDIRECT_SLASH = 1;

    class Trie
    {
    public:
        struct Node
        {
            unsigned rule_index{};
            std::array<unsigned, (int)ParamType::MAX> param_childrens{};
            std::unordered_map<std::string, unsigned> children;

            bool IsSimpleNode() const
            {
                return 
                    !rule_index &&
                    std::all_of(
                        std::begin(param_childrens), 
                        std::end(param_childrens), 
                        [](unsigned x){ return !x; });
            }
        };

        Trie() : nodes_(1)
        {
        }

private:
        void optimizeNode(Node* node)
        {
            for(auto x : node->param_childrens)
            {
                if (!x)
                    continue;
                Node* child = &nodes_[x];
                optimizeNode(child);
            }
            if (node->children.empty())
                return;
            bool mergeWithChild = true;
            for(auto& kv : node->children)
            {
                Node* child = &nodes_[kv.second];
                if (!child->IsSimpleNode())
                {
                    mergeWithChild = false;
                    break;
                }
            }
            if (mergeWithChild)
            {
                decltype(node->children) merged;
                for(auto& kv : node->children)
                {
                    Node* child = &nodes_[kv.second];
                    for(auto& child_kv : child->children)
                    {
                        merged[kv.first + child_kv.first] = child_kv.second;
                    }
                }
                node->children = std::move(merged);
                optimizeNode(node);
            }
            else
            {
                for(auto& kv : node->children)
                {
                    Node* child = &nodes_[kv.second];
                    optimizeNode(child);
                }
            }
        }

        void optimize()
        {
            optimizeNode(head());
        }

public:
        void validate()
        {
            if (!head()->IsSimpleNode())
                throw std::runtime_error("Internal error: Trie header should be simple!");
            optimize();
        }

        std::pair<unsigned, routing_params> find(const std::string& req_url, const Node* node = nullptr, unsigned pos = 0, routing_params* params = nullptr) const
        {
            routing_params empty;
            if (params == nullptr)
                params = &empty;

            unsigned found{};
            routing_params match_params;

            if (node == nullptr)
                node = head();
            if (pos == req_url.size())
                return {node->rule_index, *params};

            auto update_found = [&found, &match_params](std::pair<unsigned, routing_params>& ret)
            {
                if (ret.first && (!found || found > ret.first))
                {
                    found = ret.first;
                    match_params = std::move(ret.second);
                }
            };

            if (node->param_childrens[(int)ParamType::INT])
            {
                char c = req_url[pos];
                if ((c >= '0' && c <= '9') || c == '+' || c == '-')
                {
                    char* eptr;
                    errno = 0;
                    long long int value = strtoll(req_url.data()+pos, &eptr, 10);
                    if (errno != ERANGE && eptr != req_url.data()+pos)
                    {
                        params->int_params.push_back(value);
                        auto ret = find(req_url, &nodes_[node->param_childrens[(int)ParamType::INT]], eptr - req_url.data(), params);
                        update_found(ret);
                        params->int_params.pop_back();
                    }
                }
            }

            if (node->param_childrens[(int)ParamType::UINT])
            {
                char c = req_url[pos];
                if ((c >= '0' && c <= '9') || c == '+')
                {
                    char* eptr;
                    errno = 0;
                    unsigned long long int value = strtoull(req_url.data()+pos, &eptr, 10);
                    if (errno != ERANGE && eptr != req_url.data()+pos)
                    {
                        params->uint_params.push_back(value);
                        auto ret = find(req_url, &nodes_[node->param_childrens[(int)ParamType::UINT]], eptr - req_url.data(), params);
                        update_found(ret);
                        params->uint_params.pop_back();
                    }
                }
            }

            if (node->param_childrens[(int)ParamType::DOUBLE])
            {
                char c = req_url[pos];
                if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.')
                {
                    char* eptr;
                    errno = 0;
                    double value = strtod(req_url.data()+pos, &eptr);
                    if (errno != ERANGE && eptr != req_url.data()+pos)
                    {
                        params->double_params.push_back(value);
                        auto ret = find(req_url, &nodes_[node->param_childrens[(int)ParamType::DOUBLE]], eptr - req_url.data(), params);
                        update_found(ret);
                        params->double_params.pop_back();
                    }
                }
            }

            if (node->param_childrens[(int)ParamType::STRING])
            {
                size_t epos = pos;
                for(; epos < req_url.size(); epos ++)
                {
                    if (req_url[epos] == '/')
                        break;
                }

                if (epos != pos)
                {
                    params->string_params.push_back(req_url.substr(pos, epos-pos));
                    auto ret = find(req_url, &nodes_[node->param_childrens[(int)ParamType::STRING]], epos, params);
                    update_found(ret);
                    params->string_params.pop_back();
                }
            }

            if (node->param_childrens[(int)ParamType::PATH])
            {
                size_t epos = req_url.size();

                if (epos != pos)
                {
                    params->string_params.push_back(req_url.substr(pos, epos-pos));
                    auto ret = find(req_url, &nodes_[node->param_childrens[(int)ParamType::PATH]], epos, params);
                    update_found(ret);
                    params->string_params.pop_back();
                }
            }

            for(auto& kv : node->children)
            {
                const std::string& fragment = kv.first;
                const Node* child = &nodes_[kv.second];

                if (req_url.compare(pos, fragment.size(), fragment) == 0)
                {
                    auto ret = find(req_url, child, pos + fragment.size(), params);
                    update_found(ret);
                }
            }

            return {found, match_params};
        }

        void add(const std::string& url, unsigned rule_index)
        {
            unsigned idx{0};

            for(unsigned i = 0; i < url.size(); i ++)
            {
                char c = url[i];
                if (c == '<')
                {
                    static struct ParamTraits
                    {
                        ParamType type;
                        std::string name;
                    } paramTraits[] =
                    {
                        { ParamType::INT, "<int>" },
                        { ParamType::UINT, "<uint>" },
                        { ParamType::DOUBLE, "<float>" },
                        { ParamType::DOUBLE, "<double>" },
                        { ParamType::STRING, "<str>" },
                        { ParamType::STRING, "<string>" },
                        { ParamType::PATH, "<path>" },
                    };

                    for(auto& x:paramTraits)
                    {
                        if (url.compare(i, x.name.size(), x.name) == 0)
                        {
                            if (!nodes_[idx].param_childrens[(int)x.type])
                            {
                                auto new_node_idx = new_node();
                                nodes_[idx].param_childrens[(int)x.type] = new_node_idx;
                            }
                            idx = nodes_[idx].param_childrens[(int)x.type];
                            i += x.name.size();
                            break;
                        }
                    }

                    i --;
                }
                else
                {
                    std::string piece(&c, 1);
                    if (!nodes_[idx].children.count(piece))
                    {
                        auto new_node_idx = new_node();
                        nodes_[idx].children.emplace(piece, new_node_idx);
                    }
                    idx = nodes_[idx].children[piece];
                }
            }
            if (nodes_[idx].rule_index)
                throw std::runtime_error("handler already exists for " + url);
            nodes_[idx].rule_index = rule_index;
        }
    private:
        void debug_node_print(Node* n, int level)
        {
            for(int i = 0; i < (int)ParamType::MAX; i ++)
            {
                if (n->param_childrens[i])
                {
                    CROW_LOG_DEBUG << std::string(2*level, ' ') /*<< "("<<n->param_childrens[i]<<") "*/;
                    switch((ParamType)i)
                    {
                        case ParamType::INT:
                            CROW_LOG_DEBUG << "<int>";
                            break;
                        case ParamType::UINT:
                            CROW_LOG_DEBUG << "<uint>";
                            break;
                        case ParamType::DOUBLE:
                            CROW_LOG_DEBUG << "<float>";
                            break;
                        case ParamType::STRING:
                            CROW_LOG_DEBUG << "<str>";
                            break;
                        case ParamType::PATH:
                            CROW_LOG_DEBUG << "<path>";
                            break;
                        default:
                            CROW_LOG_DEBUG << "<ERROR>";
                            break;
                    }

                    debug_node_print(&nodes_[n->param_childrens[i]], level+1);
                }
            }
            for(auto& kv : n->children)
            {
                CROW_LOG_DEBUG << std::string(2*level, ' ') /*<< "(" << kv.second << ") "*/ << kv.first;
                debug_node_print(&nodes_[kv.second], level+1);
            }
        }

    public:
        void debug_print()
        {
            debug_node_print(head(), 0);
        }

    private:
        const Node* head() const
        {
            return &nodes_.front();
        }

        Node* head()
        {
            return &nodes_.front();
        }

        unsigned new_node()
        {
            nodes_.resize(nodes_.size()+1);
            return nodes_.size() - 1;
        }

        std::vector<Node> nodes_;
    };

    class Router
    {
    public:
        Router() : rules_(2) 
        {
        }

        DynamicRule& new_rule_dynamic(const std::string& rule)
        {
            auto ruleObject = new DynamicRule(rule);

            internal_add_rule_object(rule, ruleObject);

            return *ruleObject;
        }

        template <uint64_t N>
        typename black_magic::arguments<N>::type::template rebind<TaggedRule>& new_rule_tagged(const std::string& rule)
        {
            using RuleT = typename black_magic::arguments<N>::type::template rebind<TaggedRule>;
            auto ruleObject = new RuleT(rule);

            internal_add_rule_object(rule, ruleObject);

            return *ruleObject;
        }

        void internal_add_rule_object(const std::string& rule, BaseRule* ruleObject)
        {
            rules_.emplace_back(ruleObject);
            trie_.add(rule, rules_.size() - 1);

            // directory case: 
            //   request to `/about' url matches `/about/' rule 
            if (rule.size() > 1 && rule.back() == '/')
            {
                std::string rule_without_trailing_slash = rule;
                rule_without_trailing_slash.pop_back();
                trie_.add(rule_without_trailing_slash, RULE_SPECIAL_REDIRECT_SLASH);
            }
        }

        void validate()
        {
            trie_.validate();
            for(auto& rule:rules_)
            {
                if (rule)
				{
					auto upgraded = rule->upgrade();
					if (upgraded)
						rule = std::move(upgraded);
                    rule->validate();
				}
            }
        }

		template <typename Adaptor> 
		void handle_upgrade(const request& req, response& res, Adaptor&& adaptor)
		{
            auto found = trie_.find(req.url);
            unsigned rule_index = found.first;
            if (!rule_index)
            {
                CROW_LOG_DEBUG << "Cannot match rules " << req.url;
                res = response(404);
                res.end();
                return;
            }

            if (rule_index >= rules_.size())
                throw std::runtime_error("Trie internal structure corrupted!");

            if (rule_index == RULE_SPECIAL_REDIRECT_SLASH)
            {
                CROW_LOG_INFO << "Redirecting to a url with trailing slash: " << req.url;
                res = response(301);

                // TODO absolute url building
                if (req.get_header_value("Host").empty())
                {
                    res.add_header("Location", req.url + "/");
                }
                else
                {
                    res.add_header("Location", "http://" + req.get_header_value("Host") + req.url + "/");
                }
                res.end();
                return;
            }

            if ((rules_[rule_index]->get_methods() & (1<<(uint32_t)req.method)) == 0)
            {
                CROW_LOG_DEBUG << "Rule found but method mismatch: " << req.url << " with " << method_name(req.method) << "(" << (uint32_t)req.method << ") / " << rules_[rule_index]->get_methods();
                res = response(404);
                res.end();
                return;
            }

            CROW_LOG_DEBUG << "Matched rule (upgrade) '" << rules_[rule_index]->rule_ << "' " << (uint32_t)req.method << " / " << rules_[rule_index]->get_methods();

            // any uncaught exceptions become 500s
            try
            {
                rules_[rule_index]->handle_upgrade(req, res, std::move(adaptor));
            }
            catch(std::exception& e)
            {
                CROW_LOG_ERROR << "An uncaught exception occurred: " << e.what();
                res = response(500);
                res.end();
                return;   
            }
            catch(...)
            {
                CROW_LOG_ERROR << "An uncaught exception occurred. The type was unknown so no information was available.";
                res = response(500);
                res.end();
                return;   
            }
		}

        void handle(const request& req, response& res)
        {
            auto found = trie_.find(req.url);

            unsigned rule_index = found.first;

            if (!rule_index)
            {
                CROW_LOG_DEBUG << "Cannot match rules " << req.url;
                res = response(404);
                res.end();
                return;
            }

            if (rule_index >= rules_.size())
                throw std::runtime_error("Trie internal structure corrupted!");

            if (rule_index == RULE_SPECIAL_REDIRECT_SLASH)
            {
                CROW_LOG_INFO << "Redirecting to a url with trailing slash: " << req.url;
                res = response(301);

                // TODO absolute url building
                if (req.get_header_value("Host").empty())
                {
                    res.add_header("Location", req.url + "/");
                }
                else
                {
                    res.add_header("Location", "http://" + req.get_header_value("Host") + req.url + "/");
                }
                res.end();
                return;
            }

            if ((rules_[rule_index]->get_methods() & (1<<(uint32_t)req.method)) == 0)
            {
                CROW_LOG_DEBUG << "Rule found but method mismatch: " << req.url << " with " << method_name(req.method) << "(" << (uint32_t)req.method << ") / " << rules_[rule_index]->get_methods();
                res = response(404);
                res.end();
                return;
            }

            CROW_LOG_DEBUG << "Matched rule '" << rules_[rule_index]->rule_ << "' " << (uint32_t)req.method << " / " << rules_[rule_index]->get_methods();

            // any uncaught exceptions become 500s
            try
            {
                rules_[rule_index]->handle(req, res, found.second);
            }
            catch(std::exception& e)
            {
                CROW_LOG_ERROR << "An uncaught exception occurred: " << e.what();
                res = response(500);
                res.end();
                return;   
            }
            catch(...)
            {
                CROW_LOG_ERROR << "An uncaught exception occurred. The type was unknown so no information was available.";
                res = response(500);
                res.end();
                return;   
            }
        }

        void debug_print()
        {
            trie_.debug_print();
        }

    private:
        std::vector<std::unique_ptr<BaseRule>> rules_;
        Trie trie_;
    };
}



#pragma once








namespace crow
{
    namespace detail
    {
        template <typename ... Middlewares>
        struct partial_context
            : public black_magic::pop_back<Middlewares...>::template rebind<partial_context>
            , public black_magic::last_element_type<Middlewares...>::type::context
        {
            using parent_context = typename black_magic::pop_back<Middlewares...>::template rebind<::crow::detail::partial_context>;
            template <int N>
            using partial = typename std::conditional<N == sizeof...(Middlewares)-1, partial_context, typename parent_context::template partial<N>>::type;

            template <typename T> 
            typename T::context& get()
            {
                return static_cast<typename T::context&>(*this);
            }
        };

        template <>
        struct partial_context<>
        {
            template <int>
            using partial = partial_context;
        };

        template <int N, typename Context, typename Container, typename CurrentMW, typename ... Middlewares>
        bool middleware_call_helper(Container& middlewares, request& req, response& res, Context& ctx);

        template <typename ... Middlewares>
        struct context : private partial_context<Middlewares...>
        //struct context : private Middlewares::context... // simple but less type-safe
        {
            template <int N, typename Context, typename Container>
            friend typename std::enable_if<(N==0)>::type after_handlers_call_helper(Container& middlewares, Context& ctx, request& req, response& res);
            template <int N, typename Context, typename Container>
            friend typename std::enable_if<(N>0)>::type after_handlers_call_helper(Container& middlewares, Context& ctx, request& req, response& res);

            template <int N, typename Context, typename Container, typename CurrentMW, typename ... Middlewares2>
            friend bool middleware_call_helper(Container& middlewares, request& req, response& res, Context& ctx);

            template <typename T> 
            typename T::context& get()
            {
                return static_cast<typename T::context&>(*this);
            }

            template <int N>
            using partial = typename partial_context<Middlewares...>::template partial<N>;
        };
    }
}



#pragma once
#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/array.hpp>
#include <atomic>
#include <chrono>
#include <vector>



















namespace crow
{
    using namespace boost;
    using tcp = asio::ip::tcp;

    namespace detail
    {
        template <typename MW>
        struct check_before_handle_arity_3_const
        {
            template <typename T,
                void (T::*)(request&, response&, typename MW::context&) const = &T::before_handle
            >
            struct get
            { };
        };

        template <typename MW>
        struct check_before_handle_arity_3
        {
            template <typename T,
                void (T::*)(request&, response&, typename MW::context&) = &T::before_handle
            >
            struct get
            { };
        };

        template <typename MW>
        struct check_after_handle_arity_3_const
        {
            template <typename T,
                void (T::*)(request&, response&, typename MW::context&) const = &T::after_handle
            >
            struct get
            { };
        };

        template <typename MW>
        struct check_after_handle_arity_3
        {
            template <typename T,
                void (T::*)(request&, response&, typename MW::context&) = &T::after_handle
            >
            struct get
            { };
        };

        template <typename T>
        struct is_before_handle_arity_3_impl
        {
            template <typename C>
            static std::true_type f(typename check_before_handle_arity_3_const<T>::template get<C>*);

            template <typename C>
            static std::true_type f(typename check_before_handle_arity_3<T>::template get<C>*);

            template <typename C>
            static std::false_type f(...);

        public:
            static const bool value = decltype(f<T>(nullptr))::value;
        };

        template <typename T>
        struct is_after_handle_arity_3_impl
        {
            template <typename C>
            static std::true_type f(typename check_after_handle_arity_3_const<T>::template get<C>*);

            template <typename C>
            static std::true_type f(typename check_after_handle_arity_3<T>::template get<C>*);

            template <typename C>
            static std::false_type f(...);

        public:
            static const bool value = decltype(f<T>(nullptr))::value;
        };

        template <typename MW, typename Context, typename ParentContext>
        typename std::enable_if<!is_before_handle_arity_3_impl<MW>::value>::type
        before_handler_call(MW& mw, request& req, response& res, Context& ctx, ParentContext& /*parent_ctx*/)
        {
            mw.before_handle(req, res, ctx.template get<MW>(), ctx);
        }

        template <typename MW, typename Context, typename ParentContext>
        typename std::enable_if<is_before_handle_arity_3_impl<MW>::value>::type
        before_handler_call(MW& mw, request& req, response& res, Context& ctx, ParentContext& /*parent_ctx*/)
        {
            mw.before_handle(req, res, ctx.template get<MW>());
        }

        template <typename MW, typename Context, typename ParentContext>
        typename std::enable_if<!is_after_handle_arity_3_impl<MW>::value>::type
        after_handler_call(MW& mw, request& req, response& res, Context& ctx, ParentContext& /*parent_ctx*/)
        {
            mw.after_handle(req, res, ctx.template get<MW>(), ctx);
        }

        template <typename MW, typename Context, typename ParentContext>
        typename std::enable_if<is_after_handle_arity_3_impl<MW>::value>::type
        after_handler_call(MW& mw, request& req, response& res, Context& ctx, ParentContext& /*parent_ctx*/)
        {
            mw.after_handle(req, res, ctx.template get<MW>());
        }

        template <int N, typename Context, typename Container, typename CurrentMW, typename ... Middlewares>
        bool middleware_call_helper(Container& middlewares, request& req, response& res, Context& ctx)
        {
            using parent_context_t = typename Context::template partial<N-1>;
            before_handler_call<CurrentMW, Context, parent_context_t>(std::get<N>(middlewares), req, res, ctx, static_cast<parent_context_t&>(ctx));

            if (res.is_completed())
            {
                after_handler_call<CurrentMW, Context, parent_context_t>(std::get<N>(middlewares), req, res, ctx, static_cast<parent_context_t&>(ctx));
                return true;
            }

            if (middleware_call_helper<N+1, Context, Container, Middlewares...>(middlewares, req, res, ctx))
            {
                after_handler_call<CurrentMW, Context, parent_context_t>(std::get<N>(middlewares), req, res, ctx, static_cast<parent_context_t&>(ctx));
                return true;
            }

            return false;
        }

        template <int N, typename Context, typename Container>
        bool middleware_call_helper(Container& /*middlewares*/, request& /*req*/, response& /*res*/, Context& /*ctx*/)
        {
            return false;
        }

        template <int N, typename Context, typename Container>
        typename std::enable_if<(N<0)>::type 
        after_handlers_call_helper(Container& /*middlewares*/, Context& /*context*/, request& /*req*/, response& /*res*/)
        {
        }

        template <int N, typename Context, typename Container>
        typename std::enable_if<(N==0)>::type after_handlers_call_helper(Container& middlewares, Context& ctx, request& req, response& res)
        {
            using parent_context_t = typename Context::template partial<N-1>;
            using CurrentMW = typename std::tuple_element<N, typename std::remove_reference<Container>::type>::type;
            after_handler_call<CurrentMW, Context, parent_context_t>(std::get<N>(middlewares), req, res, ctx, static_cast<parent_context_t&>(ctx));
        }

        template <int N, typename Context, typename Container>
        typename std::enable_if<(N>0)>::type after_handlers_call_helper(Container& middlewares, Context& ctx, request& req, response& res)
        {
            using parent_context_t = typename Context::template partial<N-1>;
            using CurrentMW = typename std::tuple_element<N, typename std::remove_reference<Container>::type>::type;
            after_handler_call<CurrentMW, Context, parent_context_t>(std::get<N>(middlewares), req, res, ctx, static_cast<parent_context_t&>(ctx));
            after_handlers_call_helper<N-1, Context, Container>(middlewares, ctx, req, res);
        }
    }

#ifdef CROW_ENABLE_DEBUG
    static int connectionCount;
#endif
    template <typename Adaptor, typename Handler, typename ... Middlewares>
    class Connection
    {
    public:
        Connection(
            boost::asio::io_service& io_service, 
            Handler* handler, 
            const std::string& server_name,
            std::tuple<Middlewares...>* middlewares,
            std::function<std::string()>& get_cached_date_str_f,
            detail::dumb_timer_queue& timer_queue,
            typename Adaptor::context* adaptor_ctx_
            ) 
            : adaptor_(io_service, adaptor_ctx_), 
            handler_(handler), 
            parser_(this), 
            server_name_(server_name),
            middlewares_(middlewares),
            get_cached_date_str(get_cached_date_str_f),
            timer_queue(timer_queue)
        {
#ifdef CROW_ENABLE_DEBUG
            connectionCount ++;
            CROW_LOG_DEBUG << "Connection open, total " << connectionCount << ", " << this;
#endif
        }
        
        ~Connection()
        {
            res.complete_request_handler_ = nullptr;
            cancel_deadline_timer();
#ifdef CROW_ENABLE_DEBUG
            connectionCount --;
            CROW_LOG_DEBUG << "Connection closed, total " << connectionCount << ", " << this;
#endif
        }

        decltype(std::declval<Adaptor>().raw_socket())& socket()
        {
            return adaptor_.raw_socket();
        }

        void start()
        {
            adaptor_.start([this](const boost::system::error_code& ec) {
                if (!ec)
                {
                    start_deadline();

                    do_read();
                }
                else
                {
                    check_destroy();
                }
            });
        }

        void handle_header()
        {
            // HTTP 1.1 Expect: 100-continue
            if (parser_.check_version(1, 1) && parser_.headers.count("expect") && get_header_value(parser_.headers, "expect") == "100-continue")
            {
                buffers_.clear();
                static std::string expect_100_continue = "HTTP/1.1 100 Continue\r\n\r\n";
                buffers_.emplace_back(expect_100_continue.data(), expect_100_continue.size());
                do_write();
            }
        }

        void handle()
        {
            cancel_deadline_timer();
            bool is_invalid_request = false;
            add_keep_alive_ = false;

            req_ = std::move(parser_.to_request());
            request& req = req_;

            if (parser_.check_version(1, 0))
            {
                // HTTP/1.0
                if (req.headers.count("connection"))
                {
                    if (boost::iequals(req.get_header_value("connection"),"Keep-Alive"))
                        add_keep_alive_ = true;
                }
                else
                    close_connection_ = true;
            }
            else if (parser_.check_version(1, 1))
            {
                // HTTP/1.1
                if (req.headers.count("connection"))
                {
                    if (req.get_header_value("connection") == "close")
                        close_connection_ = true;
                    else if (boost::iequals(req.get_header_value("connection"),"Keep-Alive"))
                        add_keep_alive_ = true;
                }
                if (!req.headers.count("host"))
                {
                    is_invalid_request = true;
                    res = response(400);
                }
				if (parser_.is_upgrade())
				{
					if (req.get_header_value("upgrade") == "h2c")
					{
						// TODO HTTP/2
                        // currently, ignore upgrade header
					}
                    else
                    {
                        close_connection_ = true;
                        handler_->handle_upgrade(req, res, std::move(adaptor_));
                        return;
                    }
				}
            }

            CROW_LOG_INFO << "Request: " << boost::lexical_cast<std::string>(adaptor_.remote_endpoint()) << " " << this << " HTTP/" << parser_.http_major << "." << parser_.http_minor << ' '
             << method_name(req.method) << " " << req.url;


            need_to_call_after_handlers_ = false;
            if (!is_invalid_request)
            {
                res.complete_request_handler_ = []{};
                res.is_alive_helper_ = [this]()->bool{ return adaptor_.is_open(); };

                ctx_ = detail::context<Middlewares...>();
                req.middleware_context = (void*)&ctx_;
                req.io_service = &adaptor_.get_io_service();
                detail::middleware_call_helper<0, decltype(ctx_), decltype(*middlewares_), Middlewares...>(*middlewares_, req, res, ctx_);

                if (!res.completed_)
                {
                    res.complete_request_handler_ = [this]{ this->complete_request(); };
                    need_to_call_after_handlers_ = true;
                    handler_->handle(req, res);
                    if (add_keep_alive_)
                        res.set_header("connection", "Keep-Alive");
                }
                else
                {
                    complete_request();
                }
            }
            else
            {
                complete_request();
            }
        }

        void complete_request()
        {
            CROW_LOG_INFO << "Response: " << this << ' ' << req_.raw_url << ' ' << res.code << ' ' << close_connection_;

            if (need_to_call_after_handlers_)
            {
                need_to_call_after_handlers_ = false;

                // call all after_handler of middlewares
                detail::after_handlers_call_helper<
                    ((int)sizeof...(Middlewares)-1),
                    decltype(ctx_),
                    decltype(*middlewares_)> 
                (*middlewares_, ctx_, req_, res);
            }

            //auto self = this->shared_from_this();
            res.complete_request_handler_ = nullptr;
            
            if (!adaptor_.is_open())
            {
                //CROW_LOG_DEBUG << this << " delete (socket is closed) " << is_reading << ' ' << is_writing;
                //delete this;
                return;
            }

            static std::unordered_map<int, std::string> statusCodes = {
                {200, "HTTP/1.1 200 OK\r\n"},
                {201, "HTTP/1.1 201 Created\r\n"},
                {202, "HTTP/1.1 202 Accepted\r\n"},
                {204, "HTTP/1.1 204 No Content\r\n"},

                {300, "HTTP/1.1 300 Multiple Choices\r\n"},
                {301, "HTTP/1.1 301 Moved Permanently\r\n"},
                {302, "HTTP/1.1 302 Moved Temporarily\r\n"},
                {304, "HTTP/1.1 304 Not Modified\r\n"},

                {400, "HTTP/1.1 400 Bad Request\r\n"},
                {401, "HTTP/1.1 401 Unauthorized\r\n"},
                {403, "HTTP/1.1 403 Forbidden\r\n"},
                {404, "HTTP/1.1 404 Not Found\r\n"},

                {500, "HTTP/1.1 500 Internal Server Error\r\n"},
                {501, "HTTP/1.1 501 Not Implemented\r\n"},
                {502, "HTTP/1.1 502 Bad Gateway\r\n"},
                {503, "HTTP/1.1 503 Service Unavailable\r\n"},
            };

            static std::string seperator = ": ";
            static std::string crlf = "\r\n";

            buffers_.clear();
            buffers_.reserve(4*(res.headers.size()+5)+3);

            if (res.body.empty() && res.json_value.t() == json::type::Object)
            {
                res.body = json::dump(res.json_value);
            }

            if (!statusCodes.count(res.code))
                res.code = 500;
            {
                auto& status = statusCodes.find(res.code)->second;
                buffers_.emplace_back(status.data(), status.size());
            }

            if (res.code >= 400 && res.body.empty())
                res.body = statusCodes[res.code].substr(9);

            for(auto& kv : res.headers)
            {
                buffers_.emplace_back(kv.first.data(), kv.first.size());
                buffers_.emplace_back(seperator.data(), seperator.size());
                buffers_.emplace_back(kv.second.data(), kv.second.size());
                buffers_.emplace_back(crlf.data(), crlf.size());

            }

            if (!res.headers.count("content-length"))
            {
                content_length_ = std::to_string(res.body.size());
                static std::string content_length_tag = "Content-Length: ";
                buffers_.emplace_back(content_length_tag.data(), content_length_tag.size());
                buffers_.emplace_back(content_length_.data(), content_length_.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
            }
            if (!res.headers.count("server"))
            {
                static std::string server_tag = "Server: ";
                buffers_.emplace_back(server_tag.data(), server_tag.size());
                buffers_.emplace_back(server_name_.data(), server_name_.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
            }
            if (!res.headers.count("date"))
            {
                static std::string date_tag = "Date: ";
                date_str_ = get_cached_date_str();
                buffers_.emplace_back(date_tag.data(), date_tag.size());
                buffers_.emplace_back(date_str_.data(), date_str_.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
            }
            if (add_keep_alive_)
            {
                static std::string keep_alive_tag = "Connection: Keep-Alive";
                buffers_.emplace_back(keep_alive_tag.data(), keep_alive_tag.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
            }

            buffers_.emplace_back(crlf.data(), crlf.size());
            res_body_copy_.swap(res.body);
            buffers_.emplace_back(res_body_copy_.data(), res_body_copy_.size());

            do_write();

            if (need_to_start_read_after_complete_)
            {
                need_to_start_read_after_complete_ = false;
                start_deadline();
                do_read();
            }
        }

    private:
        void do_read()
        {
            //auto self = this->shared_from_this();
            is_reading = true;
            adaptor_.socket().async_read_some(boost::asio::buffer(buffer_), 
                [this](const boost::system::error_code& ec, std::size_t bytes_transferred)
                {
                    bool error_while_reading = true;
                    if (!ec)
                    {
                        bool ret = parser_.feed(buffer_.data(), bytes_transferred);
                        if (ret && adaptor_.is_open())
                        {
                            error_while_reading = false;
                        }
                    }

                    if (error_while_reading)
                    {
                        cancel_deadline_timer();
                        parser_.done();
                        adaptor_.close();
                        is_reading = false;
                        CROW_LOG_DEBUG << this << " from read(1)";
                        check_destroy();
                    }
                    else if (close_connection_)
                    {
                        cancel_deadline_timer();
                        parser_.done();
                        is_reading = false;
                        check_destroy();
                        // adaptor will close after write
                    }
                    else if (!need_to_call_after_handlers_)
                    {
                        start_deadline();
                        do_read();
                    }
                    else
                    {
                        // res will be completed later by user
                        need_to_start_read_after_complete_ = true;
                    }
                });
        }

        void do_write()
        {
            //auto self = this->shared_from_this();
            is_writing = true;
            boost::asio::async_write(adaptor_.socket(), buffers_, 
                [&](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
                {
                    is_writing = false;
                    res.clear();
                    res_body_copy_.clear();
                    if (!ec)
                    {
                        if (close_connection_)
                        {
                            adaptor_.close();
                            CROW_LOG_DEBUG << this << " from write(1)";
                            check_destroy();
                        }
                    }
                    else
                    {
                        CROW_LOG_DEBUG << this << " from write(2)";
                        check_destroy();
                    }
                });
        }

        void check_destroy()
        {
            CROW_LOG_DEBUG << this << " is_reading " << is_reading << " is_writing " << is_writing;
            if (!is_reading && !is_writing)
            {
                CROW_LOG_DEBUG << this << " delete (idle) ";
                delete this;
            }
        }

        void cancel_deadline_timer()
        {
            CROW_LOG_DEBUG << this << " timer cancelled: " << timer_cancel_key_.first << ' ' << timer_cancel_key_.second;
            timer_queue.cancel(timer_cancel_key_);
        }

        void start_deadline(/*int timeout = 5*/)
        {
            cancel_deadline_timer();
            
            timer_cancel_key_ = timer_queue.add([this]
            {
                if (!adaptor_.is_open())
                {
                    return;
                }
                adaptor_.close();
            });
            CROW_LOG_DEBUG << this << " timer added: " << timer_cancel_key_.first << ' ' << timer_cancel_key_.second;
        }

    private:
        Adaptor adaptor_;
        Handler* handler_;

        boost::array<char, 4096> buffer_;

        HTTPParser<Connection> parser_;
        request req_;
        response res;

        bool close_connection_ = false;

        const std::string& server_name_;
        std::vector<boost::asio::const_buffer> buffers_;

        std::string content_length_;
        std::string date_str_;
        std::string res_body_copy_;

        //boost::asio::deadline_timer deadline_;
        detail::dumb_timer_queue::key timer_cancel_key_;

        bool is_reading{};
        bool is_writing{};
        bool need_to_call_after_handlers_{};
        bool need_to_start_read_after_complete_{};
        bool add_keep_alive_{};

        std::tuple<Middlewares...>* middlewares_;
        detail::context<Middlewares...> ctx_;

        std::function<std::string()>& get_cached_date_str;
        detail::dumb_timer_queue& timer_queue;
    };

}



#pragma once

#include <chrono>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio.hpp>
#ifdef CROW_ENABLE_SSL
#include <boost/asio/ssl.hpp>
#endif
#include <cstdint>
#include <atomic>
#include <future>
#include <vector>

#include <memory>








namespace crow
{
    using namespace boost;
    using tcp = asio::ip::tcp;

    template <typename Handler, typename Adaptor = SocketAdaptor, typename ... Middlewares>
    class Server
    {
    public:
    Server(Handler* handler, std::string bindaddr, uint16_t port, std::tuple<Middlewares...>* middlewares = nullptr, uint16_t concurrency = 1, typename Adaptor::context* adaptor_ctx = nullptr)
            : acceptor_(io_service_, tcp::endpoint(boost::asio::ip::address::from_string(bindaddr), port)),
            signals_(io_service_, SIGINT, SIGTERM),
            tick_timer_(io_service_),
            handler_(handler),
            concurrency_(concurrency),
            port_(port),
            bindaddr_(bindaddr),
            middlewares_(middlewares),
            adaptor_ctx_(adaptor_ctx)
        {
        }

        void set_tick_function(std::chrono::milliseconds d, std::function<void()> f)
        {
            tick_interval_ = d;
            tick_function_ = f;
        }

        void on_tick()
        {
            tick_function_();
            tick_timer_.expires_from_now(boost::posix_time::milliseconds(tick_interval_.count()));
            tick_timer_.async_wait([this](const boost::system::error_code& ec)
                    {
                        if (ec)
                            return;
                        on_tick();
                    });
        }

        void run()
        {
            if (concurrency_ < 0)
                concurrency_ = 1;

            for(int i = 0; i < concurrency_;  i++)
                io_service_pool_.emplace_back(new boost::asio::io_service());
            get_cached_date_str_pool_.resize(concurrency_);
            timer_queue_pool_.resize(concurrency_);

            std::vector<std::future<void>> v;
            std::atomic<int> init_count(0);
            for(uint16_t i = 0; i < concurrency_; i ++)
                v.push_back(
                        std::async(std::launch::async, [this, i, &init_count]{

                            // thread local date string get function
                            auto last = std::chrono::steady_clock::now();

                            std::string date_str;
                            auto update_date_str = [&]
                            {
                                auto last_time_t = time(0);
                                tm my_tm;

#ifdef _MSC_VER
                                gmtime_s(&my_tm, &last_time_t);
#else
                                gmtime_r(&last_time_t, &my_tm);
#endif
                                date_str.resize(100);
                                size_t date_str_sz = strftime(&date_str[0], 99, "%a, %d %b %Y %H:%M:%S GMT", &my_tm);
                                date_str.resize(date_str_sz);
                            };
                            update_date_str();
                            get_cached_date_str_pool_[i] = [&]()->std::string
                            {
                                if (std::chrono::steady_clock::now() - last >= std::chrono::seconds(1))
                                {
                                    last = std::chrono::steady_clock::now();
                                    update_date_str();
                                }
                                return date_str;
                            };

                            // initializing timer queue
                            detail::dumb_timer_queue timer_queue;
                            timer_queue_pool_[i] = &timer_queue;

                            timer_queue.set_io_service(*io_service_pool_[i]);
                            boost::asio::deadline_timer timer(*io_service_pool_[i]);
                            timer.expires_from_now(boost::posix_time::seconds(1));

                            std::function<void(const boost::system::error_code& ec)> handler;
                            handler = [&](const boost::system::error_code& ec){
                                if (ec)
                                    return;
                                timer_queue.process();
                                timer.expires_from_now(boost::posix_time::seconds(1));
                                timer.async_wait(handler);
                            };
                            timer.async_wait(handler);

                            init_count ++;
                            try 
                            {
                                io_service_pool_[i]->run();
                            } catch(std::exception& e)
                            {
                                CROW_LOG_ERROR << "Worker Crash: An uncaught exception occurred: " << e.what();
                            }
                        }));

            if (tick_function_ && tick_interval_.count() > 0) 
            {
                tick_timer_.expires_from_now(boost::posix_time::milliseconds(tick_interval_.count()));
                tick_timer_.async_wait([this](const boost::system::error_code& ec)
                        {
                            if (ec)
                                return;
                            on_tick();
                        });
            }

            CROW_LOG_INFO << server_name_ << " server is running, local port " << port_;

            signals_.async_wait(
                [&](const boost::system::error_code& /*error*/, int /*signal_number*/){
                    stop();
                });

            while(concurrency_ != init_count)
                std::this_thread::yield();

            do_accept();

            std::thread([this]{
                io_service_.run();
                CROW_LOG_INFO << "Exiting.";
            }).join();
        }

        void stop()
        {
            io_service_.stop();
            for(auto& io_service:io_service_pool_)
                io_service->stop();
        }

    private:
        asio::io_service& pick_io_service()
        {
            // TODO load balancing
            roundrobin_index_++;
            if (roundrobin_index_ >= io_service_pool_.size())
                roundrobin_index_ = 0;
            return *io_service_pool_[roundrobin_index_];
        }

        void do_accept()
        {
            asio::io_service& is = pick_io_service();
            auto p = new Connection<Adaptor, Handler, Middlewares...>(
                is, handler_, server_name_, middlewares_,
                get_cached_date_str_pool_[roundrobin_index_], *timer_queue_pool_[roundrobin_index_],
                adaptor_ctx_);
            acceptor_.async_accept(p->socket(),
                [this, p, &is](boost::system::error_code ec)
                {
                    if (!ec)
                    {
                        is.post([p]
                        {
                            p->start();
                        });
                    }
                    do_accept();
                });
        }

    private:
        asio::io_service io_service_;
        std::vector<std::unique_ptr<asio::io_service>> io_service_pool_;
        std::vector<detail::dumb_timer_queue*> timer_queue_pool_;
        std::vector<std::function<std::string()>> get_cached_date_str_pool_;
        tcp::acceptor acceptor_;
        boost::asio::signal_set signals_;
        boost::asio::deadline_timer tick_timer_;

        Handler* handler_;
        uint16_t concurrency_{1};
        std::string server_name_ = "Crow/0.1";
        uint16_t port_;
        std::string bindaddr_;
        unsigned int roundrobin_index_{};

        std::chrono::milliseconds tick_interval_;
        std::function<void()> tick_function_;

        std::tuple<Middlewares...>* middlewares_;

#ifdef CROW_ENABLE_SSL
        bool use_ssl_{false};
        boost::asio::ssl::context ssl_context_{boost::asio::ssl::context::sslv23};
#endif
        typename Adaptor::context* adaptor_ctx_;
    };
}



#pragma once

#include <chrono>
#include <string>
#include <functional>
#include <memory>
#include <future>
#include <cstdint>
#include <type_traits>
#include <thread>

















#ifdef CROW_MSVC_WORKAROUND
#define CROW_ROUTE(app, url) app.route_dynamic(url)
#else
#define CROW_ROUTE(app, url) app.route<crow::black_magic::get_parameter_tag(url)>(url)
#endif

namespace crow
{
#ifdef CROW_ENABLE_SSL
    using ssl_context_t = boost::asio::ssl::context;
#endif
    template <typename ... Middlewares>
    class Crow
    {
    public:
        using self_t = Crow;
        using server_t = Server<Crow, SocketAdaptor, Middlewares...>;
#ifdef CROW_ENABLE_SSL
        using ssl_server_t = Server<Crow, SSLAdaptor, Middlewares...>;
#endif
        Crow()
        {
        }

		template <typename Adaptor> 
        void handle_upgrade(const request& req, response& res, Adaptor&& adaptor)
        {
            router_.handle_upgrade(req, res, adaptor);
        }

        void handle(const request& req, response& res)
        {
            router_.handle(req, res);
        }

        DynamicRule& route_dynamic(std::string&& rule)
        {
            return router_.new_rule_dynamic(std::move(rule));
        }

        template <uint64_t Tag>
        auto route(std::string&& rule)
            -> typename std::result_of<decltype(&Router::new_rule_tagged<Tag>)(Router, std::string&&)>::type
        {
            return router_.new_rule_tagged<Tag>(std::move(rule));
        }

        self_t& port(std::uint16_t port)
        {
            port_ = port;
            return *this;
        }

        self_t& bindaddr(std::string bindaddr)
        {
            bindaddr_ = bindaddr;
            return *this;
        }

        self_t& multithreaded()
        {
            return concurrency(std::thread::hardware_concurrency());
        }

        self_t& concurrency(std::uint16_t concurrency)
        {
            if (concurrency < 1)
                concurrency = 1;
            concurrency_ = concurrency;
            return *this;
        }

        void validate()
        {
            router_.validate();
        }

        void run()
        {
            validate();
#ifdef CROW_ENABLE_SSL
            if (use_ssl_)
            {
                ssl_server_ = std::move(std::unique_ptr<ssl_server_t>(new ssl_server_t(this, bindaddr_, port_, &middlewares_, concurrency_, &ssl_context_)));
                ssl_server_->set_tick_function(tick_interval_, tick_function_);
                ssl_server_->run();
            }
            else
#endif
            {
                server_ = std::move(std::unique_ptr<server_t>(new server_t(this, bindaddr_, port_, &middlewares_, concurrency_, nullptr)));
                server_->set_tick_function(tick_interval_, tick_function_);
                server_->run();
            }
        }

        void stop()
        {
#ifdef CROW_ENABLE_SSL
            if (use_ssl_)
            {
                ssl_server_->stop();
            }
            else
#endif
            {
                server_->stop();
            }
        }

        void debug_print()
        {
            CROW_LOG_DEBUG << "Routing:";
            router_.debug_print();
        }

#ifdef CROW_ENABLE_SSL
        self_t& ssl_file(const std::string& crt_filename, const std::string& key_filename)
        {
            use_ssl_ = true;
            ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
            ssl_context_.use_certificate_file(crt_filename, ssl_context_t::pem);
            ssl_context_.use_private_key_file(key_filename, ssl_context_t::pem);
            ssl_context_.set_options(
                    boost::asio::ssl::context::default_workarounds
                          | boost::asio::ssl::context::no_sslv2
                          | boost::asio::ssl::context::no_sslv3
                    );
            return *this;
        }

        self_t& ssl_file(const std::string& pem_filename)
        {
            use_ssl_ = true;
            ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
            ssl_context_.load_verify_file(pem_filename);
            ssl_context_.set_options(
                    boost::asio::ssl::context::default_workarounds
                          | boost::asio::ssl::context::no_sslv2
                          | boost::asio::ssl::context::no_sslv3
                    );
            return *this;
        }

        self_t& ssl(boost::asio::ssl::context&& ctx)
        {
            use_ssl_ = true;
            ssl_context_ = std::move(ctx);
            return *this;
        }


        bool use_ssl_{false};
        ssl_context_t ssl_context_{boost::asio::ssl::context::sslv23};

#else
        template <typename T, typename ... Remain>
        self_t& ssl_file(T&&, Remain&&...)
        {
            // We can't call .ssl() member function unless CROW_ENABLE_SSL is defined.
            static_assert(
                    // make static_assert dependent to T; always false
                    std::is_base_of<T, void>::value,
                    "Define CROW_ENABLE_SSL to enable ssl support.");
            return *this;
        }

        template <typename T>
        self_t& ssl(T&&)
        {
            // We can't call .ssl() member function unless CROW_ENABLE_SSL is defined.
            static_assert(
                    // make static_assert dependent to T; always false
                    std::is_base_of<T, void>::value,
                    "Define CROW_ENABLE_SSL to enable ssl support.");
            return *this;
        }
#endif

        // middleware
        using context_t = detail::context<Middlewares...>;
        template <typename T>
        typename T::context& get_context(const request& req)
        {
            static_assert(black_magic::contains<T, Middlewares...>::value, "App doesn't have the specified middleware type.");
            auto& ctx = *reinterpret_cast<context_t*>(req.middleware_context);
            return ctx.template get<T>();
        }

        template <typename T>
        T& get_middleware()
        {
            return utility::get_element_by_type<T, Middlewares...>(middlewares_);
        }

        template <typename Duration, typename Func>
        self_t& tick(Duration d, Func f) {
            tick_interval_ = std::chrono::duration_cast<std::chrono::milliseconds>(d);
            tick_function_ = f;
            return *this;
        }

    private:
        uint16_t port_ = 80;
        uint16_t concurrency_ = 1;
        std::string bindaddr_ = "0.0.0.0";
        Router router_;

        std::chrono::milliseconds tick_interval_;
        std::function<void()> tick_function_;

        std::tuple<Middlewares...> middlewares_;

#ifdef CROW_ENABLE_SSL
        std::unique_ptr<ssl_server_t> ssl_server_;
#endif
        std::unique_ptr<server_t> server_;
    };
    template <typename ... Middlewares>
    using App = Crow<Middlewares...>;
    using SimpleApp = Crow<>;
}



#pragma once














































