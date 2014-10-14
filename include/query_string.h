#pragma once

#include <stdio.h>
#include <string>

using namespace std;

// ----------------------------------------------------------------------------
// qs_parse (modified)
// https://github.com/bartgrantham/qs_parse
// ----------------------------------------------------------------------------
/*  Similar to strncmp, but handles URL-encoding for either string  */
int qs_strncmp(const char * s, const char * qs, register size_t n);


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
#define ISHEX(x)    ((((x)>='0'&&(x)<='9') || ((x)>='A'&&(x)<='F') || ((x)>='a'&&(x)<='f')) ? 1 : 0)
#define HEX2DEC(x)  (((x)>='0'&&(x)<='9') ? (x)-48 : ((x)>='A'&&(x)<='F') ? (x)-55 : ((x)>='a'&&(x)<='f') ? (x)-87 : 0)
#define ISQSCHR(x) ((((x)=='=')||((x)=='#')||((x)=='&')||((x)=='\0')) ? 0 : 1)

int qs_strncmp(const char * s, const char * qs, register size_t n)
{
    int i=0;
    register unsigned char u1, u2, unyb, lnyb;

    while(n-- > 0)
    {
        u1 = (unsigned char) *s++;
        u2 = (unsigned char) *qs++;

        if ( ! ISQSCHR(u1) ) {  u1 = '\0';  }
        if ( ! ISQSCHR(u2) ) {  u2 = '\0';  }

        if ( u1 == '+' ) {  u1 = ' ';  }
        if ( u1 == '%' ) // easier/safer than scanf
        {
            unyb = (unsigned char) *s++;
            lnyb = (unsigned char) *s++;
            if ( ISHEX(unyb) && ISHEX(lnyb) )
                u1 = (HEX2DEC(unyb) * 16) + HEX2DEC(lnyb);
            else
                u1 = '\0';
        }

        if ( u2 == '+' ) {  u2 = ' ';  }
        if ( u2 == '%' ) // easier/safer than scanf
        {
            unyb = (unsigned char) *qs++;
            lnyb = (unsigned char) *qs++;
            if ( ISHEX(unyb) && ISHEX(lnyb) )
                u2 = (HEX2DEC(unyb) * 16) + HEX2DEC(lnyb);
            else
                u2 = '\0';
        }

        if ( u1 != u2 )
            return u1 - u2;
        if ( u1 == '\0' )
            return 0;
        i++;
    }
    if ( ISQSCHR(*qs) )
        return -1;
    else
        return 0;
}


int qs_parse(char * qs, char * qs_kv[], int qs_kv_size)
{
    int i, j;
    char * substr_ptr;

    for(i=0; i<qs_kv_size; i++)  qs_kv[i] = NULL;

    // find the beginning of the k/v substrings
    if ( (substr_ptr = strchr(qs, '?')) != NULL )
        substr_ptr++;
    else
        substr_ptr = qs;

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
        if ( substr_ptr[0] == '&' )  // blank value: skip decoding
            substr_ptr[0] = '\0';
        else
            qs_decode(++substr_ptr);
    }

#ifdef _qsSORTING
// TODO: qsort qs_kv, using qs_strncmp() for the comparison
#endif

    return i;
}


int qs_decode(char * qs)
{
    int i=0, j=0;

    while( ISQSCHR(qs[j]) )
    {
        if ( qs[j] == '+' ) {  qs[i] = ' ';  }
        else if ( qs[j] == '%' ) // easier/safer than scanf
        {
            if ( ! ISHEX(qs[j+1]) || ! ISHEX(qs[j+2]) )
            {
                qs[i] = '\0';
                return i;
            }
            qs[i] = (HEX2DEC(qs[j+1]) * 16) + HEX2DEC(qs[j+2]);
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


char * qs_k2v(const char * key, char * const * qs_kv, int qs_kv_size, int nth = 0)
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


char * qs_scanvalue(const char * key, const char * qs, char * val, size_t val_len)
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

// TODO to save allocs, capping url size to 2048 seems sane and reasonable but
// crow should *technically* return a 413 if a URL is longer than this.
#define MAX_URL_SIZE (2048)

#define NUM_KV_PAIRS (256)
#define VAL_SIZE (256)

namespace crow 
{
    class query_string
    {
    public:
        query_string()
        {

        }
        query_string(std::string url)
        {
            if(url.length() <= MAX_URL_SIZE) {
                memset(_url, 0, MAX_URL_SIZE); // overkill?            
                memcpy(_url, url.c_str(), url.length());
            }
            _kv_size = qs_parse(_url, _kv_pairs, NUM_KV_PAIRS);
        }
        void clear() {
            _url[0] = 0;
        }

        friend ostream& operator<<(ostream& os, const query_string& qs)
        {
            os << "[ ";
            for(int i = 0; i < qs._kv_size; ++i) {
                os << qs._kv_pairs[i];
                if((i + 1) < qs._kv_size) {
                    os << ", ";
                }
            }
            os << " ]";
            return os;

        }

        char* get (const string name) const
        {
            char* ret = qs_k2v(name.c_str(), _kv_pairs, _kv_size);
            return ret != 0 ? ret : nullptr;
        }

        vector<char*> get_list (const string name) const
        {
            vector<char*> ret;
            string plus = name + "[]";            
            char* tmp = nullptr;
            int count = 0;
            do
            {
                tmp = qs_k2v(plus.c_str(), _kv_pairs, _kv_size, count++);
                if(tmp != nullptr) {
                    ret.push_back(tmp);
                }
            } while(tmp != nullptr);
            return move(ret);
        }


    private:
        char    _url[MAX_URL_SIZE];
        char*   _kv_pairs[NUM_KV_PAIRS];
        int     _kv_size;
    };

} // end namespace