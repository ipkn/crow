Routes define what happens when your client connects to a certain URL.<br>

##Macro
`CROW_ROUTE(app, url)`<br>
Can be replaced with `#!cpp app.route<crow::black_magick::get_parameter_tag(url)>(url)` or `#!cpp app.route_dynamic(url)` if you're using VS2013 or want runtime url evaluation. Although this usage is **NOT** recommended.
##App
Which app class to assign the route to.
##Path (URL)
Which relative path is assigned to the route.<br>
Using `/hello` means the client will need to access `http://example.com/hello` in order to access the route.<br>
A path can have parameters, for example `/hello/<int>` will allow a client to input an int into the url which will be in the handler (something like `http://example.com/hello/42`).<br>
Parameters can be `<int>`, `<uint>`, `<double>`, `<string>`, or `<path>`.<br>
It's worth nothing that the parameters also need to be defined in the handler, an example of using parameters would be to add 2 numbers based on input:
```cpp 
CROW_ROUTE(app, "/add/<int>/<int>")
([](int a, int b)
{
    return std::to_string(a+b);
});
```
you can see the first `<int>` is defined as `a` and the second as `b`. If you were to run this and call `http://example.com/add/1/2`, the result would be a page with `3`. Exciting!
##Handler
Basically a piece of code that gets executed whenever the client calls the associated route, usually in the form of a [lambda expression](https://en.cppreference.com/w/cpp/language/lambda). It can be as simple as `#!cpp ([](){return "Hello World"})`.<br><br>

###Request
Handlers can also use information from the request by adding it as a parameter `#!cpp ([](const crow::request& req){...})`.<br><br>

You can also access the url parameters in the handler using `#!cpp req.url_params.get("param_name");`. If the parameter doesn't exist, `nullptr` is returned.<br><br>

For more information on `crow::request` go [here](/reference/structcrow_1_1request.html).<br><br>

###Response
Crow also provides the ability to define a response in the parameters by using `#!cpp ([](const crow::request& req, crow::response& res){...})`.<br>
If you don't want to use the request you can write `#!cpp ([](const crow::request& , crow::response& res){...})`.<br>
 Yes I know there's a pull request to make it as simple as `#!cpp ([](crow::response& res){...})`, but I can't test it and add it to the repository while writing this documentation.<br><br>

Please note that in order to return a response defined as a parameter you'll need to use `res.end();`.<br><br>

Alternatively, you can define the response in the body and return it (`#!cpp ([](){return crow::response()})`).<br>

For more information on `crow::response` go [here](/reference/structcrow_1_1response.html).<br><br>

###return statement
A `crow::response` is very strictly tied to a route. If you can have something in a response constructor, you can return it in a handler.<br><br>
The main return type is `std::string`. although you could also return a `crow::json::wvalue` directly. ***(Support for more data types including third party libraries is coming soon)***<br><br>
For more information on the specific constructors for a `crow::response` go [here](/reference/structcrow_1_1response.html).