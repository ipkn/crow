#pragma once
#include "crow/settings.h"
#include <boost/asio/streambuf.hpp>
#include <zlib.h>
#include <string>

namespace crow {
	class zlib_compressor {
		public:
		zlib_compressor(bool reset_before_compress, bool noheader, int window_bits, int level, int mem_level, int strategy)
			: reset_before_compress(reset_before_compress)
			, window_bits(window_bits) {
			stream = std::make_unique<z_stream>();
			stream->zalloc = 0;
			stream->zfree = 0;
			stream->opaque = 0;

			::deflateInit2(stream.get(),
						   level,
						   Z_DEFLATED,
						   (noheader ? -window_bits : window_bits),
						   mem_level,
						   strategy
			);
		}

		~zlib_compressor() {
			::deflateEnd(stream.get());
		}

		std::string compress(const std::string& src) {
			if(reset_before_compress)
				::deflateReset(stream.get());

			stream->next_in = reinterpret_cast<uint8_t*>(const_cast<char*>(src.c_str()));
			stream->avail_in = src.size();

			const uint64_t bufferSize = 256;
			boost::asio::streambuf buffer;
			do {
				boost::asio::streambuf::mutable_buffers_type chunk = buffer.prepare(bufferSize);

				uint8_t* next_out = boost::asio::buffer_cast<uint8_t*>(chunk);

				stream->next_out = next_out;
				stream->avail_out = bufferSize;

				::deflate(stream.get(), reset_before_compress ? Z_FINISH : Z_SYNC_FLUSH);

				uint64_t outputSize = stream->next_out - next_out;
				buffer.commit(outputSize);
			} while(stream->avail_out == 0);

			uint64_t buffer_size = buffer.size();
			if(!reset_before_compress) buffer_size -= 4;

			return std::string(boost::asio::buffer_cast<const char*>(buffer.data()), buffer_size);
		}

		std::unique_ptr<z_stream> stream;

		bool reset_before_compress;
		int window_bits;
	};

	class zlib_decompressor {
		public:
		zlib_decompressor(bool reset_before_decompress, bool noheader, int window_bits)
			: reset_before_decompress(reset_before_decompress)
			, window_bits(window_bits) {
			stream = std::make_unique<z_stream>();
			stream->zalloc = 0;
			stream->zfree = 0;
			stream->opaque = 0;

			::inflateInit2(stream.get(), (noheader ? -window_bits : window_bits));
		}

		~zlib_decompressor() {
			inflateEnd(stream.get());
		}

		std::string decompress(std::string src) {
			if(reset_before_decompress)
				inflateReset(stream.get());

			src.push_back('\x00');
			src.push_back('\x00');
			src.push_back('\xff');
			src.push_back('\xff');

			stream->next_in = reinterpret_cast<uint8_t*>(const_cast<char*>(src.c_str()));
			stream->avail_in = src.size();

			const uint64_t bufferSize = 256;
			boost::asio::streambuf buffer;
			do {
				boost::asio::streambuf::mutable_buffers_type chunk = buffer.prepare(bufferSize);

				uint8_t* next_out = boost::asio::buffer_cast<uint8_t*>(chunk);

				stream->next_out = next_out;
				stream->avail_out = bufferSize;

				::inflate(stream.get(), reset_before_decompress ? Z_FINISH : Z_SYNC_FLUSH);
				buffer.commit(stream->next_out - next_out);
			} while(stream->avail_out == 0);

			return std::string(boost::asio::buffer_cast<const char*>(buffer.data()), buffer.size());
		}

		std::unique_ptr<z_stream> stream;

		bool reset_before_decompress;
		int window_bits;
	};
}
