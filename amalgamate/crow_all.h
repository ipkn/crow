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
                void force(char* s, uint32_t length)
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

            explicit operator int() const
            {
                return i();
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
                if (t() != type::Number)
                    throw std::runtime_error("value is not number");
#endif
                return boost::lexical_cast<int64_t>(start_, end_-start_);
            }

            double d() const
            {
#ifndef CROW_JSON_NO_ERROR_CHECK
                if (t() != type::Number)
                    throw std::runtime_error("value is not number");
#endif
                return boost::lexical_cast<double>(start_, end_-start_);
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
                                            *tail++ = 0b11100000 | (code >> 12);
                                            *tail++ = 0b10000000 | ((code >> 6) & 0b111111);
                                            *tail++ = 0b10000000 | (code & 0b111111);
                                        }
                                        else if (code >= 0x80)
                                        {
                                            *tail++ = 0b11000000 | (code >> 6);
                                            *tail++ = 0b10000000 | (code & 0b111111);
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
				Parser(char* data, size_t size)
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
                                state = (NumberParsingState)"\7\7\7\4\7\7\7"[state];
                                /*
                                if (state == NumberParsingState::Digits)
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
                        l = std::move(std::unique_ptr<std::vector<wvalue>>(new std::vector<wvalue>{}));
                        l->reserve(r.size());
                        for(auto it = r.begin(); it != r.end(); ++it)
                            l->emplace_back(*it);
                        return;
                    case type::Object:
                        o = std::move(
                            std::unique_ptr<
                                    std::unordered_map<std::string, wvalue>
                                >(
                                new std::unordered_map<std::string, wvalue>{}));
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

            wvalue& operator = (uint16_t value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (int16_t value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (uint32_t value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (int32_t value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (uint64_t value)
            {
                reset();
                t_ = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (int64_t value)
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
            wvalue& operator[](const std::vector<T>& v)
            {
                if (t_ != type::List)
                    reset();
                t_ = type::List;
                if (!l)
                    l = std::move(std::unique_ptr<std::vector<wvalue>>(new std::vector<wvalue>{}));
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
                    l = std::move(std::unique_ptr<std::vector<wvalue>>(new std::vector<wvalue>{}));
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
                    o = std::move(
                        std::unique_ptr<
                                std::unordered_map<std::string, wvalue>
                            >(
                            new std::unordered_map<std::string, wvalue>{}));
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
            std::ifstream inf(detail::get_template_base_directory_ref() + filename);
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

        inline template_t load(const std::string& filename)
        {
            return compile(detail::get_loader_ref()(filename));
        }
    }
}



#pragma once
#include <string>
#include <unordered_map>



namespace crow
{
    template <typename T>
    class Connection;
    struct response
    {
        template <typename T> 
        friend class crow::Connection;

        std::string body;
        json::wvalue json_value;
        int code{200};
        std::unordered_map<std::string, std::string> headers;

        response() {}
        explicit response(int code) : code(code) {}
        response(std::string body) : body(std::move(body)) {}
        response(json::wvalue&& json_value) : json_value(std::move(json_value)) {}
        response(const json::wvalue& json_value) : body(json::dump(json_value)) {}
        response(int code, std::string body) : body(std::move(body)), code(code) {}

        response(response&& r)
        {
            *this = std::move(r);
        }

        response& operator = (const response& r) = delete;

        response& operator = (response&& r)
        {
            body = std::move(r.body);
            json_value = std::move(r.json_value);
            code = r.code;
            headers = std::move(r.headers);
			completed_ = r.completed_;
            return *this;
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
    };
}



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
#ifndef http_parser_h
#define http_parser_h
#ifdef __cplusplus
extern "C" {
#endif

/* Also update SONAME in the Makefile whenever you change these. */
#define HTTP_PARSER_VERSION_MAJOR 2
#define HTTP_PARSER_VERSION_MINOR 3
#define HTTP_PARSER_VERSION_PATCH 0

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
#ifndef HTTP_PARSER_STRICT
# define HTTP_PARSER_STRICT 1
#endif

/* Maximium header size allowed. If the macro is not defined
 * before including this header then the default is used. To
 * change the maximum header size, define the macro in the build
 * environment (e.g. -DHTTP_MAX_HEADER_SIZE=<value>). To remove
 * the effective limit on the size of the header, define the macro
 * to a very large number (e.g. -DHTTP_MAX_HEADER_SIZE=0x7fffffff)
 */
#ifndef HTTP_MAX_HEADER_SIZE
# define HTTP_MAX_HEADER_SIZE (80*1024)
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
#define HTTP_METHOD_MAP(XX)         \
  XX(0,  DELETE,      DELETE)       \
  XX(1,  GET,         GET)          \
  XX(2,  HEAD,        HEAD)         \
  XX(3,  POST,        POST)         \
  XX(4,  PUT,         PUT)          \
  /* pathological */                \
  XX(5,  CONNECT,     CONNECT)      \
  XX(6,  OPTIONS,     OPTIONS)      \
  XX(7,  TRACE,       TRACE)        \
  /* webdav */                      \
  XX(8,  COPY,        COPY)         \
  XX(9,  LOCK,        LOCK)         \
  XX(10, MKCOL,       MKCOL)        \
  XX(11, MOVE,        MOVE)         \
  XX(12, PROPFIND,    PROPFIND)     \
  XX(13, PROPPATCH,   PROPPATCH)    \
  XX(14, SEARCH,      SEARCH)       \
  XX(15, UNLOCK,      UNLOCK)       \
  /* subversion */                  \
  XX(16, REPORT,      REPORT)       \
  XX(17, MKACTIVITY,  MKACTIVITY)   \
  XX(18, CHECKOUT,    CHECKOUT)     \
  XX(19, MERGE,       MERGE)        \
  /* upnp */                        \
  XX(20, MSEARCH,     M-SEARCH)     \
  XX(21, NOTIFY,      NOTIFY)       \
  XX(22, SUBSCRIBE,   SUBSCRIBE)    \
  XX(23, UNSUBSCRIBE, UNSUBSCRIBE)  \
  /* RFC-5789 */                    \
  XX(24, PATCH,       PATCH)        \
  XX(25, PURGE,       PURGE)        \
  /* CalDAV */                      \
  XX(26, MKCALENDAR,  MKCALENDAR)   \

enum http_method
  {
#define XX(num, name, string) HTTP_##name = num,
  HTTP_METHOD_MAP(XX)
#undef XX
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
#define HTTP_ERRNO_MAP(XX)                                           \
  /* No error */                                                     \
  XX(OK, "success")                                                  \
                                                                     \
  /* Callback-related errors */                                      \
  XX(CB_message_begin, "the on_message_begin callback failed")       \
  XX(CB_url, "the on_url callback failed")                           \
  XX(CB_header_field, "the on_header_field callback failed")         \
  XX(CB_header_value, "the on_header_value callback failed")         \
  XX(CB_headers_complete, "the on_headers_complete callback failed") \
  XX(CB_body, "the on_body callback failed")                         \
  XX(CB_message_complete, "the on_message_complete callback failed") \
  XX(CB_status, "the on_status callback failed")                     \
                                                                     \
  /* Parsing-related errors */                                       \
  XX(INVALID_EOF_STATE, "stream ended at an unexpected time")        \
  XX(HEADER_OVERFLOW,                                                \
     "too many header bytes seen; overflow detected")                \
  XX(CLOSED_CONNECTION,                                              \
     "data received after completed connection: close message")      \
  XX(INVALID_VERSION, "invalid HTTP version")                        \
  XX(INVALID_STATUS, "invalid HTTP status code")                     \
  XX(INVALID_METHOD, "invalid HTTP method")                          \
  XX(INVALID_URL, "invalid URL")                                     \
  XX(INVALID_HOST, "invalid host")                                   \
  XX(INVALID_PORT, "invalid port")                                   \
  XX(INVALID_PATH, "invalid path")                                   \
  XX(INVALID_QUERY_STRING, "invalid query string")                   \
  XX(INVALID_FRAGMENT, "invalid fragment")                           \
  XX(LF_EXPECTED, "LF character expected")                           \
  XX(INVALID_HEADER_TOKEN, "invalid character in header")            \
  XX(INVALID_CONTENT_LENGTH,                                         \
     "invalid character in content-length header")                   \
  XX(INVALID_CHUNK_SIZE,                                             \
     "invalid character in chunk size header")                       \
  XX(INVALID_CONSTANT, "invalid constant string")                    \
  XX(INVALID_INTERNAL_STATE, "encountered unexpected internal state")\
  XX(STRICT, "strict mode assertion failed")                         \
  XX(PAUSED, "parser is paused")                                     \
  XX(UNKNOWN, "an unknown error occurred")


/* Define HPE_* values for each errno value above */
#define HTTP_ERRNO_GEN(n, s) HPE_##n,
enum http_errno {
  HTTP_ERRNO_MAP(HTTP_ERRNO_GEN)
};
#undef HTTP_ERRNO_GEN


/* Get an http_errno value from an http_parser */
#define HTTP_PARSER_ERRNO(p)            ((enum http_errno) (p)->http_errno)


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

#ifndef ULLONG_MAX
# define ULLONG_MAX ((uint64_t) -1) /* 2^64-1 */
#endif

#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef BIT_AT
# define BIT_AT(a, i)                                                \
  (!!((unsigned int) (a)[(unsigned int) (i) >> 3] &                  \
   (1 << ((unsigned int) (i) & 7))))
#endif

#ifndef ELEM_AT
# define ELEM_AT(a, i, v) ((unsigned int) (i) < ARRAY_SIZE(a) ? (a)[(i)] : (v))
#endif

#define SET_ERRNO(e)                                                 \
do {                                                                 \
  parser->http_errno = (e);                                          \
} while(0)


/* Run the notify callback FOR, returning ER if it fails */
#define CALLBACK_NOTIFY_(FOR, ER)                                    \
do {                                                                 \
  assert(HTTP_PARSER_ERRNO(parser) == HPE_OK);                       \
                                                                     \
  if (settings->on_##FOR) {                                          \
    if (0 != settings->on_##FOR(parser)) {                           \
      SET_ERRNO(HPE_CB_##FOR);                                       \
    }                                                                \
                                                                     \
    /* We either errored above or got paused; get out */             \
    if (HTTP_PARSER_ERRNO(parser) != HPE_OK) {                       \
      return (ER);                                                   \
    }                                                                \
  }                                                                  \
} while (0)

/* Run the notify callback FOR and consume the current byte */
#define CALLBACK_NOTIFY(FOR)            CALLBACK_NOTIFY_(FOR, p - data + 1)

/* Run the notify callback FOR and don't consume the current byte */
#define CALLBACK_NOTIFY_NOADVANCE(FOR)  CALLBACK_NOTIFY_(FOR, p - data)

/* Run data callback FOR with LEN bytes, returning ER if it fails */
#define CALLBACK_DATA_(FOR, LEN, ER)                                 \
do {                                                                 \
  assert(HTTP_PARSER_ERRNO(parser) == HPE_OK);                       \
                                                                     \
  if (FOR##_mark) {                                                  \
    if (settings->on_##FOR) {                                        \
      if (0 != settings->on_##FOR(parser, FOR##_mark, (LEN))) {      \
        SET_ERRNO(HPE_CB_##FOR);                                     \
      }                                                              \
                                                                     \
      /* We either errored above or got paused; get out */           \
      if (HTTP_PARSER_ERRNO(parser) != HPE_OK) {                     \
        return (ER);                                                 \
      }                                                              \
    }                                                                \
    FOR##_mark = NULL;                                               \
  }                                                                  \
} while (0)
  
/* Run the data callback FOR and consume the current byte */
#define CALLBACK_DATA(FOR)                                           \
    CALLBACK_DATA_(FOR, p - FOR##_mark, p - data + 1)

/* Run the data callback FOR and don't consume the current byte */
#define CALLBACK_DATA_NOADVANCE(FOR)                                 \
    CALLBACK_DATA_(FOR, p - FOR##_mark, p - data)

/* Set the mark FOR; non-destructive if mark is already set */
#define MARK(FOR)                                                    \
do {                                                                 \
  if (!FOR##_mark) {                                                 \
    FOR##_mark = p;                                                  \
  }                                                                  \
} while (0)


#define PROXY_CONNECTION "proxy-connection"
#define CONNECTION "connection"
#define CONTENT_LENGTH "content-length"
#define TRANSFER_ENCODING "transfer-encoding"
#define UPGRADE "upgrade"
#define CHUNKED "chunked"
#define KEEP_ALIVE "keep-alive"
#define CLOSE "close"




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
   * checking. See the PARSING_HEADER() macro.
   */

  , s_chunk_data
  , s_chunk_data_almost_done
  , s_chunk_data_done

  , s_body_identity
  , s_body_identity_eof

  , s_message_done
  };


#define PARSING_HEADER(state) (state <= s_headers_done)


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
#define CR                  '\r'
#define LF                  '\n'
#define LOWER(c)            (unsigned char)(c | 0x20)
#define IS_ALPHA(c)         (LOWER(c) >= 'a' && LOWER(c) <= 'z')
#define IS_NUM(c)           ((c) >= '0' && (c) <= '9')
#define IS_ALPHANUM(c)      (IS_ALPHA(c) || IS_NUM(c))
#define IS_HEX(c)           (IS_NUM(c) || (LOWER(c) >= 'a' && LOWER(c) <= 'f'))
#define IS_MARK(c)          ((c) == '-' || (c) == '_' || (c) == '.' || \
  (c) == '!' || (c) == '~' || (c) == '*' || (c) == '\'' || (c) == '(' || \
  (c) == ')')
#define IS_USERINFO_CHAR(c) (IS_ALPHANUM(c) || IS_MARK(c) || (c) == '%' || \
  (c) == ';' || (c) == ':' || (c) == '&' || (c) == '=' || (c) == '+' || \
  (c) == '$' || (c) == ',')

#if HTTP_PARSER_STRICT
#define TOKEN(c)            (tokens[(unsigned char)c])
#define IS_URL_CHAR(c)      (BIT_AT(normal_url_char, (unsigned char)c))
#define IS_HOST_CHAR(c)     (IS_ALPHANUM(c) || (c) == '.' || (c) == '-')
#else
#define TOKEN(c)            ((c == ' ') ? ' ' : tokens[(unsigned char)c])
#define IS_URL_CHAR(c)                                                         \
  (BIT_AT(normal_url_char, (unsigned char)c) || ((c) & 0x80))
#define IS_HOST_CHAR(c)                                                        \
  (IS_ALPHANUM(c) || (c) == '.' || (c) == '-' || (c) == '_')
#endif


#define start_state (parser->type == HTTP_REQUEST ? s_start_req : s_start_res)


#if HTTP_PARSER_STRICT
# define STRICT_CHECK(cond)                                          \
do {                                                                 \
  if (cond) {                                                        \
    SET_ERRNO(HPE_STRICT);                                           \
    goto error;                                                      \
  }                                                                  \
} while (0)
# define NEW_MESSAGE() (http_should_keep_alive(parser) ? start_state : s_dead)
#else
# define STRICT_CHECK(cond)
# define NEW_MESSAGE() start_state
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
#if HTTP_PARSER_STRICT
# define T(v) 0
#else
# define T(v) v
#endif


static const uint8_t normal_url_char[32] = {
/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
        0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
        0    | T(2)   |   0    |   0    | T(16)  |   0    |   0    |   0,
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
/*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
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

#undef T

  if (ch == ' ' || ch == '\r' || ch == '\n') {
    return s_dead;
  }

#if HTTP_PARSER_STRICT
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

      if (IS_ALPHA(ch)) {
        return s_req_schema;
      }

      break;

    case s_req_schema:
      if (IS_ALPHA(ch)) {
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

      if (IS_USERINFO_CHAR(ch) || ch == '[' || ch == ']') {
        return s_req_server;
      }

      break;

    case s_req_path:
      if (IS_URL_CHAR(ch)) {
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
      if (IS_URL_CHAR(ch)) {
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
      if (IS_URL_CHAR(ch)) {
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
      if (IS_URL_CHAR(ch)) {
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
#define XX(num, name, string) #string,
  HTTP_METHOD_MAP(XX)
#undef XX
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
  if (HTTP_PARSER_ERRNO(parser) != HPE_OK) {
    return 0;
  }

  if (len == 0) {
    switch (parser->state) {
      case s_body_identity_eof:
        /* Use of CALLBACK_NOTIFY() here would erroneously return 1 byte read if
         * we got paused.
         */
        CALLBACK_NOTIFY_NOADVANCE(message_complete);
        return 0;

      case s_dead:
      case s_start_req_or_res:
      case s_start_res:
      case s_start_req:
        return 0;

      default:
        SET_ERRNO(HPE_INVALID_EOF_STATE);
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

    if (PARSING_HEADER(parser->state)) {
      ++parser->nread;
      /* Don't allow the total size of the HTTP headers (including the status
       * line) to exceed HTTP_MAX_HEADER_SIZE.  This check is here to protect
       * embedders against denial-of-service attacks where the attacker feeds
       * us a never-ending header that the embedder keeps buffering.
       *
       * This check is arguably the responsibility of embedders but we're doing
       * it on the embedder's behalf because most won't bother and this way we
       * make the web a little safer.  HTTP_MAX_HEADER_SIZE is still far bigger
       * than any reasonable request or response so this should never affect
       * day-to-day operation.
       */
      if (parser->nread > (HTTP_MAX_HEADER_SIZE)) {
        SET_ERRNO(HPE_HEADER_OVERFLOW);
        goto error;
      }
    }

    reexecute_byte:
    switch (parser->state) {

      case s_dead:
        /* this state is used after a 'Connection: close' message
         * the parser will error out if it reads another message
         */
        if (ch == CR || ch == LF)
          break;

        SET_ERRNO(HPE_CLOSED_CONNECTION);
        goto error;

      case s_start_req_or_res:
      {
        if (ch == CR || ch == LF)
          break;
        parser->flags = 0;
        parser->content_length = ULLONG_MAX;

        if (ch == 'H') {
          parser->state = s_res_or_resp_H;

          CALLBACK_NOTIFY(message_begin);
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
            SET_ERRNO(HPE_INVALID_CONSTANT);
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
        parser->content_length = ULLONG_MAX;

        switch (ch) {
          case 'H':
            parser->state = s_res_H;
            break;

          case CR:
          case LF:
            break;

          default:
            SET_ERRNO(HPE_INVALID_CONSTANT);
            goto error;
        }

        CALLBACK_NOTIFY(message_begin);
        break;
      }

      case s_res_H:
        STRICT_CHECK(ch != 'T');
        parser->state = s_res_HT;
        break;

      case s_res_HT:
        STRICT_CHECK(ch != 'T');
        parser->state = s_res_HTT;
        break;

      case s_res_HTT:
        STRICT_CHECK(ch != 'P');
        parser->state = s_res_HTTP;
        break;

      case s_res_HTTP:
        STRICT_CHECK(ch != '/');
        parser->state = s_res_first_http_major;
        break;

      case s_res_first_http_major:
        if (ch < '0' || ch > '9') {
          SET_ERRNO(HPE_INVALID_VERSION);
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

        if (!IS_NUM(ch)) {
          SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_major *= 10;
        parser->http_major += ch - '0';

        if (parser->http_major > 999) {
          SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        break;
      }

      /* first digit of minor HTTP version */
      case s_res_first_http_minor:
        if (!IS_NUM(ch)) {
          SET_ERRNO(HPE_INVALID_VERSION);
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

        if (!IS_NUM(ch)) {
          SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_minor *= 10;
        parser->http_minor += ch - '0';

        if (parser->http_minor > 999) {
          SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        break;
      }

      case s_res_first_status_code:
      {
        if (!IS_NUM(ch)) {
          if (ch == ' ') {
            break;
          }

          SET_ERRNO(HPE_INVALID_STATUS);
          goto error;
        }
        parser->status_code = ch - '0';
        parser->state = s_res_status_code;
        break;
      }

      case s_res_status_code:
      {
        if (!IS_NUM(ch)) {
          switch (ch) {
            case ' ':
              parser->state = s_res_status_start;
              break;
            case CR:
              parser->state = s_res_line_almost_done;
              break;
            case LF:
              parser->state = s_header_field_start;
              break;
            default:
              SET_ERRNO(HPE_INVALID_STATUS);
              goto error;
          }
          break;
        }

        parser->status_code *= 10;
        parser->status_code += ch - '0';

        if (parser->status_code > 999) {
          SET_ERRNO(HPE_INVALID_STATUS);
          goto error;
        }

        break;
      }

      case s_res_status_start:
      {
        if (ch == CR) {
          parser->state = s_res_line_almost_done;
          break;
        }

        if (ch == LF) {
          parser->state = s_header_field_start;
          break;
        }

        MARK(status);
        parser->state = s_res_status;
        parser->index = 0;
        break;
      }

      case s_res_status:
        if (ch == CR) {
          parser->state = s_res_line_almost_done;
          CALLBACK_DATA(status);
          break;
        }

        if (ch == LF) {
          parser->state = s_header_field_start;
          CALLBACK_DATA(status);
          break;
        }

        break;

      case s_res_line_almost_done:
        STRICT_CHECK(ch != LF);
        parser->state = s_header_field_start;
        break;

      case s_start_req:
      {
        if (ch == CR || ch == LF)
          break;
        parser->flags = 0;
        parser->content_length = ULLONG_MAX;

        if (!IS_ALPHA(ch)) {
          SET_ERRNO(HPE_INVALID_METHOD);
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
            SET_ERRNO(HPE_INVALID_METHOD);
            goto error;
        }
        parser->state = s_req_method;

        CALLBACK_NOTIFY(message_begin);

        break;
      }

      case s_req_method:
      {
        const char *matcher;
        if (ch == '\0') {
          SET_ERRNO(HPE_INVALID_METHOD);
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
            SET_ERRNO(HPE_INVALID_METHOD);
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
            SET_ERRNO(HPE_INVALID_METHOD);
            goto error;
          }
        } else if (parser->method == HTTP_SUBSCRIBE) {
          if (parser->index == 1 && ch == 'E') {
            parser->method = HTTP_SEARCH;
          } else {
            SET_ERRNO(HPE_INVALID_METHOD);
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
            SET_ERRNO(HPE_INVALID_METHOD);
            goto error;
          }
        } else if (parser->index == 2) {
          if (parser->method == HTTP_PUT) {
            if (ch == 'R') {
              parser->method = HTTP_PURGE;
            } else {
              SET_ERRNO(HPE_INVALID_METHOD);
              goto error;
            }
          } else if (parser->method == HTTP_UNLOCK) {
            if (ch == 'S') {
              parser->method = HTTP_UNSUBSCRIBE;
            } else {
              SET_ERRNO(HPE_INVALID_METHOD);
              goto error;
            }
          } else {
            SET_ERRNO(HPE_INVALID_METHOD);
            goto error;
          }
        } else if (parser->index == 4 && parser->method == HTTP_PROPFIND && ch == 'P') {
          parser->method = HTTP_PROPPATCH;
        } else {
          SET_ERRNO(HPE_INVALID_METHOD);
          goto error;
        }

        ++parser->index;
        break;
      }

      case s_req_spaces_before_url:
      {
        if (ch == ' ') break;

        MARK(url);
        if (parser->method == HTTP_CONNECT) {
          parser->state = s_req_server_start;
        }

        parser->state = parse_url_char((enum state)parser->state, ch);
        if (parser->state == s_dead) {
          SET_ERRNO(HPE_INVALID_URL);
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
          case CR:
          case LF:
            SET_ERRNO(HPE_INVALID_URL);
            goto error;
          default:
            parser->state = parse_url_char((enum state)parser->state, ch);
            if (parser->state == s_dead) {
              SET_ERRNO(HPE_INVALID_URL);
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
            CALLBACK_DATA(url);
            break;
          case CR:
          case LF:
            parser->http_major = 0;
            parser->http_minor = 9;
            parser->state = (ch == CR) ?
              s_req_line_almost_done :
              s_header_field_start;
            CALLBACK_DATA(url);
            break;
          default:
            parser->state = parse_url_char((enum state)parser->state, ch);
            if (parser->state == s_dead) {
              SET_ERRNO(HPE_INVALID_URL);
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
            SET_ERRNO(HPE_INVALID_CONSTANT);
            goto error;
        }
        break;

      case s_req_http_H:
        STRICT_CHECK(ch != 'T');
        parser->state = s_req_http_HT;
        break;

      case s_req_http_HT:
        STRICT_CHECK(ch != 'T');
        parser->state = s_req_http_HTT;
        break;

      case s_req_http_HTT:
        STRICT_CHECK(ch != 'P');
        parser->state = s_req_http_HTTP;
        break;

      case s_req_http_HTTP:
        STRICT_CHECK(ch != '/');
        parser->state = s_req_first_http_major;
        break;

      /* first digit of major HTTP version */
      case s_req_first_http_major:
        if (ch < '1' || ch > '9') {
          SET_ERRNO(HPE_INVALID_VERSION);
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

        if (!IS_NUM(ch)) {
          SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_major *= 10;
        parser->http_major += ch - '0';

        if (parser->http_major > 999) {
          SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        break;
      }

      /* first digit of minor HTTP version */
      case s_req_first_http_minor:
        if (!IS_NUM(ch)) {
          SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_minor = ch - '0';
        parser->state = s_req_http_minor;
        break;

      /* minor HTTP version or end of request line */
      case s_req_http_minor:
      {
        if (ch == CR) {
          parser->state = s_req_line_almost_done;
          break;
        }

        if (ch == LF) {
          parser->state = s_header_field_start;
          break;
        }

        /* XXX allow spaces after digit? */

        if (!IS_NUM(ch)) {
          SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        parser->http_minor *= 10;
        parser->http_minor += ch - '0';

        if (parser->http_minor > 999) {
          SET_ERRNO(HPE_INVALID_VERSION);
          goto error;
        }

        break;
      }

      /* end of request line */
      case s_req_line_almost_done:
      {
        if (ch != LF) {
          SET_ERRNO(HPE_LF_EXPECTED);
          goto error;
        }

        parser->state = s_header_field_start;
        break;
      }

      case s_header_field_start:
      {
        if (ch == CR) {
          parser->state = s_headers_almost_done;
          break;
        }

        if (ch == LF) {
          /* they might be just sending \n instead of \r\n so this would be
           * the second \n to denote the end of headers*/
          parser->state = s_headers_almost_done;
          goto reexecute_byte;
        }

        c = TOKEN(ch);

        if (!c) {
          SET_ERRNO(HPE_INVALID_HEADER_TOKEN);
          goto error;
        }

        MARK(header_field);

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
        c = TOKEN(ch);

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
              if (parser->index > sizeof(CONNECTION)-1
                  || c != CONNECTION[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(CONNECTION)-2) {
                parser->header_state = h_connection;
              }
              break;

            /* proxy-connection */

            case h_matching_proxy_connection:
              parser->index++;
              if (parser->index > sizeof(PROXY_CONNECTION)-1
                  || c != PROXY_CONNECTION[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(PROXY_CONNECTION)-2) {
                parser->header_state = h_connection;
              }
              break;

            /* content-length */

            case h_matching_content_length:
              parser->index++;
              if (parser->index > sizeof(CONTENT_LENGTH)-1
                  || c != CONTENT_LENGTH[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(CONTENT_LENGTH)-2) {
                parser->header_state = h_content_length;
              }
              break;

            /* transfer-encoding */

            case h_matching_transfer_encoding:
              parser->index++;
              if (parser->index > sizeof(TRANSFER_ENCODING)-1
                  || c != TRANSFER_ENCODING[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(TRANSFER_ENCODING)-2) {
                parser->header_state = h_transfer_encoding;
              }
              break;

            /* upgrade */

            case h_matching_upgrade:
              parser->index++;
              if (parser->index > sizeof(UPGRADE)-1
                  || c != UPGRADE[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(UPGRADE)-2) {
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
          CALLBACK_DATA(header_field);
          break;
        }

        if (ch == CR) {
          parser->state = s_header_almost_done;
          CALLBACK_DATA(header_field);
          break;
        }

        if (ch == LF) {
          parser->state = s_header_field_start;
          CALLBACK_DATA(header_field);
          break;
        }

        SET_ERRNO(HPE_INVALID_HEADER_TOKEN);
        goto error;
      }

      case s_header_value_discard_ws:
        if (ch == ' ' || ch == '\t') break;

        if (ch == CR) {
          parser->state = s_header_value_discard_ws_almost_done;
          break;
        }

        if (ch == LF) {
          parser->state = s_header_value_discard_lws;
          break;
        }

        /* FALLTHROUGH */

      case s_header_value_start:
      {
        MARK(header_value);

        parser->state = s_header_value;
        parser->index = 0;

        c = LOWER(ch);

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
            if (!IS_NUM(ch)) {
              SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
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

        if (ch == CR) {
          parser->state = s_header_almost_done;
          CALLBACK_DATA(header_value);
          break;
        }

        if (ch == LF) {
          parser->state = s_header_almost_done;
          CALLBACK_DATA_NOADVANCE(header_value);
          goto reexecute_byte;
        }

        c = LOWER(ch);

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

            if (!IS_NUM(ch)) {
              SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
              goto error;
            }

            t = parser->content_length;
            t *= 10;
            t += ch - '0';

            /* Overflow? Test against a conservative limit for simplicity. */
            if ((ULLONG_MAX - 10) / 10 < parser->content_length) {
              SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
              goto error;
            }

            parser->content_length = t;
            break;
          }

          /* Transfer-Encoding: chunked */
          case h_matching_transfer_encoding_chunked:
            parser->index++;
            if (parser->index > sizeof(CHUNKED)-1
                || c != CHUNKED[parser->index]) {
              parser->header_state = h_general;
            } else if (parser->index == sizeof(CHUNKED)-2) {
              parser->header_state = h_transfer_encoding_chunked;
            }
            break;

          /* looking for 'Connection: keep-alive' */
          case h_matching_connection_keep_alive:
            parser->index++;
            if (parser->index > sizeof(KEEP_ALIVE)-1
                || c != KEEP_ALIVE[parser->index]) {
              parser->header_state = h_general;
            } else if (parser->index == sizeof(KEEP_ALIVE)-2) {
              parser->header_state = h_connection_keep_alive;
            }
            break;

          /* looking for 'Connection: close' */
          case h_matching_connection_close:
            parser->index++;
            if (parser->index > sizeof(CLOSE)-1 || c != CLOSE[parser->index]) {
              parser->header_state = h_general;
            } else if (parser->index == sizeof(CLOSE)-2) {
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
        STRICT_CHECK(ch != LF);

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
        STRICT_CHECK(ch != LF);
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
          MARK(header_value);
          parser->state = s_header_field_start;
          CALLBACK_DATA_NOADVANCE(header_value);
          goto reexecute_byte;
        }
      }

      case s_headers_almost_done:
      {
        STRICT_CHECK(ch != LF);

        if (parser->flags & F_TRAILING) {
          /* End of a chunked request */
          parser->state = NEW_MESSAGE();
          CALLBACK_NOTIFY(message_complete);
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
         * We'd like to use CALLBACK_NOTIFY_NOADVANCE() here but we cannot, so
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
              SET_ERRNO(HPE_CB_headers_complete);
              return p - data; /* Error */
          }
        }

        if (HTTP_PARSER_ERRNO(parser) != HPE_OK) {
          return p - data;
        }

        goto reexecute_byte;
      }

      case s_headers_done:
      {
        STRICT_CHECK(ch != LF);

        parser->nread = 0;

        /* Exit, the rest of the connect is in a different protocol. */
        if (parser->upgrade) {
          parser->state = NEW_MESSAGE();
          CALLBACK_NOTIFY(message_complete);
          return (p - data) + 1;
        }

        if (parser->flags & F_SKIPBODY) {
          parser->state = NEW_MESSAGE();
          CALLBACK_NOTIFY(message_complete);
        } else if (parser->flags & F_CHUNKED) {
          /* chunked encoding - ignore Content-Length header */
          parser->state = s_chunk_size_start;
        } else {
          if (parser->content_length == 0) {
            /* Content-Length header given but zero: Content-Length: 0\r\n */
            parser->state = NEW_MESSAGE();
            CALLBACK_NOTIFY(message_complete);
          } else if (parser->content_length != ULLONG_MAX) {
            /* Content-Length header given and non-zero */
            parser->state = s_body_identity;
          } else {
            if (parser->type == HTTP_REQUEST ||
                !http_message_needs_eof(parser)) {
              /* Assume content-length 0 - read the next */
              parser->state = NEW_MESSAGE();
              CALLBACK_NOTIFY(message_complete);
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
        uint64_t to_read = MIN(parser->content_length,
                               (uint64_t) ((data + len) - p));

        assert(parser->content_length != 0
            && parser->content_length != ULLONG_MAX);

        /* The difference between advancing content_length and p is because
         * the latter will automaticaly advance on the next loop iteration.
         * Further, if content_length ends up at 0, we want to see the last
         * byte again for our message complete callback.
         */
        MARK(body);
        parser->content_length -= to_read;
        p += to_read - 1;

        if (parser->content_length == 0) {
          parser->state = s_message_done;

          /* Mimic CALLBACK_DATA_NOADVANCE() but with one extra byte.
           *
           * The alternative to doing this is to wait for the next byte to
           * trigger the data callback, just as in every other case. The
           * problem with this is that this makes it difficult for the test
           * harness to distinguish between complete-on-EOF and
           * complete-on-length. It's not clear that this distinction is
           * important for applications, but let's keep it for now.
           */
          CALLBACK_DATA_(body, p - body_mark + 1, p - data);
          goto reexecute_byte;
        }

        break;
      }

      /* read until EOF */
      case s_body_identity_eof:
        MARK(body);
        p = data + len - 1;

        break;

      case s_message_done:
        parser->state = NEW_MESSAGE();
        CALLBACK_NOTIFY(message_complete);
        break;

      case s_chunk_size_start:
      {
        assert(parser->nread == 1);
        assert(parser->flags & F_CHUNKED);

        unhex_val = unhex[(unsigned char)ch];
        if (unhex_val == -1) {
          SET_ERRNO(HPE_INVALID_CHUNK_SIZE);
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

        if (ch == CR) {
          parser->state = s_chunk_size_almost_done;
          break;
        }

        unhex_val = unhex[(unsigned char)ch];

        if (unhex_val == -1) {
          if (ch == ';' || ch == ' ') {
            parser->state = s_chunk_parameters;
            break;
          }

          SET_ERRNO(HPE_INVALID_CHUNK_SIZE);
          goto error;
        }

        t = parser->content_length;
        t *= 16;
        t += unhex_val;

        /* Overflow? Test against a conservative limit for simplicity. */
        if ((ULLONG_MAX - 16) / 16 < parser->content_length) {
          SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
          goto error;
        }

        parser->content_length = t;
        break;
      }

      case s_chunk_parameters:
      {
        assert(parser->flags & F_CHUNKED);
        /* just ignore this shit. TODO check for overflow */
        if (ch == CR) {
          parser->state = s_chunk_size_almost_done;
          break;
        }
        break;
      }

      case s_chunk_size_almost_done:
      {
        assert(parser->flags & F_CHUNKED);
        STRICT_CHECK(ch != LF);

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
        uint64_t to_read = MIN(parser->content_length,
                               (uint64_t) ((data + len) - p));

        assert(parser->flags & F_CHUNKED);
        assert(parser->content_length != 0
            && parser->content_length != ULLONG_MAX);

        /* See the explanation in s_body_identity for why the content
         * length and data pointers are managed this way.
         */
        MARK(body);
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
        STRICT_CHECK(ch != CR);
        parser->state = s_chunk_data_done;
        CALLBACK_DATA(body);
        break;

      case s_chunk_data_done:
        assert(parser->flags & F_CHUNKED);
        STRICT_CHECK(ch != LF);
        parser->nread = 0;
        parser->state = s_chunk_size_start;
        break;

      default:
        assert(0 && "unhandled state");
        SET_ERRNO(HPE_INVALID_INTERNAL_STATE);
        goto error;
    }
  }

  /* Run callbacks for any marks that we have leftover after we ran our of
   * bytes. There should be at most one of these set, so it's OK to invoke
   * them in series (unset marks will not result in callbacks).
   *
   * We use the NOADVANCE() variety of callbacks here because 'p' has already
   * overflowed 'data' and this allows us to correct for the off-by-one that
   * we'd otherwise have (since CALLBACK_DATA() is meant to be run with a 'p'
   * value that's in-bounds).
   */

  assert(((header_field_mark ? 1 : 0) +
          (header_value_mark ? 1 : 0) +
          (url_mark ? 1 : 0)  +
          (body_mark ? 1 : 0) +
          (status_mark ? 1 : 0)) <= 1);

  CALLBACK_DATA_NOADVANCE(header_field);
  CALLBACK_DATA_NOADVANCE(header_value);
  CALLBACK_DATA_NOADVANCE(url);
  CALLBACK_DATA_NOADVANCE(body);
  CALLBACK_DATA_NOADVANCE(status);

  return len;

error:
  if (HTTP_PARSER_ERRNO(parser) == HPE_OK) {
    SET_ERRNO(HPE_UNKNOWN);
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

  if ((parser->flags & F_CHUNKED) || parser->content_length != ULLONG_MAX) {
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
#define XX(num, name, string) #string,
  HTTP_METHOD_MAP(XX)
#undef XX
  };
  return ELEM_AT(method_strings, m, "<unknown>");
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
#define HTTP_STRERROR_GEN(n, s) { "HPE_" #n, s },
static struct {
  const char *name;
  const char *description;
} http_strerror_tab[] = {
  HTTP_ERRNO_MAP(HTTP_STRERROR_GEN)
};
#undef HTTP_STRERROR_GEN
  assert(err < (sizeof(http_strerror_tab)/sizeof(http_strerror_tab[0])));
  return http_strerror_tab[err].name;
}

inline const char *
http_errno_description(enum http_errno err) {
/* Map errno values to strings for human-readable output */
#define HTTP_STRERROR_GEN(n, s) { "HPE_" #n, s },
static struct {
  const char *name;
  const char *description;
} http_strerror_tab[] = {
  HTTP_ERRNO_MAP(HTTP_STRERROR_GEN)
};
#undef HTTP_STRERROR_GEN
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

      if (IS_USERINFO_CHAR(ch)) {
        return s_http_userinfo;
      }
      break;

    case s_http_host_start:
      if (ch == '[') {
        return s_http_host_v6_start;
      }

      if (IS_HOST_CHAR(ch)) {
        return s_http_host;
      }

      break;

    case s_http_host:
      if (IS_HOST_CHAR(ch)) {
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
      if (IS_HEX(ch) || ch == ':' || ch == '.') {
        return s_http_host_v6;
      }

      break;

    case s_http_host_port:
    case s_http_host_port_start:
      if (IS_NUM(ch)) {
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
  if (HTTP_PARSER_ERRNO(parser) == HPE_OK ||
      HTTP_PARSER_ERRNO(parser) == HPE_PAUSED) {
    SET_ERRNO((paused) ? HPE_PAUSED : HPE_OK);
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
  return HTTP_PARSER_VERSION_MAJOR * 0x10000 |
         HTTP_PARSER_VERSION_MINOR * 0x00100 |
         HTTP_PARSER_VERSION_PATCH * 0x00001;
}

#undef HTTP_METHOD_MAP
#undef HTTP_ERRNO_MAP
#undef SET_ERRNO
#undef CALLBACK_NOTIFY_
#undef CALLBACK_NOTIFY
#undef CALLBACK_NOTIFY_NOADVANCE
#undef CALLBACK_DATA_
#undef CALLBACK_DATA
#undef CALLBACK_DATA_NOADVANCE
#undef MARK
#undef PROXY_CONNECTION
#undef CONNECTION
#undef CONTENT_LENGTH
#undef TRANSFER_ENCODING
#undef UPGRADE
#undef CHUNKED
#undef KEEP_ALIVE
#undef CLOSE
#undef PARSING_HEADER
#undef CR
#undef LF
#undef LOWER
#undef IS_ALPHA
#undef IS_NUM
#undef IS_ALPHANUM
#undef IS_HEX
#undef IS_MARK
#undef IS_USERINFO_CHAR
#undef TOKEN
#undef IS_URL_CHAR
#undef IS_HOST_CHAR
#undef start_state
#undef STRICT_CHECK
#undef NEW_MESSAGE

#ifdef __cplusplus
}
#endif
#endif



#pragma once

#include <string>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/filesystem.hpp>

namespace crow
{
    // code from http://stackoverflow.com/questions/2838524/use-boost-date-time-to-parse-and-create-http-dates
    class DateTime
    {
        public:
            DateTime()
                : m_dt(boost::local_time::local_sec_clock::local_time(boost::local_time::time_zone_ptr()))
            {
            }
            DateTime(const std::string& path)
                : DateTime()
            {
                from_file(path);
            }

            // return datetime string
            std::string str()
            {
                static const std::locale locale_(std::locale::classic(), new boost::local_time::local_time_facet("%a, %d %b %Y %H:%M:%S GMT") );
                std::string result;
                try
                {
                    std::stringstream ss;
                    ss.imbue(locale_);
                    ss << m_dt;
                    result = ss.str();
                }
                catch (std::exception& e)
                {
                    std::cerr << "Exception: " << e.what() << std::endl;
                }
                return result;
            }

            // update datetime from file mod date
            std::string from_file(const std::string& path)
            {
                try
                {
                    boost::filesystem::path p(path);
                    boost::posix_time::ptime pt = boost::posix_time::from_time_t(
                            boost::filesystem::last_write_time(p));
                    m_dt = boost::local_time::local_date_time(pt, boost::local_time::time_zone_ptr());
                }
                catch (std::exception& e)
                {
                    std::cout << "Exception: " << e.what() << std::endl;
                }
                return str();
            }

            // parse datetime string
            void parse(const std::string& dt)
            {
                static const std::locale locale_(std::locale::classic(), new boost::local_time::local_time_facet("%a, %d %b %Y %H:%M:%S GMT") );
                std::stringstream ss(dt);
                ss.imbue(locale_);
                ss >> m_dt;
            }

            // boolean equal operator
            friend bool operator==(const DateTime& left, const DateTime& right)
            {
                return (left.m_dt == right.m_dt);
            }

        private:
            boost::local_time::local_date_time m_dt;
    };
}



// settings for crow
// TODO - replace with runtime config. libucl?

/* #ifdef - enables debug mode */
#define CROW_ENABLE_DEBUG

/* #ifdef - enables logging */
#define CROW_ENABLE_LOGGING

/* #define - specifies log level */
/*
	DEBUG 		= 0
	INFO 		= 1
	WARNING		= 2
	ERROR		= 3
	CRITICAL 	= 4
*/
#define CROW_LOG_LEVEL 0



#pragma once

#include <string>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>




using namespace std;

namespace crow
{
    enum class LogLevel
    {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL,
    };

    class ILogHandler {
        public:
            virtual void log(string message, LogLevel level) = 0;
    };

    class CerrLogHandler : public ILogHandler {
        public:
            void log(string message, LogLevel level) override {
                cerr << message;
            }
    };

    class logger {

        private:
            //
            static string timestamp()
            {
                char date[32];
                  time_t t = time(0);
                  strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", gmtime(&t));
                  return string(date);
            }

        public:


            logger(string prefix, LogLevel level) : level_(level) {
    #ifdef CROW_ENABLE_LOGGING
                    stringstream_ << "(" << timestamp() << ") [" << prefix << "] ";
    #endif

            }
            ~logger() {
    #ifdef CROW_ENABLE_LOGGING
                if(level_ >= get_current_log_level()) {
                    stringstream_ << endl;
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
            ostringstream stringstream_;
            LogLevel level_;
    };
}

#define CROW_LOG_CRITICAL    crow::logger("CRITICAL", crow::LogLevel::CRITICAL)
#define CROW_LOG_ERROR        crow::logger("ERROR   ", crow::LogLevel::ERROR)
#define CROW_LOG_WARNING    crow::logger("WARNING ", crow::LogLevel::WARNING)
#define CROW_LOG_INFO        crow::logger("INFO    ", crow::LogLevel::INFO)
#define CROW_LOG_DEBUG        crow::logger("DEBUG   ", crow::LogLevel::DEBUG)






#pragma once

#include <cstdint>
#include <stdexcept>

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
                *a == 0 || *b == 0
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

	}
}



#pragma once

#include <string>
#include <stdexcept>



namespace crow
{
    enum class HTTPMethod
    {
        DELETE,
        GET,
        HEAD,
        POST,
        PUT,
        CONNECT,
        OPTIONS,
        TRACE,
    };

    inline std::string method_name(HTTPMethod method)
    {
        switch(method)
        {
            case HTTPMethod::DELETE:
                return "DELETE";
            case HTTPMethod::GET:
                return "GET";
            case HTTPMethod::HEAD:
                return "HEAD";
            case HTTPMethod::POST:
                return "POST";
            case HTTPMethod::PUT:
                return "PUT";
            case HTTPMethod::CONNECT:
                return "CONNECT";
            case HTTPMethod::OPTIONS:
                return "OPTIONS";
            case HTTPMethod::TRACE:
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

constexpr crow::HTTPMethod operator "" _method(const char* str, size_t len)
{
    return
        crow::black_magic::is_equ_p(str, "GET", 3) ? crow::HTTPMethod::GET :
        crow::black_magic::is_equ_p(str, "DELETE", 6) ? crow::HTTPMethod::DELETE :
        crow::black_magic::is_equ_p(str, "HEAD", 4) ? crow::HTTPMethod::HEAD :
        crow::black_magic::is_equ_p(str, "POST", 4) ? crow::HTTPMethod::POST :
        crow::black_magic::is_equ_p(str, "PUT", 3) ? crow::HTTPMethod::PUT :
        crow::black_magic::is_equ_p(str, "OPTIONS", 7) ? crow::HTTPMethod::OPTIONS :
        crow::black_magic::is_equ_p(str, "CONNECT", 7) ? crow::HTTPMethod::CONNECT :
        crow::black_magic::is_equ_p(str, "TRACE", 5) ? crow::HTTPMethod::TRACE :
        throw std::runtime_error("invalid http method");
};




#pragma once




namespace crow
{
    struct request
    {
        HTTPMethod method;
        std::string url;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };
}



#pragma once

#include <string>
#include <unordered_map>
#include <boost/algorithm/string.hpp>




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
            self->url.insert(self->url.end(), at, at+length);
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
                        boost::algorithm::to_lower(self->header_field);
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
                boost::algorithm::to_lower(self->header_field);
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
            self->process_message();
            return 0;
        }
        HTTPParser(Handler* handler) :
            settings_ {
                on_message_begin,
                on_url,
                nullptr,
                on_header_field,
                on_header_value,
                on_headers_complete,
                on_body,
                on_message_complete,
            },
            handler_(handler)
        {
            http_parser_init(this, HTTP_REQUEST);
        }

        // return false on error
        bool feed(const char* buffer, int length)
        {
            int nparsed = http_parser_execute(this, &settings_, buffer, length);
            return nparsed == length;
        }

        bool done()
        {
            int nparsed = http_parser_execute(this, &settings_, nullptr, 0);
            return nparsed == 0;
        }

        void clear()
        {
            url.clear();
            header_building_state = 0;
            header_field.clear();
            header_value.clear();
            headers.clear();
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
            return request{(HTTPMethod)method, std::move(url), std::move(headers), std::move(body)};
        }

        bool check_version(int major, int minor) const
        {
            return http_major == major && http_minor == minor;
        }

        std::string url;
        int header_building_state = 0;
        std::string header_field;
        std::string header_value;
        std::unordered_map<std::string, std::string> headers;
        std::string body;

        http_parser_settings settings_;

        Handler* handler_;
    };
}



#pragma once
#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>
#include <atomic>
#include <chrono>
#include <array>















namespace crow
{
    using namespace boost;
    using tcp = asio::ip::tcp;
#ifdef CROW_ENABLE_DEBUG
    static int connectionCount;
#endif
    template <typename Handler>
    class Connection : public std::enable_shared_from_this<Connection<Handler>>
    {
    public:
        Connection(tcp::socket&& socket, Handler* handler, const std::string& server_name) 
            : socket_(std::move(socket)), 
            handler_(handler), 
            parser_(this), 
            server_name_(server_name),
            deadline_(socket_.get_io_service()),
            address_str_(boost::lexical_cast<std::string>(socket_.remote_endpoint()))
        {
#ifdef CROW_ENABLE_DEBUG
            connectionCount ++;
            CROW_LOG_DEBUG << "Connection open, total " << connectionCount << ", " << this;
#endif
        }
        
        ~Connection()
        {
            res.complete_request_handler_ = nullptr;
#ifdef CROW_ENABLE_DEBUG
            connectionCount --;
            CROW_LOG_DEBUG << "Connection closed, total " << connectionCount << ", " << this;
#endif
        }

        void start()
        {
            auto self = this->shared_from_this();
            start_deadline();

            do_read();
        }

        void handle_header()
        {
            // HTTP 1.1 Expect: 100-continue
            if (parser_.check_version(1, 1) && parser_.headers.count("expect") && parser_.headers["expect"] == "100-continue")
            {
                buffers_.clear();
                static std::string expect_100_continue = "HTTP/1.1 100 Continue\r\n\r\n";
                buffers_.emplace_back(expect_100_continue.data(), expect_100_continue.size());
                do_write();
            }
        }

        void handle()
        {
            bool is_invalid_request = false;

            request req = parser_.to_request();
            if (parser_.check_version(1, 0))
            {
                // HTTP/1.0
                if (!(req.headers.count("connection") && boost::iequals(req.headers["connection"],"Keep-Alive")))
                    close_connection_ = true;
            }
            else if (parser_.check_version(1, 1))
            {
                // HTTP/1.1
                if (req.headers.count("connection") && req.headers["connection"] == "close")
                    close_connection_ = true;
                if (!req.headers.count("host"))
                {
                    is_invalid_request = true;
                    res = response(400);
                }
            }

            CROW_LOG_INFO << "Request: " << address_str_ << " " << this << " HTTP/" << parser_.http_major << "." << parser_.http_minor << ' '
             << method_name(req.method) << " " << req.url;


            if (!is_invalid_request)
            {
                deadline_.cancel();
                auto self = this->shared_from_this();
                res.complete_request_handler_ = [self]{ self->complete_request(); };
                res.is_alive_helper_ = [this]()->bool{ return socket_.is_open(); };
                handler_->handle(req, res);
            }
			else
			{
				complete_request();
			}
        }

        void complete_request()
        {
            CROW_LOG_INFO << "Response: " << this << ' ' << res.code << ' ' << close_connection_;

            auto self = this->shared_from_this();
            res.complete_request_handler_ = nullptr;

			if (!socket_.is_open())
				return;


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
            buffers_.reserve(4*(res.headers.size()+4)+3);

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

            bool has_content_length = false;
            bool has_date = false;
            bool has_server = false;

            for(auto& kv : res.headers)
            {
                buffers_.emplace_back(kv.first.data(), kv.first.size());
                buffers_.emplace_back(seperator.data(), seperator.size());
                buffers_.emplace_back(kv.second.data(), kv.second.size());
                buffers_.emplace_back(crlf.data(), crlf.size());

                if (boost::iequals(kv.first, "content-length"))
                    has_content_length = true;
                if (boost::iequals(kv.first, "date"))
                    has_date = true;
                if (boost::iequals(kv.first, "server"))
                    has_server = true;
            }

            if (!has_content_length)
            {
                content_length_ = std::to_string(res.body.size());
                static std::string content_length_tag = "Content-Length: ";
                buffers_.emplace_back(content_length_tag.data(), content_length_tag.size());
                buffers_.emplace_back(content_length_.data(), content_length_.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
            }
            if (!has_server)
            {
                static std::string server_tag = "Server: ";
                buffers_.emplace_back(server_tag.data(), server_tag.size());
                buffers_.emplace_back(server_name_.data(), server_name_.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
            }
            if (!has_date)
            {
                static std::string date_tag = "Date: ";
                date_str_ = get_cached_date_str();
                buffers_.emplace_back(date_tag.data(), date_tag.size());
                buffers_.emplace_back(date_str_.data(), date_str_.size());
                buffers_.emplace_back(crlf.data(), crlf.size());
            }

            buffers_.emplace_back(crlf.data(), crlf.size());
            buffers_.emplace_back(res.body.data(), res.body.size());

            do_write();
            res.clear();
        }

    private:
        static std::string get_cached_date_str()
        {
            using namespace std::chrono;
            thread_local auto last = steady_clock::now();
            thread_local std::string date_str = DateTime().str();

            if (steady_clock::now() - last >= seconds(1))
            {
                last = steady_clock::now();
                date_str = DateTime().str();
            }
            return date_str;
        }

        void do_read()
        {
            auto self = this->shared_from_this();
            socket_.async_read_some(boost::asio::buffer(buffer_), 
                [self, this](const boost::system::error_code& ec, std::size_t bytes_transferred)
                {
                    bool error_while_reading = true;
                    if (!ec)
                    {
                        bool ret = parser_.feed(buffer_.data(), bytes_transferred);
                        if (ret)
                        {
                            do_read();
                            error_while_reading = false;
                        }
                    }

                    if (error_while_reading)
                    {
                        deadline_.cancel();
                        parser_.done();
                        socket_.close();
                    }
                    else
                    {
                        start_deadline();
                    }
                });
        }

        void do_write()
        {
            auto self = this->shared_from_this();
            boost::asio::async_write(socket_, buffers_, 
                [&, self](const boost::system::error_code& ec, std::size_t bytes_transferred)
                {
                    if (!ec)
                    {
                        start_deadline();
                        if (close_connection_)
                        {
                            socket_.close();
                        }
                    }
                });
        }

        void start_deadline(int timeout = 5)
        {
            deadline_.expires_from_now(boost::posix_time::seconds(timeout));
            auto self = this->shared_from_this();
            deadline_.async_wait([self, this](const boost::system::error_code& ec) 
            {
                if (ec || !socket_.is_open())
                {
                    return;
                }
                bool is_deadline_passed = deadline_.expires_at() <= boost::asio::deadline_timer::traits_type::now();
                if (is_deadline_passed)
                {
                    socket_.close();
                }
            });
        }

    private:
        tcp::socket socket_;
        Handler* handler_;

        std::array<char, 8192> buffer_;

        HTTPParser<Connection> parser_;
        response res;

        bool close_connection_ = false;

        const std::string& server_name_;
        std::vector<boost::asio::const_buffer> buffers_;

        std::string content_length_;
        std::string date_str_;

        boost::asio::deadline_timer deadline_;
        std::string address_str_;
    };

}



#pragma once

#include <boost/asio.hpp>
#include <cstdint>
#include <atomic>
#include <future>

#include <memory>








namespace crow
{
    using namespace boost;
    using tcp = asio::ip::tcp;
    
    template <typename Handler>
    class Server
    {
    public:
        Server(Handler* handler, uint16_t port, uint16_t concurrency = 1)
            : acceptor_(io_service_, tcp::endpoint(asio::ip::address(), port)), 
            socket_(io_service_), 
            signals_(io_service_, SIGINT, SIGTERM),
            handler_(handler), 
            concurrency_(concurrency),
            port_(port)
        {
            do_accept();
        }

        void run()
        {
            std::vector<std::future<void>> v;
            for(uint16_t i = 0; i < concurrency_; i ++)
                v.push_back(
                        std::async(std::launch::async, [this]{io_service_.run();})
                        );

            CROW_LOG_INFO << server_name_ << " server is running, local port " << port_;

            signals_.async_wait(
                [&](const boost::system::error_code& error, int signal_number){
                    io_service_.stop();
                });
            
        }

        void stop()
        {
            io_service_.stop();
        }

    private:
        void do_accept()
        {
            acceptor_.async_accept(socket_, 
                [this](boost::system::error_code ec)
                {
                    if (!ec)
                    {
                        auto p = std::make_shared<Connection<Handler>>(std::move(socket_), handler_, server_name_);
                        p->start();
                    }
                    do_accept();
                });
        }

    private:
        asio::io_service io_service_;
        tcp::acceptor acceptor_;
        tcp::socket socket_;
        boost::asio::signal_set signals_;

        Handler* handler_;
        uint16_t concurrency_{1};
        std::string server_name_ = "Crow/0.1";
        uint16_t port_;
    };
}



#pragma once

#include <cstdint>
#include <utility>
#include <tuple>
#include <unordered_map>
#include <memory>
#include <boost/lexical_cast.hpp>












namespace crow
{
    class BaseRule
    {
    public:
        virtual ~BaseRule()
        {
        }
        
        virtual void validate() = 0;

        virtual void handle(const request&, response&, const routing_params&) = 0;

    protected:

    };

    template <typename ... Args>
    class TaggedRule : public BaseRule
    {
    private:
        template <typename H1, typename H2, typename H3>
        struct call_params
        {
            H1& handler;
            H2& handler_with_req;
            H3& handler_with_req_res;
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
                if (cparams.handler) 
                {
                    cparams.res = cparams.handler(
                        cparams.params.template get<typename Args1::type>(Args1::pos)... 
                    );
                    cparams.res.end();
                    return;
                }
                if (cparams.handler_with_req)
                {
                    cparams.res = cparams.handler_with_req(
                        cparams.req,
                        cparams.params.template get<typename Args1::type>(Args1::pos)... 
                    );
                    cparams.res.end();
                    return;
                }
                if (cparams.handler_with_req_res)
                {
                    cparams.handler_with_req_res(
                        cparams.req,
                        cparams.res,
                        cparams.params.template get<typename Args1::type>(Args1::pos)... 
                    );
                    return;
                }
#ifdef CROW_ENABLE_LOGGING
                std::cerr << "ERROR cannot find handler" << std::endl;
#endif
                // we already found matched url; this is server error
                cparams.res = response(500);
            }
        };
    public:
        using self_t = TaggedRule<Args...>;
        TaggedRule(std::string rule)
            : rule_(std::move(rule))
        {
        }
        
        self_t& name(std::string name) noexcept
        {
            name_ = std::move(name);
            return *this;
        }

        self_t& methods(HTTPMethod method)
        {
            methods_ = 1<<(int)method;
        }

        template <typename ... MethodArgs>
        self_t& methods(HTTPMethod method, MethodArgs ... args_method)
        {
            methods(args_method...);
            methods_ |= 1<<(int)method;
        }

        void validate()
        {
            if (!handler_ && !handler_with_req_ && !handler_with_req_res_)
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
                "Handler type is mismatched with URL paramters");
            static_assert(!std::is_same<void, decltype(f(std::declval<Args>()...))>::value, 
                "Handler function cannot have void return type; valid return types: string, int, crow::resposne, crow::json::wvalue");

                handler_ = [f = std::move(f)](Args ... args){
                    return response(f(args...));
                };
                handler_with_req_ = nullptr;
                handler_with_req_res_ = nullptr;
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
                "Handler type is mismatched with URL paramters");
            static_assert(!std::is_same<void, decltype(f(std::declval<crow::request>(), std::declval<Args>()...))>::value, 
                "Handler function cannot have void return type; valid return types: string, int, crow::resposne, crow::json::wvalue");

                handler_with_req_ = [f = std::move(f)](const crow::request& req, Args ... args){
                    return response(f(req, args...));
                };
                handler_ = nullptr;
                handler_with_req_res_ = nullptr;
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
                "Handler type is mismatched with URL paramters");
            static_assert(std::is_same<void, decltype(f(std::declval<crow::request>(), std::declval<crow::response&>(), std::declval<Args>()...))>::value, 
                "Handler function with response argument should have void return type");

                handler_with_req_res_ = std::move(f);
                //[f = std::move(f)](const crow::request& req, crow::response& res, Args ... args){
                //    f(req, response, args...);
                //};
                handler_ = nullptr;
                handler_with_req_ = nullptr;
        }

        template <typename Func>
        void operator()(std::string name, Func&& f)
        {
            name_ = std::move(name);
            (*this).template operator()<Func>(std::forward(f));
        }

        void handle(const request& req, response& res, const routing_params& params) override
        {
            call<
                call_params<
                    decltype(handler_), 
                    decltype(handler_with_req_),
                    decltype(handler_with_req_res_)>, 
                0, 0, 0, 0, 
                black_magic::S<Args...>, 
                black_magic::S<>
            >()(
                call_params<
                    decltype(handler_), 
                    decltype(handler_with_req_),
                    decltype(handler_with_req_res_)>
                {handler_, handler_with_req_, handler_with_req_res_, params, req, res}
            );
        }

    private:
        std::function<response(Args...)> handler_;
        std::function<response(const crow::request&, Args...)> handler_with_req_;
        std::function<void(const crow::request&, crow::response&, Args...)> handler_with_req_res_;

        std::string rule_;
        std::string name_;
        uint32_t methods_{1<<(int)HTTPMethod::GET};

        template <typename T, int Pos>
        struct call_pair
        {
            using type = T;
            static const int pos = Pos;
        };

        friend class Router;
    };

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

        std::pair<unsigned, routing_params> find(const request& req, const Node* node = nullptr, unsigned pos = 0, routing_params* params = nullptr) const
        {
            routing_params empty;
            if (params == nullptr)
                params = &empty;

            unsigned found{};
            routing_params match_params;

            if (node == nullptr)
                node = head();
            if (pos == req.url.size())
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
                char c = req.url[pos];
                if ((c >= '0' && c <= '9') || c == '+' || c == '-')
                {
                    char* eptr;
                    errno = 0;
                    long long int value = strtoll(req.url.data()+pos, &eptr, 10);
                    if (errno != ERANGE && eptr != req.url.data()+pos)
                    {
                        params->int_params.push_back(value);
                        auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::INT]], eptr - req.url.data(), params);
                        update_found(ret);
                        params->int_params.pop_back();
                    }
                }
            }

            if (node->param_childrens[(int)ParamType::UINT])
            {
                char c = req.url[pos];
                if ((c >= '0' && c <= '9') || c == '+')
                {
                    char* eptr;
                    errno = 0;
                    unsigned long long int value = strtoull(req.url.data()+pos, &eptr, 10);
                    if (errno != ERANGE && eptr != req.url.data()+pos)
                    {
                        params->uint_params.push_back(value);
                        auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::UINT]], eptr - req.url.data(), params);
                        update_found(ret);
                        params->uint_params.pop_back();
                    }
                }
            }

            if (node->param_childrens[(int)ParamType::DOUBLE])
            {
                char c = req.url[pos];
                if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.')
                {
                    char* eptr;
                    errno = 0;
                    double value = strtod(req.url.data()+pos, &eptr);
                    if (errno != ERANGE && eptr != req.url.data()+pos)
                    {
                        params->double_params.push_back(value);
                        auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::DOUBLE]], eptr - req.url.data(), params);
                        update_found(ret);
                        params->double_params.pop_back();
                    }
                }
            }

            if (node->param_childrens[(int)ParamType::STRING])
            {
                size_t epos = pos;
                for(; epos < req.url.size(); epos ++)
                {
                    if (req.url[epos] == '/')
                        break;
                }

                if (epos != pos)
                {
                    params->string_params.push_back(req.url.substr(pos, epos-pos));
                    auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::STRING]], epos, params);
                    update_found(ret);
                    params->string_params.pop_back();
                }
            }

            if (node->param_childrens[(int)ParamType::PATH])
            {
                size_t epos = req.url.size();

                if (epos != pos)
                {
                    params->string_params.push_back(req.url.substr(pos, epos-pos));
                    auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::PATH]], epos, params);
                    update_found(ret);
                    params->string_params.pop_back();
                }
            }

            for(auto& kv : node->children)
            {
                const std::string& fragment = kv.first;
                const Node* child = &nodes_[kv.second];

                if (req.url.compare(pos, fragment.size(), fragment) == 0)
                {
                    auto ret = find(req, child, pos + fragment.size(), params);
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
        Router() : rules_(1) {}
        template <uint64_t N>
        typename black_magic::arguments<N>::type::template rebind<TaggedRule>& new_rule_tagged(const std::string& rule)
        {
            using RuleT = typename black_magic::arguments<N>::type::template rebind<TaggedRule>;
            auto ruleObject = new RuleT(rule);
            rules_.emplace_back(ruleObject);
            trie_.add(rule, rules_.size() - 1);
            return *ruleObject;
        }

        void validate()
        {
            trie_.validate();
            for(auto& rule:rules_)
            {
                if (rule)
                    rule->validate();
            }
        }

        void handle(const request& req, response& res)
        {
            auto found = trie_.find(req);

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

            CROW_LOG_DEBUG << "Matched rule '" << ((TaggedRule<>*)rules_[rule_index].get())->rule_ << "'";

            rules_[rule_index]->handle(req, res, found.second);
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

#include <string>
#include <functional>
#include <memory>
#include <future>
#include <cstdint>
#include <type_traits>
#include <thread>




 







// TEST
#include <iostream>

#define CROW_ROUTE(app, url) app.route<crow::black_magic::get_parameter_tag(url)>(url)

namespace crow
{
    class Crow
    {
    public:
        using self_t = Crow;
        Crow()
        {
        }

        void handle(const request& req, response& res)
        {
            return router_.handle(req, res);
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
            Server<self_t> server(this, port_, concurrency_);
            server.run();
        }

        void debug_print()
        {
            CROW_LOG_DEBUG << "Routing:";
            router_.debug_print();
        }

    private:
        uint16_t port_ = 80;
        uint16_t concurrency_ = 1;

        Router router_;
    };
    using App = Crow;
};



