#include "flask.h"

#include <iostream>

int main()
{
    flask::Flask app;

    app.route("/")
       .name("hello")
    ([]{
        return "Hello World!";
    });

    app.route("/about")
    ([]{
        return "About Flask example.";
    });

    //app.route("/hello/<int>");
    //([]{
        //return "About Flask example.";
    //});

    app.port(8080)
       .run();
}
