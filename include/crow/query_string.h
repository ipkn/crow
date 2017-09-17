#pragma once

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <boost/optional.hpp>

namespace crow
{
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

inline boost::optional<std::pair<std::string, std::string>> qs_dict_name2kv(const char * dict_name, char * const * qs_kv, int qs_kv_size, int nth = 0)
{
    int i;
    size_t name_len, skip_to_eq, skip_to_brace_open, skip_to_brace_close;

    name_len = strlen(dict_name);

#ifdef _qsSORTING
// TODO: binary search for key in the sorted qs_kv
#else  // _qsSORTING
    for(i=0; i<qs_kv_size; i++)
    {
        if ( strncmp(dict_name, qs_kv[i], name_len) == 0 )
        {
            skip_to_eq = strcspn(qs_kv[i], "=");
            if ( qs_kv[i][skip_to_eq] == '=' )
                skip_to_eq++;
            skip_to_brace_open = strcspn(qs_kv[i], "[");
            if ( qs_kv[i][skip_to_brace_open] == '[' )
                skip_to_brace_open++;
            skip_to_brace_close = strcspn(qs_kv[i], "]");

            if ( skip_to_brace_open <= skip_to_brace_close &&
                 skip_to_brace_open > 0 &&
                 skip_to_brace_close > 0 &&
                 nth == 0 )
            {
                auto key = std::string(qs_kv[i] + skip_to_brace_open, skip_to_brace_close - skip_to_brace_open);
                auto value = std::string(qs_kv[i] + skip_to_eq);
                return boost::make_optional(std::make_pair(key, value));
            }
            else
            {
                --nth;
            }
        }
    }
#endif  // _qsSORTING

    return boost::none;
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
#ifdef _MSC_VER
        strncpy_s(val, val_len, qs, (val_len - 1)<(i + 1) ? (val_len - 1) : (i + 1));
#else
        strncpy(val, qs, (val_len - 1)<(i + 1) ? (val_len - 1) : (i + 1));
#endif
		qs_decode(val);
    }
    else
    {
        if ( val_len > 0 )
            val[0] = '\0';
    }

    return val;
}
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

        std::unordered_map<std::string, std::string> get_dict (const std::string& name) const
        {
            std::unordered_map<std::string, std::string> ret;

            int count = 0;
            while(1)
            {
                if (auto element = qs_dict_name2kv(name.c_str(), key_value_pairs_.data(), key_value_pairs_.size(), count++))
                    ret.insert(*element);
                else
                    break;
            }
            return ret;
        }

    private:
        std::string url_;
        std::vector<char*> key_value_pairs_;
    };

} // end namespace
