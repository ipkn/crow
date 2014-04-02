#pragma once

namespace flask
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

		// from http://akrzemi1.wordpress.com/2011/05/11/parsing-strings-at-compile-time-part-i/
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

			constexpr unsigned size() const { 
				return size_; 
			}
		};


		constexpr unsigned find_closing_tag(const_str s, unsigned p)
		{
			return s[p] == '>' ? p : find_closing_tag(s, p+1);
		}

		constexpr int count(const_str s, int i=0)
		{
			return i == s.size() ? 0 : s[i] == '<' ? 1+count(s,i+1) : count(s,i+1);
		}
	}
}
