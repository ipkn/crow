#include "crow.h"
#include "json.h"

#include <sstream>

class ExampleLogHandler : public crow::ILogHandler {
    public:
        void log(string message, crow::LogLevel level) override {
            cerr << "ExampleLogHandler -> " << message;
        }
};

class ExampleMiddleware : public crow::IMiddleware {
    public:
        const char* tag_;
        ExampleMiddleware(const char* tag) : tag_(tag) {}
        void before_handle(const crow::request& req, crow::response& res)
        {
            CROW_LOG_INFO << "Middleware " << tag_ << " : before handle";
        }
        void after_handle(const crow::request& req, crow::response& res)
        {
            CROW_LOG_INFO << "Middleware " << tag_ << " : after handle";
        }
};

int main()
{
    crow::Crow app;

    app.use(std::make_shared<ExampleMiddleware>("A"));

    app.use([](const crow::request& req, crow::response& res){
        CROW_LOG_INFO << "Middleware B : before handle";
    },
    [](const crow::request& req, crow::response& res){
        CROW_LOG_INFO << "Middleware B : after handle";
    });

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

    CROW_ROUTE(app,"/add/<int>/<int>")
    ([](const crow::request& req, crow::response& res, int a, int b){
        std::ostringstream os;
        os << a+b;
        res.write(os.str());
        res.end();
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

    //crow::logger::setLogLevel(LogLevel::INFO);
    //crow::logger::setHandler(std::make_shared<ExampleLogHandler>());

    app.port(18080)
        .multithreaded()
        .run();
}
