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
		class StrWrap
		{
			const char * const begin_;
			unsigned size_;

			public:
			template< unsigned N >
				constexpr StrWrap( const char(&arr)[N] ) : begin_(arr), size_(N - 1) {
					static_assert( N >= 1, "not a string literal");
				}

			constexpr char operator[]( unsigned i ) { 
				return requires_in_range(i, size_), begin_[i]; 
			}

			constexpr operator const char *() { 
				return begin_; 
			}

			constexpr unsigned size() { 
				return size_; 
			}
		};


		constexpr int find_closing_tag(StrWrap s, std::size_t p)
		{
			return s[p] == '>' ? p : find_closing_tag(s, p+1);
		}

		constexpr int count(StrWrap s, int i=0)
		{
			return i == s.size() ? 0 : s[i] == '<' ? 1+count(s,i+1) : count(s,i+1);
		}
	}
}
