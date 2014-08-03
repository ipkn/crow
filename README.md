# Crow

Crow is C++ microframework for web. (inspired by Python Flask)

## Features

 - Easy routing
   - Similiar to Flask
   - Type-safe Handlers (see Example)
 - Fast built-in JSON parser (crow::json)
 - [Mustache](http://mustache.github.io/) based templating library (crow::mustache)

## Still in development   
 - Built-in ORM
 - Middleware support

## Example

```c++

#include "crow.h"
#include "json.h"

#include <sstream>

int main()
{
    crow::Crow app;

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

    // argument
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

    app.port(18080)
        .multithreaded()
        .run();
}
```

## How to Build

### Requirements

 - C++ compiler with good C++11 support (tested with g++>=4.8)
 - boost library
 - tcmalloc (optional)

### Ubuntu
#### Installing missing dependencies
    sudo apt-get install build-essential libtcmalloc-minimal4 && sudo ln -s /usr/lib/libtcmalloc_minimal.so.4 /usr/lib/libtcmalloc_minimal.so

#### Building
    git submodule init && git submodule update
    make -j$(($(grep -c '^processor' /proc/cpuinfo)+1))

### OSX
#### Installing missing dependencies
    brew install boost google-perftools

#### Building
    git submodule init && git submodule update
    make -j$(($(sysctl -a | grep machdep.cpu.thread_count | awk -F " " '{print $2}')+1))

