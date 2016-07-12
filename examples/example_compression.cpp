#include "crow.h"
#include "middleware_compression.h"

int main()
{
    crow::App<crow::CompressionDeflate> app;
    //crow::App<crow::CompressionGzip> app;

    CROW_ROUTE(app, "/hello")
    ([&](const crow::request& req){
        app.get_context<crow::CompressionDeflate>(req).compress = false;
        //app.get_context<crow::CompressionGzip>(req).compress = false;

        return "Hello World! This is uncompressed!";
    });

    CROW_ROUTE(app, "/hello_compressed")
    ([](){
        return "Hello World! This is compressed by default!";
    });

    // ignore all log
    crow::logger::setLogLevel(crow::LogLevel::DEBUG);
    //crow::logger::setHandler(std::make_shared<ExampleLogHandler>());

    app.port(18080)
        .multithreaded()
        .run();
}
