#include "flask.h"
#include "json.h"

#include <sstream>

int main()
{
    flask::Flask app;

    FLASK_ROUTE(app, "/")
        .name("hello")
    ([]{
        return "Hello World!";
    });

    FLASK_ROUTE(app, "/about")
    ([](){
        return "About Flask example.";
    });

    // simple json response
    FLASK_ROUTE(app, "/json")
    ([]{
        flask::json::wvalue x;
        x["message"] = "Hello, World!";
        return x;
    });

    FLASK_ROUTE(app,"/hello/<int>")
    ([](int count){
        if (count > 100)
            return flask::response(400);
        std::ostringstream os;
        os << count << " bottles of beer!";
        return flask::response(os.str());
    });

    // Compile error with message "Handler type is mismatched with URL paramters"
    //FLASK_ROUTE(app,"/another/<int>")
    //([](int a, int b){
        //return flask::response(500);
    //});

    // more json example
    FLASK_ROUTE(app, "/add_json")
    ([](const flask::request& req){
        auto x = flask::json::load(req.body);
        if (!x)
            return flask::response(400);
        int sum = x["a"].i()+x["b"].i();
        std::ostringstream os;
        os << sum;
        return flask::response{os.str()};
    });

    app.port(18080)
        .run();
}
