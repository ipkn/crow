#include "flask.h"

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

    app.port(8080)
       .run();
}
