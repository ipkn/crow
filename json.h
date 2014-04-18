#pragma once

//#define FLASKPP_JSON_NO_ERROR_CHECK

#include <string>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <memory>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/operators.hpp>

#ifdef __GNUG__
#define flask_json_likely(x) __builtin_expect(x, 1)
#define flask_json_unlikely(x) __builtin_expect(x, 0)
#else
#ifdef __clang__
#define flask_json_likely(x) __builtin_expect(x, 1)
#define flask_json_unlikely(x) __builtin_expect(x, 0)
#else
#define flask_json_likely(x) x
#define flask_json_unlikely(x) x
#endif
#endif


namespace flask
{
    namespace json
    {
        std::string escape(const std::string& str)
        {
            // TODO
            return str;
        }
        std::string unescape(const std::string& str)
        {
            // TODO
            return str;
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
        rvalue load_copy(const char* data, size_t size);

        namespace detail 
        {

            struct r_string 
                : boost::less_than_comparable<r_string>,
                boost::less_than_comparable<r_string, std::string>,
                boost::equality_comparable<r_string>,
                boost::equality_comparable<r_string, std::string>
            {
                r_string() {};
                r_string(const char* s, uint32_t length, uint8_t has_escaping) 
                    : s_(s), length_(length), has_escaping_(has_escaping) 
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
                    length_ = r.length_;
                    has_escaping_ = r.has_escaping_;
                    owned_ = r.owned_;
                    return *this;
                }

                r_string& operator = (const r_string& r)
                {
                    s_ = r.s_;
                    length_ = r.length_;
                    has_escaping_ = r.has_escaping_;
                    owned_ = 0;
                    return *this;
                }

                operator std::string () const
                {
                    return unescape();
                }

                std::string unescape() const
                {
                    // TODO
                    return std::string(begin(), end());
                }

                const char* begin() const { return s_; }
                const char* end() const { return s_+length_; }

                using iterator = const char*;
                using const_iterator = const char*;

                const char* s_;
                uint32_t length_;
                uint8_t has_escaping_;
                uint8_t owned_{0};
                friend std::ostream& operator << (std::ostream& os, const r_string& s)
                {
                    os << (std::string)s;
                    return os;
                }
            private:
                void force(const char* s, uint32_t length)
                {
                    s_ = s;
                    length_ = length;
                    owned_ = 1;
                }
                friend rvalue flask::json::load_copy(const char* data, size_t size);
            };

            bool operator < (const r_string& l, const r_string& r)
            {
                return boost::lexicographical_compare(l,r);
            }

            bool operator < (const r_string& l, const std::string& r)
            {
                return boost::lexicographical_compare(l,r);
            }

            bool operator > (const r_string& l, const std::string& r)
            {
                return boost::lexicographical_compare(r,l);
            }

            bool operator == (const r_string& l, const r_string& r)
            {
                return boost::equals(l,r);
            }

            bool operator == (const r_string& l, const std::string& r)
            {
                return boost::equals(l,r);
            }
        }

        class rvalue
        {
            static const int error_bit = 4;
        public:
		    rvalue() noexcept : option_{error_bit} 
			{}
            rvalue(type t) noexcept
                : lsize_{}, lremain_{}, t_{t}
            {}
            rvalue(type t, const char* s, const char* e)  noexcept
                : start_{s},
                end_{e},
                t_{t}
            {}
            rvalue(type t, const char* s, const char* e, uint8_t option)  noexcept
                : start_{s},
                end_{e},
                t_{t},
                option_{option}
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
#ifndef FLASKPP_JSON_NO_ERROR_CHECK
                if (option_ & error_bit)
                {
                    throw std::runtime_error("invalid json object");
                }
#endif
                return t_;
            }

            int64_t i() const
            {
#ifndef FLASKPP_JSON_NO_ERROR_CHECK
                if (t() != type::Number)
                    throw std::runtime_error("value is not number");
#endif
                return boost::lexical_cast<int64_t>(start_, end_-start_);
            }

            double d() const
            {
#ifndef FLASKPP_JSON_NO_ERROR_CHECK
                if (t() != type::Number)
                    throw std::runtime_error("value is not number");
#endif
                return boost::lexical_cast<double>(start_, end_-start_);
            }

            detail::r_string s_raw() const
            {
#ifndef FLASKPP_JSON_NO_ERROR_CHECK
                if (t() != type::String)
                    throw std::runtime_error("value is not string");
#endif
                return detail::r_string{start_, (uint32_t)(end_-start_), has_escaping()};
            }

            detail::r_string s() const
            {
#ifndef FLASKPP_JSON_NO_ERROR_CHECK
                if (t() != type::String)
                    throw std::runtime_error("value is not string");
#endif
                return detail::r_string{start_, (uint32_t)(end_-start_), has_escaping()};
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

            rvalue* begin() const 
			{ 
#ifndef FLASKPP_JSON_NO_ERROR_CHECK
                if (t() != type::Object && t() != type::List)
                    throw std::runtime_error("value is not a container");
#endif
				return l_.get(); 
			}
            rvalue* end() const 
			{ 
#ifndef FLASKPP_JSON_NO_ERROR_CHECK
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
#ifndef FLASKPP_JSON_NO_ERROR_CHECK
                if (t() != type::Object && t() != type::List)
                    throw std::runtime_error("value is not a container");
#endif
                return lsize_;
            }

			const rvalue& operator[](int index) const
			{
#ifndef FLASKPP_JSON_NO_ERROR_CHECK
                if (t() != type::List)
                    throw std::runtime_error("value is not a list");
				if (index >= (int)lsize_ || index < 0)
                    throw std::runtime_error("list out of bound");
#endif
				return l_[index];
			}

			const rvalue& operator[](size_t index) const
			{
#ifndef FLASKPP_JSON_NO_ERROR_CHECK
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
#ifndef FLASKPP_JSON_NO_ERROR_CHECK
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
#ifndef FLASKPP_JSON_NO_ERROR_CHECK
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
            bool has_escaping() const
            {
                return (option_&1)!=0;
            }
            bool is_cached() const
            {
                return (option_&2)!=0;
            }
            void set_cached() const
            {
                option_ |= 2;
            }
            void copy_l(const rvalue& r)
            {
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

            const char* start_;
            const char* end_;
            detail::r_string key_;
            std::unique_ptr<rvalue[]> l_;
            uint32_t lsize_;
            uint16_t lremain_;
            type t_;
            mutable uint8_t option_{0};

            friend rvalue load_nocopy(const char* data, size_t size);
            friend rvalue load_copy(const char* data, size_t size);
            friend std::ostream& operator <<(std::ostream& os, const rvalue& r)
            {
                switch(r.t_)
                {

                case type::Null: os << "null"; break;
                case type::False: os << "false"; break;
                case type::True: os << "true"; break;
                case type::Number: os << r.d(); break;
                case type::String: os << '"' << r.s_raw() << '"'; break;
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
                            os << '"' << escape(r.key_) << '"';
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

        bool operator == (const rvalue& l, const std::string& r)
        {
            return l.s() == r;
        }

        bool operator == (const std::string& l, const rvalue& r)
        {
            return l == r.s();
        }

        bool operator != (const rvalue& l, const std::string& r)
        {
            return l.s() != r;
        }

        bool operator != (const std::string& l, const rvalue& r)
        {
            return l != r.s();
        }

        bool operator == (const rvalue& l, double r)
        {
            return l.d() == r;
        }

        bool operator == (double l, const rvalue& r)
        {
            return l == r.d();
        }

        bool operator != (const rvalue& l, double r)
        {
            return l.d() != r;
        }

        bool operator != (double l, const rvalue& r)
        {
            return l != r.d();
        }


        //inline rvalue decode(const std::string& s)
        //{
        //}
        inline rvalue load_nocopy(const char* data, size_t size)
        {
            //static const char* escaped = "\"\\/\b\f\n\r\t";
			struct Parser
			{
				Parser(const char* data, size_t size)
					: data(data)
				{
				}

            	bool consume(char c)
                {
                    if (flask_json_unlikely(*data != c))
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
                    if (flask_json_unlikely(!consume('"')))
                        return {};
                    const char* start = data;
                    uint8_t has_escaping = 0;
                    while(1)
                    {
                        if (flask_json_likely(*data != '"' && *data != '\\'))
                        {
                            data ++;
                        }
                        else if (*data == '"')
                        {
                            data++;
                            return {type::String, start, data-1, has_escaping};
                        }
                        else if (*data == '\\')
                        {
                            has_escaping = 1;
                            // TODO
                            data++;
                            switch(*data)
                            {
                                case 'u':
                                    data += 4;
                                    // TODO
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
                    }
                    return {};
                }

            	rvalue decode_list()
                {
					rvalue ret(type::List);
					if (flask_json_unlikely(!consume('[')))
                    {
                        ret.set_error();
						return ret;
                    }
					ws_skip();
					if (flask_json_unlikely(*data == ']'))
					{
						data++;
						return ret;
					}

					while(1)
					{
                        auto v = decode_value();
						if (flask_json_unlikely(!v))
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
                        if (flask_json_unlikely(!consume(',')))
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
                    const char* start = data;

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
                    while(flask_json_likely(state != Invalid))
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
                                if (flask_json_likely(state == NumberParsingState::ZeroFirst || 
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
                    if (flask_json_unlikely(!consume('{')))
                    {
                        ret.set_error();
                        return ret;
                    }

					ws_skip();

                    if (flask_json_unlikely(*data == '}'))
                    {
                        data++;
                        return ret;
                    }

                    while(1)
                    {
                        auto t = decode_string();
                        if (flask_json_unlikely(!t))
                        {
                            ret.set_error();
                            break;
                        }

						ws_skip();
                        if (flask_json_unlikely(!consume(':')))
                        {
                            ret.set_error();
                            break;
                        }

						// TODO caching key to speed up (flyweight?)
                        auto key = t.s();

						ws_skip();
                        auto v = decode_value();
                        if (flask_json_unlikely(!v))
                        {
                            ret.set_error();
                            break;
                        }
						ws_skip();

                        v.key_ = std::move(key);
                        ret.emplace_back(std::move(v));
                        if (flask_json_unlikely(*data == '}'))
                        {
                            data++;
                            break;
                        }
                        if (flask_json_unlikely(!consume(',')))
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
					//auto ret = decode_object();
                    ws_skip();
					auto ret = decode_value();
					ws_skip();
                    if (*data != '\0')
                        ret.set_error();
					return ret;
				}

				const char* data;
			};
			return Parser(data, size).parse();
        }
        inline rvalue load_copy(const char* data, size_t size)
        {
            char* s = new char[size+1];
            memcpy(s, data, size+1);
            auto ret = load_nocopy(s, size);
            if (ret)
                ret.key_.force(s, size);
            else
                delete[] s;
            return ret;
        }

        inline rvalue load_copy(const char* data)
        {
            return load_copy(data, strlen(data));
        }

        inline rvalue load_copy(const std::string& str)
        {
            return load_copy(str.data(), str.size());
        }

        inline rvalue load_nocopy(const char* data)
        {
            return load_nocopy(data, strlen(data));
        }

        inline rvalue load_nocopy(const std::string& str)
        {
            return load_nocopy(str.data(), str.size());
        }


        class wvalue
        {
        public:
            type t{type::Null};
        private:
            double d;
            std::string s;
            std::unique_ptr<std::vector<wvalue>> l;
            std::unique_ptr<std::unordered_map<std::string, wvalue>> o;
        public:

            wvalue() {}

            wvalue(wvalue&& r)
            {
                *this = std::move(r);
            }

            wvalue& operator = (wvalue&& r)
            {
                t = r.t;
                d = r.d;
                s = std::move(r.s);
                l = std::move(r.l);
                o = std::move(r.o);
                return *this;
            }

            void clear()
            {
                t = type::Null;
                l.reset();
                o.reset();
            }

            void reset()
            {
                t = type::Null;
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
                    t = type::True;
                else
                    t = type::False;
                return *this;
            }

            wvalue& operator = (double value)
            {
                reset();
                t = type::Number;
                d = value;
                return *this;
            }

            wvalue& operator = (uint16_t value)
            {
                reset();
                t = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (int16_t value)
            {
                reset();
                t = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (uint32_t value)
            {
                reset();
                t = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (int32_t value)
            {
                reset();
                t = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (uint64_t value)
            {
                reset();
                t = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator = (int64_t value)
            {
                reset();
                t = type::Number;
                d = (double)value;
                return *this;
            }

            wvalue& operator=(const char* str)
            {
                reset();
                t = type::String;
                s = str;
                return *this;
            }

            wvalue& operator=(const std::string& str)
            {
                reset();
                t = type::String;
                s = str;
                return *this;
            }

            template <typename T>
            wvalue& operator[](const std::vector<T>& v)
            {
                if (t != type::List)
                    reset();
                t = type::List;
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
                if (t != type::List)
                    reset();
                t = type::List;
                if (!l)
                    l = std::move(std::unique_ptr<std::vector<wvalue>>(new std::vector<wvalue>{}));
                if (l->size() < index+1)
                    l->resize(index+1);
                return (*l)[index];
            }

            wvalue& operator[](const std::string& str)
            {
                if (t != type::Object)
                    reset();
                t = type::Object;
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
                switch(t)
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

        void dump_string(const std::string& str, std::string& out)
        {
            // TODO escaping
            out.push_back('"');
            out += str;
            out.push_back('"');
        }
        void dump_internal(const wvalue& v, std::string& out)
        {
            switch(v.t)
            {
                case type::Null: out += "null"; break;
                case type::False: out += "false"; break;
                case type::True: out += "true"; break;
                case type::Number: out += boost::lexical_cast<std::string>(v.d); break;
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

        std::string dump(const wvalue& v)
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

#undef flask_json_likely
#undef flask_json_unlikely
