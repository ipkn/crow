#define CROW_ENABLE_SSL
#include "crow.h"

int main()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")
    ([]() {
        return "Hello world!";
    });

    app.port(18080).ssl_file("test.crt", "test.key").run();

    // Use .pem file
    //app.port(18080).ssl_file("test.pem").run();
    
    // Use custom context; see boost::asio::ssl::context
    /*
     * crow::ssl_context_t ctx;
     * ctx.set_verify_mode(...)
     *
     *   ... configuring ctx
     *
     *   app.port(18080).ssl(ctx).run();
     */
}
