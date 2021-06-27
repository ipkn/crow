#pragma once
#include <string>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <memory>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/operators.hpp>
#include <vector>
#include "crow/settings.h"
#if defined(__GNUG__) || defined(__clang__)
#define crow_json_likely(x) __builtin_expect(x, 1)
#define crow_json_unlikely(x) __builtin_expect(x, 0)
#else
#define crow_json_likely(x) x
#define crow_json_unlikely(x) x
#endif
namespace crow {
  namespace mustache { class template_t; }
  namespace Cjson {
	inline void escape(const std::string& str,std::string& ret) {
	  ret.reserve(ret.size()+str.size()+str.size()/4);
	  for (char c:str) {
		switch (c) {
		  case 0x22: ret+="\\\""; break;
		  case 0x5c: ret+="\\\\"; break;
		  case 0xa: ret+="\\n"; break;
		  case 0x8: ret+="\\b"; break;
		  case 0xc: ret+="\\f"; break;
		  case 0xd: ret+="\\r"; break;
		  case 0x9: ret+="\\t"; break;
		  default:
		  if (0<=c&&c<0x20) {
			ret+="\\u00";
			auto to_hex=[](char c) {
			  c=c&0xf;
			  if (c<10)
				return 0x30+c;
			  return 0x61+c-0xa;
			};
			ret+=to_hex(c/0x10);
			ret+=to_hex(c%0x10);
		  } else
			ret+=c;
		  break;
		}
	  }
	}
	inline std::string escape(const std::string& str) {
	  std::string ret;
	  escape(str,ret);
	  return ret;
	}

	enum class value_t : char {
	  null,
	  False,
	  True,
	  number_integer,
	  number_unsigned,
	  number_float,
	  string,
	  array,
	  object,
	};

	inline const char* get_type_str(value_t t) {
	  switch (t) {
		case value_t::number_integer: return "Number";
		case value_t::number_float: return "Number";
		case value_t::number_unsigned: return "Number";
		case value_t::False: return "False";
		case value_t::True: return "True";
		case value_t::array: return "List";
		case value_t::string: return "String";
		case value_t::object: return "Object";
		case value_t::null: return "Null";
		default: return "Unknown";
	  }
	}

	class rvalue;
	rvalue parse(const char* data,size_t size);

	namespace detail {
	  struct r_string
		: boost::less_than_comparable<r_string>,
		boost::less_than_comparable<r_string,std::string>,
		boost::equality_comparable<r_string>,
		boost::equality_comparable<r_string,std::string> {
		r_string() {};
		r_string(char* s,char* e)
		  : s_(s),e_(e) {};
		~r_string() {
		  if (owned_)
			delete[] s_;
		}

		r_string(const r_string& r) {
		  *this=r;
		}

		r_string(r_string&& r) {
		  *this=r;
		}

		r_string& operator = (r_string&& r) {
		  s_=r.s_;
		  e_=r.e_;
		  owned_=r.owned_;
		  if (r.owned_)
			r.owned_=0;
		  return *this;
		}

		r_string& operator = (const r_string& r) {
		  s_=r.s_;
		  e_=r.e_;
		  owned_=0;
		  return *this;
		}
		operator std::string() const {
		  return std::string(s_,e_);
		}
		const char* begin() const { return s_; }
		const char* end() const { return e_; }
		size_t size() const { return end()-begin(); }
		using iterator=const char*;
		using const_iterator=const char*;
		char* s_;
		mutable char* e_;
		uint8_t owned_{0};
		friend std::ostream& operator << (std::ostream& os,const r_string& s) {
		  os<<(std::string)s;
		  return os;
		}
		private:
		void force(char* s,uint32_t length) {
		  s_=s;
		  e_=s_+length;
		  owned_=1;
		}
		friend rvalue crow::Cjson::parse(const char* data,size_t size);
	  };
	  inline bool operator < (const r_string& l,const r_string& r) {
		return boost::lexicographical_compare(l,r);
	  }

	  inline bool operator < (const r_string& l,const std::string& r) {
		return boost::lexicographical_compare(l,r);
	  }

	  inline bool operator > (const r_string& l,const std::string& r) {
		return boost::lexicographical_compare(r,l);
	  }

	  inline bool operator == (const r_string& l,const r_string& r) {
		return boost::equals(l,r);
	  }

	  inline bool operator == (const r_string& l,const std::string& r) {
		return boost::equals(l,r);
	  }
	}

	class rvalue {
	  static const int cached_bit=2;
	  static const int error_bit=4;
	  public:
	  rvalue() noexcept: option_{error_bit} {}
	  rvalue(value_t t) noexcept
		: lsize_{},lremain_{},t_{t}
	  {}
	  rvalue(value_t t,char* s,char* e)  noexcept
		: start_{s},
		end_{e},
		t_{t}
	  {
		determine_num_type();
	  }

	  rvalue(const rvalue& r)
		: start_(r.start_),
		end_(r.end_),
		key_(r.key_),
		t_(r.t_),
		option_(r.option_) {
		copy_l(r);
	  }

	  rvalue(rvalue&& r) noexcept {
		*this=std::move(r);
	  }

	  rvalue& operator = (const rvalue& r) {
		start_=r.start_;
		end_=r.end_;
		key_=r.key_;
		t_=r.t_;
		option_=r.option_;
		copy_l(r);
		return *this;
	  }
	  rvalue& operator = (rvalue&& r) noexcept {
		start_=r.start_;
		end_=r.end_;
		key_=std::move(r.key_);
		l_=std::move(r.l_);
		lsize_=r.lsize_;
		lremain_=r.lremain_;
		t_=r.t_;
		option_=r.option_;
		return *this;
	  }

	  explicit operator bool() const noexcept {
		return (option_&error_bit)==0;
	  }

	  explicit operator int64_t() const {
		return i();
	  }

	  explicit operator uint64_t() const {
		return u();
	  }

	  explicit operator int() const {
		return (int)i();
	  }
	  //
	  inline value_t t() const {
		return t_;
	  }
	  //
	  inline int64_t i() const {
		return boost::lexical_cast<int64_t>(start_,end_-start_);
	  }
	  //
	  inline uint64_t u() const {
		return boost::lexical_cast<uint64_t>(start_,end_-start_);
	  }
	  //
	  inline double d() const {
		return boost::lexical_cast<double>(start_,end_-start_);
	  }
	  //
	  inline bool b() const {
		return t()==value_t::True;
	  }
	  //
	  inline void unescape() const {
		if (*(start_-1)) {
		  char* head=start_;
		  char* tail=start_;
		  while (head!=end_) {
			if (*head=='\\') {
			  switch (*++head) {
				case '"':  *tail++='"'; break;
				case '\\': *tail++='\\'; break;
				case '/':  *tail++='/'; break;
				case 'b':  *tail++='\b'; break;
				case 'f':  *tail++='\f'; break;
				case 'n':  *tail++='\n'; break;
				case 'r':  *tail++='\r'; break;
				case 't':  *tail++='\t'; break;
				case 'u':
				{
				  auto from_hex=[](char c) {
					if (c>='a')
					  return c-'a'+10;
					if (c>='A')
					  return c-'A'+10;
					return c-'0';
				  };
				  unsigned int code=
					(from_hex(head[1])<<12)+
					(from_hex(head[2])<<8)+
					(from_hex(head[3])<<4)+
					from_hex(head[4]);
				  if (code>=0x800) {
					*tail++=0xE0|(code>>12);
					*tail++=0x80|((code>>6)&0x3F);
					*tail++=0x80|(code&0x3F);
				  } else if (code>=0x80) {
					*tail++=0xC0|(code>>6);
					*tail++=0x80|(code&0x3F);
				  } else {
					*tail++=code;
				  }
				  head+=4;
				}
				break;
			  }
			} else
			  *tail++=*head;
			head++;
		  }
		  end_=tail;
		  *end_=0;
		  *(start_-1)=0;
		}
	  }

	  detail::r_string s() const {
		unescape();
		return detail::r_string{start_, end_};
	  }

	  bool has(const char* str) const {
		return has(std::string(str));
	  }

	  bool has(const std::string& str) const {
		struct Pred {
		  bool operator()(const rvalue& l,const rvalue& r) const {
			return l.key_<r.key_;
		  };
		  bool operator()(const rvalue& l,const std::string& r) const {
			return l.key_<r;
		  };
		  bool operator()(const std::string& l,const rvalue& r) const {
			return l<r.key_;
		  };
		};
		if (!is_cached()) {
		  std::sort(begin(),end(),Pred());
		  set_cached();
		}
		auto it=lower_bound(begin(),end(),str,Pred());
		return it!=end()&&it->key_==str;
	  }

	  int count(const std::string& str) {
		return has(str)?1:0;
	  }

	  rvalue* begin() const {
		return l_.get();
	  }
	  rvalue* end() const {
		return l_.get()+lsize_;
	  }

	  const detail::r_string& key() const {
		return key_;
	  }

	  size_t size() const {
		if (t()==value_t::string)
		  return s().size();
		return lsize_;
	  }

	  const rvalue& operator[](int index) const {
		return l_[index];
	  }

	  const rvalue& operator[](size_t index) const {
		return l_[index];
	  }

	  const rvalue& operator[](const char* str) const {
		return this->operator[](std::string(str));
	  }

	  const rvalue& operator[](const std::string& str) const {
		struct Pred {
		  bool operator()(const rvalue& l,const rvalue& r) const {
			return l.key_<r.key_;
		  };
		  bool operator()(const rvalue& l,const std::string& r) const {
			return l.key_<r;
		  };
		  bool operator()(const std::string& l,const rvalue& r) const {
			return l<r.key_;
		  };
		};
		if (!is_cached()) {
		  std::sort(begin(),end(),Pred());
		  set_cached();
		}
		auto it=lower_bound(begin(),end(),str,Pred());
		if (it!=end()&&it->key_==str)
		  return *it;
		//throw std::runtime_error("cannot find key");
		static rvalue nullValue;
		return nullValue;
	  }

	  void set_error() {
		option_|=error_bit;
	  }

	  bool error() const {
		return (option_&error_bit)!=0;
	  }
	  private:
	  bool is_cached() const {
		return (option_&cached_bit)!=0;
	  }
	  void set_cached() const {
		option_|=cached_bit;
	  }
	  void copy_l(const rvalue& r) {
		if (r.t()!=value_t::object&&r.t()!=value_t::array)
		  return;
		lsize_=r.lsize_;
		lremain_=0;
		l_.reset(new rvalue[lsize_]);
		std::copy(r.begin(),r.end(),begin());
	  }

	  void emplace_back(rvalue&& v) {
		if (!lremain_) {
		  int new_size=lsize_+lsize_;
		  if (new_size-lsize_>60000)
			new_size=lsize_+60000;
		  if (new_size<4)
			new_size=4;
		  rvalue* p=new rvalue[new_size];
		  rvalue* p2=p;
		  for (auto& x:*this)
			*p2++=std::move(x);
		  l_.reset(p);
		  lremain_=new_size-lsize_;
		}
		l_[lsize_++]=std::move(v);
		lremain_--;
	  }

	  // determines num_type from the string
	  void determine_num_type() {
		if (t_!=value_t::number_integer) {
		  t_=value_t::null;
		  return;
		}

		const std::size_t len=end_-start_;
		const bool has_minus=std::memchr(start_,'-',len)!=nullptr;
		const bool has_e=std::memchr(start_,'e',len)!=nullptr
		  ||std::memchr(start_,'E',len)!=nullptr;
		const bool has_dec_sep=std::memchr(start_,'.',len)!=nullptr;
		if (has_dec_sep||has_e)
		  t_=value_t::number_float;
		else if (has_minus)
		  t_=value_t::number_integer;
		else
		  t_=value_t::number_unsigned;
	  }

	  mutable char* start_;
	  mutable char* end_;
	  detail::r_string key_;
	  std::unique_ptr<rvalue[]> l_;
	  uint32_t lsize_;
	  uint16_t lremain_;
	  value_t t_;
	  mutable uint8_t option_{0};

	  friend rvalue load_nocopy_internal(char* data,size_t size);
	  friend rvalue parse(const char* data,size_t size);
	  friend std::ostream& operator <<(std::ostream& os,const rvalue& r) {
		switch (r.t_) {
		  case value_t::null: os<<"null"; break;
		  case value_t::False: os<<"false"; break;
		  case value_t::True: os<<"true"; break;
		  case value_t::number_float:os<<r.d(); break;
		  case value_t::number_unsigned:os<<r.u(); break;
		  case value_t::number_integer:os<<r.i(); break;
		  case value_t::string: os<<'"'<<r.s()<<'"'; break;
		  case value_t::array: {
			os<<'['<<r[0];
			for (int i=1;r[i];++i) {
			  os<<','<<r[i];
			}
			os<<']';
		  }
		  break;
		  case value_t::object: {
			os<<'{'<<'"'<<escape(r[0].key_)<<"\":"<<r[0];
			for (int x=1;r[x];++x) {
			  os<<','<<'"'<<escape(r[x].key_)<<"\":"<<r[x];
			}
			os<<'}';
		  }
		  break;
		}
		return os;
	  }
	};
	inline bool operator == (const rvalue& l,const std::string& r) {
	  return l.s()==r;
	}

	inline bool operator == (const std::string& l,const rvalue& r) {
	  return l==r.s();
	}

	inline bool operator != (const rvalue& l,const std::string& r) {
	  return l.s()!=r;
	}

	inline bool operator != (const std::string& l,const rvalue& r) {
	  return l!=r.s();
	}

	inline bool operator == (const rvalue& l,double r) {
	  return l.d()==r;
	}

	inline bool operator == (double l,const rvalue& r) {
	  return l==r.d();
	}

	inline bool operator != (const rvalue& l,double r) {
	  return l.d()!=r;
	}

	inline bool operator != (double l,const rvalue& r) {
	  return l!=r.d();
	}

	inline rvalue load_nocopy_internal(char* data,size_t size) {
	  //static const char* escaped = "\"\\/\b\f\n\r\t";
	  struct Parser {
		Parser(char* data,size_t /*size*/)
		  : data(data) {}
		bool consume(char c) {
		  if (crow_json_unlikely(*data!=c))
			return false;
		  data++;
		  return true;
		}

		void ws_skip() {
		  while (*data==' '||*data=='\t'||*data=='\r'||*data=='\n') ++data;
		};

		inline rvalue decode_string() {
		  if (crow_json_unlikely(!consume('"')))
			return {};
		  char* start=data;
		  uint8_t has_escaping=0;
		  while (1) {
			if (crow_json_likely(*data!='"'&&*data!='\\'&&*data!='\0')) {
			  data++;
			} else if (*data=='"') {
			  *data=0;
			  *(start-1)=has_escaping;
			  data++;
			  return {value_t::string, start, data-1};
			} else if (*data=='\\') {
			  has_escaping=1;
			  data++;
			  switch (*data) {
				case 'u':
				{
				  auto check=[](char c) {
					return
					  ('0'<=c&&c<='9')||
					  ('a'<=c&&c<='f')||
					  ('A'<=c&&c<='F');
				  };
				  if (!(check(*(data+1))&&
						check(*(data+2))&&
						check(*(data+3))&&
						check(*(data+4))))
					return {};
				}
				data+=5;
				break;
				case '"':
				case '\\':
				case '/':
				case 'b':
				case 'f':
				case 'n':
				case 'r':
				case 't':
				data++;
				break;
				default:
				return {};
			  }
			} else
			  return {};
		  }
		  return {};
		}

		inline rvalue decode_list() {
		  rvalue ret(value_t::array);
		  if (crow_json_unlikely(!consume('['))) {
			ret.set_error();
			return ret;
		  }
		  ws_skip();
		  if (crow_json_unlikely(*data==']')) {
			data++;
			return ret;
		  }

		  while (1) {
			auto v=decode_value();
			if (crow_json_unlikely(!v)) {
			  ret.set_error();
			  break;
			}
			ws_skip();
			ret.emplace_back(std::move(v));
			if (*data==']') {
			  data++;
			  break;
			}
			if (crow_json_unlikely(!consume(','))) {
			  ret.set_error();
			  break;
			}
			ws_skip();
		  }
		  return ret;
		}

		rvalue decode_number() {
		  char* start=data;

		  enum NumberParsingState {
			Minus,
			AfterMinus,
			ZeroFirst,
			Digits,
			DigitsAfterPoints,
			E,
			DigitsAfterE,
			Invalid,
		  } state{Minus};
		  while (crow_json_likely(state!=Invalid)) {
			switch (*data) {
			  case '0':
			  state=(NumberParsingState)"\2\2\7\3\4\6\6"[state];
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
			  state=(NumberParsingState)"\3\3\7\3\4\6\6"[state];
			  while (*(data+1)>='0'&&*(data+1)<='9') data++;
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
			  state=(NumberParsingState)"\7\7\4\4\7\7\7"[state];
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
			  state=(NumberParsingState)"\1\7\7\7\7\6\7"[state];
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
			  state=(NumberParsingState)"\7\7\7\7\7\6\7"[state];
			  /*if (state == NumberParsingState::E)
			  {
				  state = NumberParsingState::DigitsAfterE;
			  }
			  else
				  return {};*/
			  break;
			  case 'e': case 'E':
			  state=(NumberParsingState)"\7\7\7\5\5\7\7"[state];
			  /*if (state == NumberParsingState::Digits ||
				  state == NumberParsingState::DigitsAfterPoints)
			  {
				  state = NumberParsingState::E;
			  }
			  else
				  return {};*/
			  break;
			  default:
			  if (crow_json_likely(state==NumberParsingState::ZeroFirst||
								   state==NumberParsingState::Digits||
								   state==NumberParsingState::DigitsAfterPoints||
								   state==NumberParsingState::DigitsAfterE))
				return {value_t::number_integer, start, data};
			  else
				return {};
			}
			data++;
		  }

		  return {};
		}

		inline rvalue decode_value() {
		  switch (*data) {
			case '[':
			return decode_list();
			case '{':
			return decode_object();
			case '"':
			return decode_string();
			case 't':
			if (//e-data >= 4 &&
				data[1]=='r'&&
				data[2]=='u'&&
				data[3]=='e') {
			  data+=4;
			  return {value_t::True};
			} else
			  return {};
			case 'f':
			if (//e-data >= 5 &&
				data[1]=='a'&&
				data[2]=='l'&&
				data[3]=='s'&&
				data[4]=='e') {
			  data+=5;
			  return {value_t::False};
			} else
			  return {};
			case 'n':
			if (//e-data >= 4 &&
				data[1]=='u'&&
				data[2]=='l'&&
				data[3]=='l') {
			  data+=4;
			  return {value_t::null};
			} else
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

		inline rvalue decode_object() {
		  rvalue ret(value_t::object);
		  if (crow_json_unlikely(!consume('{'))) {
			ret.set_error();
			return ret;
		  }

		  ws_skip();

		  if (crow_json_unlikely(*data=='}')) {
			data++;
			return ret;
		  }

		  while (1) {
			auto t=decode_string();
			if (crow_json_unlikely(!t)) {
			  ret.set_error();
			  break;
			}

			ws_skip();
			if (crow_json_unlikely(!consume(':'))) {
			  ret.set_error();
			  break;
			}

			// TODO caching key to speed up (flyweight?)
			auto key=t.s();

			ws_skip();
			auto v=decode_value();
			if (crow_json_unlikely(!v)) {
			  ret.set_error();
			  break;
			}
			ws_skip();

			v.key_=std::move(key);
			ret.emplace_back(std::move(v));
			if (crow_json_unlikely(*data=='}')) {
			  data++;
			  break;
			}
			if (crow_json_unlikely(!consume(','))) {
			  ret.set_error();
			  break;
			}
			ws_skip();
		  }
		  return ret;
		}

		rvalue parse() {
		  ws_skip();
		  auto ret=decode_value(); // or decode object?
		  ws_skip();
		  if (ret&&*data!='\0')
			ret.set_error();
		  return ret;
		}

		char* data;
	  };
	  return Parser(data,size).parse();
	}
	inline rvalue parse(const char* data,size_t size) {
	  char* s=new char[size+1];
	  memcpy(s,data,size);
	  s[size]=0;
	  auto ret=load_nocopy_internal(s,size);
	  if (ret)
		ret.key_.force(s,size);
	  else
		delete[] s;
	  return ret;
	}

	inline rvalue parse(const char* data) {
	  return parse(data,strlen(data));
	}

	inline rvalue parse(const std::string& str) {
	  return parse(str.data(),str.size());
	}

	class value {
	  friend class crow::mustache::template_t;
	  public:
	  value_t t() const { return t_; }
	  private:
	  value_t t_{value_t::null};
	  union {
		double d;
		int64_t si;
		uint64_t ui{};
	  } num;
	  std::string s;
	  std::unique_ptr<std::vector<value>> l;
	  std::unordered_map<std::string,value> o;
	  public:
	  value() {}
	  value(const rvalue& r) {
		t_=r.t();
		switch (r.t()) {
		  case value_t::null:
		  case value_t::False:
		  case value_t::True:
		  return;
		  case value_t::number_integer:num.si=r.i();
		  case value_t::number_unsigned:num.si=r.u();
		  case value_t::number_float:num.si=r.d();
		  return;
		  case value_t::string:
		  s=r.s();
		  return;
		  case value_t::array:
		  l=std::unique_ptr<std::vector<value>>(new std::vector<value>{});
		  l->reserve(r.size());
		  for (auto it=r.begin(); it!=r.end(); ++it)
			l->emplace_back(*it);
		  return;
		  case value_t::object:
		  o.clear();
		  for (auto it=r.begin(); it!=r.end(); ++it)
			o.emplace(it->key(),*it);
		  return;
		}
	  }

	  value(value&& r) {
		*this=std::move(r);
	  }

	  value& operator = (value&& r) {
		t_=r.t_;
		num=r.num;
		s=std::move(r.s);
		l=std::move(r.l);
		o=std::move(r.o);
		return *this;
	  }

	  void clear() {
		reset();
	  }

	  void reset() {
		t_=value_t::null;
		l.reset();
		o.clear();
	  }

	  value& operator = (std::nullptr_t) {
		reset();
		return *this;
	  }
	  value& operator = (bool value) {
		reset();
		if (value)
		  t_=value_t::True;
		else
		  t_=value_t::False;
		return *this;
	  }

	  value& operator = (double value) {
		reset();
		t_=value_t::number_float;
		num.d=value;
		return *this;
	  }

	  value& operator = (unsigned short value) {
		reset();
		t_=value_t::number_unsigned;
		num.ui=value;
		return *this;
	  }

	  value& operator = (short value) {
		reset();
		t_=value_t::number_integer;
		num.si=value;
		return *this;
	  }

	  value& operator = (long long value) {
		reset();
		t_=value_t::number_integer;
		num.si=value;
		return *this;
	  }

	  value& operator = (long value) {
		reset();
		t_=value_t::number_integer;
		num.si=value;
		return *this;
	  }

	  value& operator = (int value) {
		reset();
		t_=value_t::number_integer;
		num.si=value;
		return *this;
	  }

	  value& operator = (unsigned long long value) {
		reset();
		t_=value_t::number_unsigned;
		num.ui=value;
		return *this;
	  }

	  value& operator = (unsigned long value) {
		reset();
		t_=value_t::number_unsigned;
		num.ui=value;
		return *this;
	  }

	  value& operator = (unsigned int value) {
		reset();
		t_=value_t::number_unsigned;
		num.ui=value;
		return *this;
	  }

	  value& operator=(const char* str) {
		reset();
		t_=value_t::string;
		s=str;
		return *this;
	  }

	  value& operator=(const std::string& str) {
		reset();
		t_=value_t::string;
		s=str;
		return *this;
	  }

	  value& operator=(std::vector<value>&& v) {
		if (t_!=value_t::array)
		  reset();
		t_=value_t::array;
		if (!l)
		  l=std::unique_ptr<std::vector<value>>(new std::vector<value>{});
		l->clear();
		l->resize(v.size());
		size_t idx=0;
		for (auto& x:v) {
		  (*l)[idx++]=std::move(x);
		}
		return *this;
	  }

	  template <typename T>
	  value& operator=(const std::vector<T>& v) {
		if (t_!=value_t::array)
		  reset();
		t_=value_t::array;
		if (!l)
		  l=std::unique_ptr<std::vector<value>>(new std::vector<value>{});
		l->clear();
		l->resize(v.size());
		size_t idx=0;
		for (auto& x:v) {
		  (*l)[idx++]=x;
		}
		return *this;
	  }

	  value& operator[](unsigned index) {
		if (t_!=value_t::array)
		  reset();
		t_=value_t::array;
		if (!l)
		  l=std::unique_ptr<std::vector<value>>(new std::vector<value>{});
		if (l->size()<index+1)
		  l->resize(index+1);
		return (*l)[index];
	  }

	  int count(const std::string& str) {
		if (t_!=value_t::object)
		  return 0;
		if (o.empty())
		  return 0;
		return o.count(str);
	  }

	  value& operator[](const std::string& str) {
		if (t_!=value_t::object)
		  reset();
		t_=value_t::object;
		return o[str];
	  }
	  std::vector<std::string> keys() const {
		if (t_!=value_t::object)
		  return {};
		std::vector<std::string> result;
		for (auto& kv:o) {
		  result.push_back(kv.first);
		}
		return result;
	  }

	  size_t estimate_length() const {
		switch (t_) {
		  case value_t::null: return 4;
		  case value_t::False: return 5;
		  case value_t::True: return 4;
		  case value_t::number_integer: return 30;
		  case value_t::string: return 2+s.size()+s.size()/2;
		  case value_t::array:
		  {
			size_t sum{};
			if (l) {
			  for (auto& x:*l) {
				sum+=1;
				sum+=x.estimate_length();
			  }
			}
			return sum+2;
		  }
		  case value_t::object:
		  {
			size_t sum{};
			if (!o.empty()) {
			  for (auto& kv:o) {
				sum+=2;
				sum+=2+kv.first.size()+kv.first.size()/2;
				sum+=kv.second.estimate_length();
			  }
			}
			return sum+2;
		  }
		}
		return 1;
	  }
	  //asciphx
	  std::string dump() const {
		if (t_!=value_t::object)return {};
		std::string ret;
		ret.reserve(this->estimate_length());
		dump_internal(*this,ret);
		return ret;
	  }
	  friend void dump_internal(const value& v,std::string& out);
	};

	inline void dump_string(const std::string& str,std::string& out) {
	  out.push_back('"');
	  escape(str,out);
	  out.push_back('"');
	}
	inline void dump_internal(const value& v,std::string& out) {
	  switch (v.t_) {
		case value_t::null: out+="null"; break;
		case value_t::False: out+="false"; break;
		case value_t::True: out+="true"; break;
		case value_t::number_float:
#ifdef _MSC_VER
#define MSC_COMPATIBLE_SPRINTF(BUFFER_PTR, FORMAT_PTR, VALUE) sprintf_s((BUFFER_PTR), 128, (FORMAT_PTR), (VALUE))
#else
#define MSC_COMPATIBLE_SPRINTF(BUFFER_PTR, FORMAT_PTR, VALUE) sprintf((BUFFER_PTR), (FORMAT_PTR), (VALUE))
#endif
		char outbuf[128];
		MSC_COMPATIBLE_SPRINTF(outbuf,"%g",v.num.d);
		out+=outbuf;
#undef MSC_COMPATIBLE_SPRINTF
		break;
		case value_t::number_integer:out+=std::to_string(v.num.si);break;
		case value_t::number_unsigned:out+=std::to_string(v.num.ui);break;
		case value_t::string: dump_string(v.s,out); break;
		case value_t::array: {
		  out.push_back('[');
		  bool first=true;
		  for (auto& x:*v.l) {
			if (!first) out.push_back(',');
			first=false;
			dump_internal(x,out);
		  }
		  out.push_back(']');
		}
		break;
		case value_t::object:{
		  out.push_back('{');
		  bool first=true;
		  for (auto& kv:v.o) {
			if (!first) out.push_back(',');
			first=false;
			dump_string(kv.first,out);
			out.push_back(':');
			dump_internal(kv.second,out);
		  }
		  out.push_back('}');
		}
		break;
	  }
	}
  }
}
#undef crow_json_likely
#undef crow_json_unlikely
