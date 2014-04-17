#pragma once
#include <iostream>

#include <string>
#include <unordered_map>

namespace flask
{
    namespace json
    {
        enum class type : intptr_t
        {
            Null,
            False,
            True,
            Number,
            String,
            List,
            Object,
        };

        const char* json_type_str =
            "\0\0\0\0\0\0\0\0"
            "\1\0\0\0\0\0\0\0"
            "\2\0\0\0\0\0\0\0"
            "\3\0\0\0\0\0\0\0"
            "\4\0\0\0\0\0\0\0"
            "\5\0\0\0\0\0\0\0"
            "\6\0\0\0\0\0\0\0";

        struct r_string
        {
            operator std::string ()
            {
                return std::string(s_, e_);
            }

            char* s_;
            char* e_;
        };

        struct rvalue
        {
            rvalue() {}
            rvalue(type t) 
                : start_{json_type_str + 8*(int)t}, 
                end_{json_type_str+8*(int)t+8}
            {}
            ~rvalue()
            {
                deallocate();
            }
            void deallocate()
            {
                if (mem_)
                {
                    delete[] mem_;
                    mem_ = nullptr;
                }
            }
            type t() const
            {
                return *reinterpret_cast<const type*>(start_);
            }
            const void* data() const
            {
                return reinterpret_cast<const void*>(start_ + sizeof(intptr_t));
            }

            int i() const
            {
                if (t() != type::Number)
                    throw std::runtime_error("value is not number");
                return static_cast<int>(*(double*)data());
            }

            double d() const
            {
                if (t() != type::Number)
                    throw std::runtime_error("value is not number");
                return *(double*)data();
            }

            r_string s() const
            {
                if (t() != type::Number)
                    throw std::runtime_error("value is not number");
                return {(char*)data(),(char*)data() + sizeof(char*)};
            }

            const char* start_{};
            const char* end_{};
            char* mem_{};
        };

        //inline rvalue decode(const std::string& s)
        //{
        //}

        /*inline boost::optional<const rvalue> decode(const char* data, size_t size, bool partial = false)
        {
            static char* escaped = "\"\\\/\b\f\n\r\t"
            const char* e = data + size;
            auto ws_skip = [&]
                {
                    while(data != e)
                    {
                        while(data != e && *data == ' ') ++data;
                        while(data != e && *data == '\t') ++data;
                        while(data != e && *data == '\r') ++data;
                        while(data != e && *data == '\n') ++data;
                    }
                };
            auto decode_string = [&]->boost::optional<rvalue>
                {
                    ws_skip();
                    if (data == e || *data != '"')
                        return {};
                    while(data != e)
                    {
                        if (*data == '"')
                        {
                        }
                        else if (*data == '\\')
                        {
                            // TODO
                            //if (data+1 == e)
                                //return {};
                            //data += 2;
                        }
                        else
                        {
                            data ++;
                        }
                    }
                };
            auto decode_list = [&]->boost::optional<rvalue>
                {
                    // TODO
                };
            auto decode_number = [&]->boost::optional<rvalue>
                {
                    // TODO
                };
            auto decode_value = [&]->boost::optional<rvalue>
                {
                    if (data == e)
                        return {};
                    switch(*data)
                    {
                        case '"':
                            emit_str_begin();
                        case '{':
                        case 't':
                            if (e-data >= 4 &&
                                    data[1] == 'r' &&
                                    data[2] == 'u' &&
                                    data[3] == 'e')
                                return rvalue(type::True);
                            else
                                return {};
                        case 'f':
                        case 'n':
                    }
                    // TODO
                    return {};
                };
            auto decode_object =[&]->boost::optional<rvalue>
                {
                    ws_skip();
                    if (data == e || *data != '{')
                        return {};

                    ws_skip();
                    if (data == e)
                        return {};
                    if (*data == '}')
                        return rvalue(type::Object);
                    while(1)
                    {
                        auto t = decode_string();
                        ws_skip();
                        if (data == e)
                            return {};
                        if (data != ':')
                            return {};
                        auto v = decode_value();
                        ws_skip();
                        if (data == e)
                            return {};
                        if (data == '}')
                            break;
                        if (data != ',')
                            return {};
                    }
                    // TODO build up object
                    throw std::runtime_error("Not implemented");
                    return rvalue(type::Object);
                };

            return decode_object();


                        }
                }
            }
        }*/

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

            wvalue& operator[](unsigned index)
            {
                if (t != type::List)
                    reset();
                t = type::List;
                if (!l)
                    l = std::move(std::unique_ptr<std::vector<wvalue>>(new std::vector<wvalue>{}));
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


            friend void encode_internal(const wvalue& v, std::string& out);
            friend std::string encode(const wvalue& v);
        };

        void encode_string(const std::string& str, std::string& out)
        {
            // TODO escaping
            out.push_back('"');
            out += str;
            out.push_back('"');
        }
        void encode_internal(const wvalue& v, std::string& out)
        {
            switch(v.t)
            {
                case type::Null: out += "null"; break;
                case type::False: out += "false"; break;
                case type::True: out += "true"; break;
                case type::Number: out += boost::lexical_cast<std::string>(v.d); break;
                case type::String: encode_string(v.s, out); break;
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
                                 encode_internal(x, out);
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
                                 encode_string(kv.first, out);
                                 out.push_back(':');
                                 encode_internal(kv.second, out);
                             }
                         }
                         out.push_back('}');
                     }
                     break;
            }
        }

        std::string encode(const wvalue& v)
        {
            std::string ret;
            ret.reserve(v.estimate_length());
            encode_internal(v, ret);
            return ret;
        }

        //std::vector<boost::asio::const_buffer> encode_ref(wvalue& v)
        //{
        //}
    }
}
