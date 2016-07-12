#pragma once

#ifdef CROW_ENABLE_COMPRESSION

#include "http_request.h"
#include "http_response.h"

#include <zlib.h>

// http://zlib.net/manual.html

namespace crow
{
    namespace compression
    {
        // Values used in the 'windowBits' parameter for deflateInit2.
        enum algorithm
        {
            // 15 is the default value for deflate
            DEFLATE = 15,
            // windowBits can also be greater than 15 for optional gzip encoding. 
            // Add 16 to windowBits to write a simple gzip header and trailer around the compressed data instead of a zlib wrapper.
            GZIP = 15|16,
        };

        std::string compress_string(std::string const & str, algorithm algo)
        {
            std::string compressed_str;
            z_stream stream{};
            // Initialize with the default values
            if (::deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, algo, 8, Z_DEFAULT_STRATEGY) == Z_OK)
            {      
                char buffer[8192];

                stream.avail_in = str.size();
                // zlib does not take a const pointer. The data is not altered.
                stream.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(str.c_str()));

                int code = Z_OK;
                do
                {
                    stream.avail_out = sizeof(buffer);
                    stream.next_out = reinterpret_cast<Bytef *>(&buffer[0]);

                    code = ::deflate(&stream, Z_FINISH);
                    // Successful and non-fatal error code returned by deflate when used with Z_FINISH flush
                    if (code == Z_OK || code == Z_STREAM_END)
                    {
                        std::copy(&buffer[0], &buffer[sizeof(buffer) - stream.avail_out], std::back_inserter(compressed_str));
                    }

                } while (code == Z_OK);

                if (code != Z_STREAM_END)
                    compressed_str.clear();

                ::deflateEnd(&stream);
            }

            return compressed_str;
        }
    }

    // Middleware for deflate algorithm
    struct CompressionDeflate
    {
        struct context
        {
            // Compress by default
            bool compress = true;
        };

        void before_handle(crow::request & request, crow::response & response, context & ctx)
        {
        }

        void after_handle(crow::request & request, crow::response & response, context & ctx)
        {
            // Only compress if told to
            if (ctx.compress == false)
                return;

            std::string compressed_body = compression::compress_string(response.body, compression::algorithm::DEFLATE);

            // If compression went fine, replace the body; otherwise, leave it as is
            if (compressed_body.empty() == false)
            {
                response.body = std::move(compressed_body);
                // Attach the appropriate header so the client knows how to handle the data
                response.add_header("Content-Encoding", "deflate");
            }
            else
            {
                // Warn and do nothing
                CROW_LOG_WARNING << "Unable to compress request for " << request.url;
            }
        }
    };

    // Middleware for gzip algorithm
    struct CompressionGzip
    {
        struct context
        {
            // Compress by default
            bool compress = true;
        };

        void before_handle(crow::request & request, crow::response & response, context & ctx)
        {
        }

        void after_handle(crow::request & request, crow::response & response, context & ctx)
        {
            // Only compress if told to
            if (ctx.compress == false)
                return;

            std::string compressed_body = compression::compress_string(response.body, compression::algorithm::GZIP);

            // If compression went fine, replace the body; otherwise, leave it as is
            if (compressed_body.empty() == false)
            {
                response.body = std::move(compressed_body);
                // Attach the appropriate header so the client knows how to handle the data
                response.add_header("Content-Encoding", "gzip");
            }
            else
            {
                // Warn and do nothing
                CROW_LOG_WARNING << "Unable to compress request for " << request.url;
            }
        }
    };
}

#endif
