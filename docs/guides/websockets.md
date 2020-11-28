Websockets are a way of connecting a client and a server without the request response nature of HTTP.<br><br>

To create a websocket in Crow, you need a websocket route.<br>
A websocket route differs from a normal route quite a bit. While it uses the same `CROW_ROUTE(app, "/url")` macro, that's about where the similarities end.<br>
A websocket route follows the macro with `.websocket()` which is then followed by a series of methods (with handlers inside) for each event. These are:

- `#!cpp onopen([&](crow::websocket::connection& conn){handler code goes here})`
- `#!cpp onaccept([&](const crow::request&){handler code goes here})` (This handler has to return bool)
- `#!cpp onmessage([&](crow::websocket::connection& conn, const std::string message, bool is_binary){handler code goes here})`
- `#!cpp onclose([&](crow::websocket::connection& conn, const std::string reason){handler code goes here})`
- `#!cpp onerror([&](crow::websocket::connection& conn){handler code goes here})`<br><br>

These event methods and their handlers can be chained. The full Route should look similar to this:
```cpp
CROW_ROUTE(app, "/ws")
    .websocket()
    .onopen([&](crow::websocket::connection& conn){
            do_something();
            })
    .onclose([&](crow::websocket::connection& conn, const std::string& reason){
            do_something();
            })
    .onmessage([&](crow::websocket::connection& /*conn*/, const std::string& data, bool is_binary){
                if (is_binary)
                    do_something(data);
                else
                    do_something_else(data);
            });
```
<br><br>
For more info go [here](/reference/classcrow_1_1_web_socket_rule.html).