#include "crow.h"

int main()
{
    crow::LocalApp app;

    CROW_ROUTE(app, "/")
    ([]() {
        return "Hello Unix Socket world!";
    });

    ::unlink("/tmp/local_sock"); // Remove previous binding.
    app.path("/tmp/local_sock").run();
}
