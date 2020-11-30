<p align="center"><img src="https://pixeldrain.com/api/file/DMc7xYye" width=600></p>

<h4 align="center">A Fast and Easy to use microframework for the web.</h4>
<p align="center">
<a href="https://travis-ci.com/crowcpp/crow"><img src="https://travis-ci.com/crowcpp/crow.svg?branch=master" alt="Build Status"></a>
<a href="https://coveralls.io/github/crowcpp/crow?branch=master"><img src="https://coveralls.io/repos/github/crowcpp/crow/badge.svg?branch=master" alt="Coverage Status"></a>
<a href="https://crowcpp.github.io/crow"><img src="https://img.shields.io/badge/-Documentation-informational" alt="Documentation"></a>
<a href="https://gitter.im/crowfork/community?utm_source=badge&amp;utm_medium=badge&amp;utm_campaign=pr-badge"><img src="https://badges.gitter.im/crowfork/community.svg" alt="Gitter"></a>
</p>


## Description

Crow is a C++ microframework for running web services. It uses routing similar to Python's Flask which makes it easy to use. It is also extremely fast, beating multiple existing C++ frameworks as well as non C++ frameworks.

### Features
 - Easy Routing (similar to flask).
 - Type-safe Handlers.
 - Blazingly fast (see [this benchmark](https://github.com/ipkn/crow-benchmark) and [this benchmark](https://github.com/guteksan/REST-CPP-benchmark)).
 - Built in JSON support.
 - [Mustache](http://mustache.github.io/) based templating library (`crow::mustache`).
 - Header only library (single header file available).
 - Middleware support for extensions.
 - HTTP/1.1 and Websocket support.
 - Multi-part request and response support.
 - Uses modern C++ (11/14)

### Still in development
 - [HTTP/2 support](https://github.com/crowcpp/crow/issues/8)

## Documentation
Available [here](https://crowcpp.github.io/crow).

## Examples

#### Hello World
```c++
#include "crow.h"

int main()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([](){
        return "Hello world";
    });

    app.port(18080).multithreaded().run();
}
```

#### JSON Response
```cpp
CROW_ROUTE(app, "/json")
([]{
    crow::json::wvalue x;
    x["message"] = "Hello, World!";
    return x;
});
```

#### Arguments
```cpp
CROW_ROUTE(app,"/hello/<int>")
([](int count){
    if (count > 100)
        return crow::response(400);
    std::ostringstream os;
    os << count << " bottles of beer!";
    return crow::response(os.str());
});
```
Handler arguments type check at compile time
```cpp
// Compile error with message "Handler type is mismatched with URL paramters"
CROW_ROUTE(app,"/another/<int>")
([](int a, int b){
    return crow::response(500);
});
```

#### Handling JSON Requests
```cpp
CROW_ROUTE(app, "/add_json")
.methods("POST"_method)
([](const crow::request& req){
    auto x = crow::json::load(req.body);
    if (!x)
        return crow::response(400);
    int sum = x["a"].i()+x["b"].i();
    std::ostringstream os;
    os << sum;
    return crow::response{os.str()};
});
```

More examples can be found [here](https://github.com/crowcpp/crow/tree/master/examples).

## Setting Up / Building
Available [here](https://crowcpp.github.io/crow/getting_started/setup).


### Attributions

Crow uses the following libraries.

    http-parser (used for converting http strings to crow::request objects)

    https://github.com/nodejs/http-parser

    http_parser.c is based on src/http/ngx_http_parse.c from NGINX copyright
    Igor Sysoev.

    Additional changes are licensed under the same terms as NGINX and
    copyright Joyent, Inc. and other Node contributors. All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE. 


    qs_parse (used for reading query string parameters)

    https://github.com/bartgrantham/qs_parse

    Copyright (c) 2010 Bart Grantham
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.


    TinySHA1 (used during the websocket handshake, not for security)

    https://github.com/mohaps/TinySHA1

    TinySHA1 - a header only implementation of the SHA1 algorithm. Based on the implementation in boost::uuid::details

    Copyright (c) 2012-22 SAURAV MOHAPATRA mohaps@gmail.com
    Permission to use, copy, modify, and distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
