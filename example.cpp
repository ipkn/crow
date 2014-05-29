#include "crow.h"
#include "json.h"

#include <sstream>

class ExampleLogHandler : public crow::ILogHandler {
    public:
        void log(string message, crow::LogLevel level) override {
            cerr << "ExampleLogHandler -> " << message;
        }
};

class ExampleMiddlewareHandler : public crow::IMiddlewareHandler {
    public:
        crow::response handle(const crow::request& req, crow::middleware::context *c) override {
            CROW_LOG_DEBUG << "MIDDLEWARE PRE";
            auto result = c->next();
            CROW_LOG_DEBUG << "MIDDLEWARE POST";

            crow::json::wvalue x;
            x["message"] = result.body;

            return x;
        }
};

int main()
{
    crow::Crow app;

    CROW_ROUTE(app, "/")
        .name("hello")
    ([]{
        return "Hello World!";
    });

    CROW_ROUTE(app, "/about")
    ([](){
        return "About Crow example.";
    });

    // simple json response
    CROW_ROUTE(app, "/json")
    ([]{
        crow::json::wvalue x;
        x["message"] = "Hello, World!";
        return x;
    });

    CROW_ROUTE(app,"/hello/<int>")
    ([](int count){
        if (count > 100)
            return crow::response(400);
        std::ostringstream os;
        os << count << " bottles of beer!";
        return crow::response(os.str());
    });

    // Compile error with message "Handler type is mismatched with URL paramters"
    //CROW_ROUTE(app,"/another/<int>")
    //([](int a, int b){
        //return crow::response(500);
    //});

    // more json example
    CROW_ROUTE(app, "/add_json")
    ([](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x)
            return crow::response(400);
        int sum = x["a"].i()+x["b"].i();
        std::ostringstream os;
        os << sum;
        return crow::response{os.str()};
    });

    crow::middleware::use(std::make_shared<ExampleMiddlewareHandler>());

    //crow::logger::setLogLevel(LogLevel::INFO);
    //crow::logger::setHandler(std::make_shared<ExampleLogHandler>());

    app.port(18080)
        .multithreaded()
        .run();
}
