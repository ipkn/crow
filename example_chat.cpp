#include "crow.h"
#include "json.h"
#include "mustache.h"
#include <string>
#include <vector>

using namespace std;

vector<string> msgs;
vector<crow::response*> ress;

void broadcast(const string& msg)
{
    msgs.push_back(msg);
    crow::json::wvalue x;
    x["msgs"][0] = msgs.back();
    x["last"] = msgs.size();
    string body = crow::json::dump(x);
    for(auto* res:ress)
    {
        CROW_LOG_DEBUG << res->p << " replied: " << body;
        res->end(body);
    }
    ress.clear();
}

int main()
{
    crow::App app;
    crow::mustache::set_base(".");

    CROW_ROUTE(app, "/")
    ([]{
        crow::mustache::context ctx;
        return crow::mustache::load("example_chat.html").render();
    });

    CROW_ROUTE(app, "/logs")
    ([]{
        CROW_LOG_INFO << "logs requested";
        crow::json::wvalue x;
        for(int i = max(0, (int)msgs.size()-100); i < (int)msgs.size(); i++)
            x["msgs"][i] = msgs[i];
        x["last"] = msgs.size();
        CROW_LOG_INFO << "logs completed";
        return x;
    });

    CROW_ROUTE(app, "/logs/<int>")
    ([](const crow::request& req, crow::response& res, int after){
        CROW_LOG_INFO << "logs with last " << after;
        if (after < (int)msgs.size())
        {
            crow::json::wvalue x;
            for(int i = after; i < (int)msgs.size(); i ++)
                x["msgs"][i-after] = msgs[i];
            x["last"] = msgs.size();

            res.write(crow::json::dump(x));
            res.end();
        }
        else
        {
            ress.push_back(&res);
            CROW_LOG_DEBUG << res.p << " stored";
        }
    });

    CROW_ROUTE(app, "/send")
    ([](const crow::request& req)
    {
        CROW_LOG_INFO << "msg from client: " << req.body;
        broadcast(req.body);
        return "";
    });

    app.port(18080)
        //.multithreaded()
        .run();
}
