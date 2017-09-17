#include "crow.h"
#include <unordered_set>
#include <mutex>


int main()
{
    crow::SimpleApp app;

    std::mutex mtx;;
    std::unordered_set<crow::websocket::connection*> users;

    CROW_ROUTE(app, "/ws")
        .websocket()
        .onopen([&](crow::websocket::connection& conn){
                CROW_LOG_INFO << "new websocket connection";
                std::lock_guard<std::mutex> _(mtx);
                users.insert(&conn);
                })
        .onclose([&](crow::websocket::connection& conn, const std::string& reason){
                CROW_LOG_INFO << "websocket connection closed: " << reason;
                std::lock_guard<std::mutex> _(mtx);
                users.erase(&conn);
                })
        .onmessage([&](crow::websocket::connection& /*conn*/, const std::string& data, bool is_binary){
                std::lock_guard<std::mutex> _(mtx);
                for(auto u:users)
                    if (is_binary)
                        u->send_binary(data);
                    else
                        u->send_text(data);
                });

    CROW_ROUTE(app, "/")
    ([]{
        char name[256];
        gethostname(name, 256);
        crow::mustache::context x;
        x["servername"] = name;
	
        auto page = crow::mustache::load("ws.html");
        return page.render(x);
     });

    app.port(40080)
        .multithreaded()
        .run();
}
