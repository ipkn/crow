//#define CROW_ENABLE_LOGGING
#define CROW_ENABLE_DEBUG
#include <iostream>
#include <vector>
#include "settings.h"
#undef CROW_LOG_LEVEL
#define CROW_LOG_LEVEL 0
#include "routing.h"
#include "utility.h"
#include "crow.h"
#include "json.h"
#include "mustache.h"
#include "middleware.h"
#include "query_string.h"

using namespace std;
using namespace crow;

struct Test { Test(); virtual void test() = 0; };
vector<Test*> tests;
Test::Test() { tests.push_back(this); }

bool failed__ = false;
void error_print()
{
    cerr << endl;
}

template <typename A, typename ...Args>
void error_print(const A& a, Args...args)
{
    cerr<<a;
    error_print(args...);
}

template <typename ...Args>
void fail(Args...args) { error_print(args...);failed__ = true; }

#define ASSERT_TRUE(x) if (!(x)) fail(__FILE__ ":", __LINE__, ": Assert fail: expected ", #x, " is true, at " __FILE__ ":",__LINE__)
#define ASSERT_EQUAL(a, b) if ((a) != (b)) fail(__FILE__ ":", __LINE__, ": Assert fail: expected ", (a), " actual " , (b),  ", " #a " == " #b ", at " __FILE__ ":",__LINE__)
#define ASSERT_NOTEQUAL(a, b) if ((a) == (b)) fail(__FILE__ ":", __LINE__, ": Assert fail: not expected ", (a), ", " #a " != " #b ", at " __FILE__ ":",__LINE__)
#define ASSERT_THROW(x) \
    try \
    { \
        x; \
        fail(__FILE__ ":", __LINE__, ": Assert fail: exception should be thrown"); \
    } \
    catch(std::exception&) \
    { \
    } 



#define TEST(x) struct test##x:public Test{void test();}x##_; \
    void test##x::test()
#define DISABLE_TEST(x) struct test##x{void test();}x##_; \
    void test##x::test()

TEST(Rule)
{
    TaggedRule<> r("/http/");
    r.name("abc");

    // empty handler - fail to validate
    try 
    {
        r.validate();
        fail("empty handler should fail to validate");
    }
    catch(runtime_error& e)
    {
    }

    int x = 0;

    // registering handler
    r([&x]{x = 1;return "";});

    r.validate();

    response res;

    // executing handler
    ASSERT_EQUAL(0, x);
    r.handle(request(), res, routing_params());
    ASSERT_EQUAL(1, x);

    // registering handler with request argument
    r([&x](const crow::request&){x = 2;return "";});

    r.validate();

    // executing handler
    ASSERT_EQUAL(1, x);
    r.handle(request(), res, routing_params());
    ASSERT_EQUAL(2, x);
}

TEST(ParameterTagging)
{
    static_assert(black_magic::is_valid("<int><int><int>"), "valid url");
    static_assert(!black_magic::is_valid("<int><int<<int>"), "invalid url");
    static_assert(!black_magic::is_valid("nt>"), "invalid url");
    ASSERT_EQUAL(1, black_magic::get_parameter_tag("<int>"));
    ASSERT_EQUAL(2, black_magic::get_parameter_tag("<uint>"));
    ASSERT_EQUAL(3, black_magic::get_parameter_tag("<float>"));
    ASSERT_EQUAL(3, black_magic::get_parameter_tag("<double>"));
    ASSERT_EQUAL(4, black_magic::get_parameter_tag("<str>"));
    ASSERT_EQUAL(4, black_magic::get_parameter_tag("<string>"));
    ASSERT_EQUAL(5, black_magic::get_parameter_tag("<path>"));
    ASSERT_EQUAL(6*6+6+1, black_magic::get_parameter_tag("<int><int><int>"));
    ASSERT_EQUAL(6*6+6+2, black_magic::get_parameter_tag("<uint><int><int>"));
    ASSERT_EQUAL(6*6+6*3+2, black_magic::get_parameter_tag("<uint><double><int>"));

    // url definition parsed in compile time, build into *one number*, and given to template argument
    static_assert(std::is_same<black_magic::S<uint64_t, double, int64_t>, black_magic::arguments<6*6+6*3+2>::type>::value, "tag to type container");
}

TEST(RoutingTest)
{
    SimpleApp app;
    int A{};
    uint32_t B{};
    double C{};
    string D{};
    string E{};

    CROW_ROUTE(app, "/0/<uint>")
    ([&](uint32_t b){
        B = b;
        return "OK";
    });

    CROW_ROUTE(app, "/1/<int>/<uint>")
    ([&](int a, uint32_t b){
        A = a; B = b;
        return "OK";
    });

    CROW_ROUTE(app, "/4/<int>/<uint>/<double>/<string>")
    ([&](int a, uint32_t b, double c, string d){
        A = a; B = b; C = c; D = d;
        return "OK";
    });

    CROW_ROUTE(app, "/5/<int>/<uint>/<double>/<string>/<path>")
    ([&](int a, uint32_t b, double c, string d, string e){
        A = a; B = b; C = c; D = d; E = e;
        return "OK";
    });

    app.validate();
    //app.debug_print();
	{
        request req;
        response res;

        req.url = "/-1";

        app.handle(req, res);

        ASSERT_EQUAL(404, res.code);
	}

    {
        request req;
        response res;

        req.url = "/0/1001999";

        app.handle(req, res);

        ASSERT_EQUAL(200, res.code);

        ASSERT_EQUAL(1001999, B);
    }

    {
        request req;
        response res;

        req.url = "/1/-100/1999";

        app.handle(req, res);

        ASSERT_EQUAL(200, res.code);

        ASSERT_EQUAL(-100, A);
        ASSERT_EQUAL(1999, B);
    }
    {
        request req;
        response res;

        req.url = "/4/5000/3/-2.71828/hellhere";
        req.add_header("TestHeader", "Value");

        app.handle(req, res);

        ASSERT_EQUAL(200, res.code);

        ASSERT_EQUAL(5000, A);
        ASSERT_EQUAL(3, B);
        ASSERT_EQUAL(-2.71828, C);
        ASSERT_EQUAL("hellhere", D);
    }
    {
        request req;
        response res;

        req.url = "/5/-5/999/3.141592/hello_there/a/b/c/d";
        req.add_header("TestHeader", "Value");

        app.handle(req, res);

        ASSERT_EQUAL(200, res.code);

        ASSERT_EQUAL(-5, A);
        ASSERT_EQUAL(999, B);
        ASSERT_EQUAL(3.141592, C);
        ASSERT_EQUAL("hello_there", D);
        ASSERT_EQUAL("a/b/c/d", E);
    }
}

TEST(simple_response_routing_params)
{
    ASSERT_EQUAL(100, response(100).code);
    ASSERT_EQUAL(200, response("Hello there").code);
    ASSERT_EQUAL(500, response(500, "Internal Error?").code);

    routing_params rp;
    rp.int_params.push_back(1);
    rp.int_params.push_back(5);
    rp.uint_params.push_back(2);
    rp.double_params.push_back(3);
    rp.string_params.push_back("hello");
    ASSERT_EQUAL(1, rp.get<int64_t>(0));
    ASSERT_EQUAL(5, rp.get<int64_t>(1));
    ASSERT_EQUAL(2, rp.get<uint64_t>(0));
    ASSERT_EQUAL(3, rp.get<double>(0));
    ASSERT_EQUAL("hello", rp.get<string>(0));
}

TEST(handler_with_response)
{
    SimpleApp app;
    CROW_ROUTE(app, "/")([](const crow::request&, crow::response&)
    {
    });
}

TEST(server_handling_error_request)
{
    static char buf[2048];
    SimpleApp app;
    CROW_ROUTE(app, "/")([]{return "A";});
    Server<SimpleApp> server(&app, 45451);
    auto _ = async(launch::async, [&]{server.run();});
    std::string sendmsg = "POX";
    asio::io_service is;
    {
        asio::ip::tcp::socket c(is);
        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));


        c.send(asio::buffer(sendmsg));

        try
        {
            c.receive(asio::buffer(buf, 2048));
            fail();
        }
        catch(std::exception& e)
        {
            //std::cerr << e.what() << std::endl;
        }
    }
    server.stop();
}

TEST(multi_server)
{
    static char buf[2048];
    SimpleApp app1, app2;
    CROW_ROUTE(app1, "/")([]{return "A";});
    CROW_ROUTE(app2, "/")([]{return "B";});

    Server<SimpleApp> server1(&app1, 45451);
    Server<SimpleApp> server2(&app2, 45452);

    auto _ = async(launch::async, [&]{server1.run();});
    auto _2 = async(launch::async, [&]{server2.run();});

    std::string sendmsg = "POST /\r\nContent-Length:3\r\nX-HeaderTest: 123\r\n\r\nA=B\r\n";
    asio::io_service is;
    {
        asio::ip::tcp::socket c(is);
        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));


        c.send(asio::buffer(sendmsg));

        size_t recved = c.receive(asio::buffer(buf, 2048));
        ASSERT_EQUAL('A', buf[recved-1]);
    }

    {
        asio::ip::tcp::socket c(is);
        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45452));

        for(auto ch:sendmsg)
        {
            char buf[1] = {ch};
            c.send(asio::buffer(buf));
        }

        size_t recved = c.receive(asio::buffer(buf, 2048));
        ASSERT_EQUAL('B', buf[recved-1]);
    }

    server1.stop();
    server2.stop();
}

TEST(json_read)
{
	{
        const char* json_error_tests[] = 
        {
            "{} 3", "{{}", "{3}",
            "3.4.5", "+3", "3-2", "00", "03", "1e3e3", "1e+.3",
            "nll", "f", "t",
            "{\"x\":3,}",
            "{\"x\"}",
            "{\"x\":3   q}",
            "{\"x\":[3 4]}",
            "{\"x\":[\"",
            "{\"x\":[[], 4],\"y\",}",
            "{\"x\":[3",
            "{\"x\":[ null, false, true}",
        };
        for(auto s:json_error_tests)
        {
            auto x = json::load(s);
            if (x)
            {
                fail("should fail to parse ", s);
                return;
            }
        }
	}

    auto x = json::load(R"({"message":"hello, world"})");
    if (!x)
        fail("fail to parse");
    ASSERT_EQUAL("hello, world", x["message"]);
    ASSERT_EQUAL(1, x.size());
    ASSERT_EQUAL(false, x.has("mess"));
    ASSERT_THROW(x["mess"]);
    // TODO returning false is better than exception
    //ASSERT_THROW(3 == x["message"]);
    ASSERT_EQUAL(12, x["message"].size());

    std::string s = R"({"int":3,     "ints"  :[1,2,3,4,5]		})";
    auto y = json::load(s);
    ASSERT_EQUAL(3, y["int"]);
    ASSERT_EQUAL(3.0, y["int"]);
    ASSERT_NOTEQUAL(3.01, y["int"]);
	ASSERT_EQUAL(5, y["ints"].size());
	ASSERT_EQUAL(1, y["ints"][0]);
	ASSERT_EQUAL(2, y["ints"][1]);
	ASSERT_EQUAL(3, y["ints"][2]);
	ASSERT_EQUAL(4, y["ints"][3]);
	ASSERT_EQUAL(5, y["ints"][4]);
	ASSERT_EQUAL(1u, y["ints"][0]);
	ASSERT_EQUAL(1.f, y["ints"][0]);

	int q = (int)y["ints"][1];
	ASSERT_EQUAL(2, q);
	q = y["ints"][2].i();
	ASSERT_EQUAL(3, q);

}

TEST(json_read_unescaping)
{
    {
        auto x = json::load(R"({"data":"\ud55c\n\t\r"})");
        if (!x)
        {
            fail("fail to parse");
            return;
        }
        ASSERT_EQUAL(6, x["data"].size());
        ASSERT_EQUAL("한\n\t\r", x["data"]);
    }
    {
        // multiple r_string instance
        auto x = json::load(R"({"data":"\ud55c\n\t\r"})");
        auto a = x["data"].s();
        auto b = x["data"].s();
        ASSERT_EQUAL(6, a.size());
        ASSERT_EQUAL(6, b.size());
        ASSERT_EQUAL(6, x["data"].size());
    }
}

TEST(json_write)
{
    json::wvalue x;
    x["message"] = "hello world";
    ASSERT_EQUAL(R"({"message":"hello world"})", json::dump(x));
    x["message"] = std::string("string value");
    ASSERT_EQUAL(R"({"message":"string value"})", json::dump(x));
    x["message"]["x"] = 3;
    ASSERT_EQUAL(R"({"message":{"x":3}})", json::dump(x));
    x["message"]["y"] = 5;
    ASSERT_TRUE(R"({"message":{"x":3,"y":5}})" == json::dump(x) || R"({"message":{"y":5,"x":3}})" == json::dump(x));
    x["message"] = 5.5;
    ASSERT_EQUAL(R"({"message":5.5})", json::dump(x));

    json::wvalue y;
    y["scores"][0] = 1;
    y["scores"][1] = "king";
    y["scores"][2] = 3.5;
    ASSERT_EQUAL(R"({"scores":[1,"king",3.5]})", json::dump(y));

    y["scores"][2][0] = "real";
    y["scores"][2][1] = false;
    y["scores"][2][2] = true;
    ASSERT_EQUAL(R"({"scores":[1,"king",["real",false,true]]})", json::dump(y));

    y["scores"]["a"]["b"]["c"] = nullptr;
    ASSERT_EQUAL(R"({"scores":{"a":{"b":{"c":null}}}})", json::dump(y));
}

TEST(template_basic)
{
    auto t = crow::mustache::compile(R"---(attack of {{name}})---");
    crow::mustache::context ctx;
    ctx["name"] = "killer tomatoes";
    auto result = t.render(ctx);
    ASSERT_EQUAL("attack of killer tomatoes", result);
    //crow::mustache::load("basic.mustache");
}

TEST(template_load)
{
    crow::mustache::set_base(".");
    ofstream("test.mustache") << R"---(attack of {{name}})---";
    auto t = crow::mustache::load("test.mustache");
    crow::mustache::context ctx;
    ctx["name"] = "killer tomatoes";
    auto result = t.render(ctx);
    ASSERT_EQUAL("attack of killer tomatoes", result);
    unlink("test.mustache");
}

int testmain()
{
    bool failed = false;
    for(auto t:tests)
    {
        failed__ = false;
        try
        {
            //cerr << typeid(*t).name() << endl;
            t->test();
        }
        catch(std::exception& e)
        {
            fail(e.what());
        }
        if (failed__)
        {
            cerr << "F";
            cerr << '\t' << typeid(*t).name() << endl;
            failed = true;
        }
        else
            cerr << ".";
    }
    cerr<<endl;
    return failed ? -1 : 0;
}

TEST(black_magic)
{
    using namespace black_magic;
    static_assert(std::is_same<void, last_element_type<int, char, void>::type>::value, "last_element_type");
    static_assert(std::is_same<char, pop_back<int, char, void>::rebind<last_element_type>::type>::value, "pop_back");
    static_assert(std::is_same<int, pop_back<int, char, void>::rebind<pop_back>::rebind<last_element_type>::type>::value, "pop_back");
}

struct NullMiddleware
{
    struct context {};

    template <typename AllContext>
    void before_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
    {}

    template <typename AllContext>
    void after_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
    {}
};

struct NullSimpleMiddleware
{
    struct context {};

    void before_handle(request& req, response& res, context& ctx)
    {}

    void after_handle(request& req, response& res, context& ctx)
    {}
};

TEST(middleware_simple)
{
    App<NullMiddleware, NullSimpleMiddleware> app;
    decltype(app)::server_t server(&app, 45451);
    CROW_ROUTE(app, "/")([&](const crow::request& req)
    {
        app.get_context<NullMiddleware>(req);
        app.get_context<NullSimpleMiddleware>(req);
        return "";
    });
}

struct IntSettingMiddleware
{
    struct context { int val; };

    template <typename AllContext>
    void before_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
    {
        ctx.val = 1;
    }

    template <typename AllContext>
    void after_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
    {
        ctx.val = 2;
    }
};

std::vector<std::string> test_middleware_context_vector;

struct FirstMW
{
    struct context 
    { 
        std::vector<string> v; 
    };

    void before_handle(request& req, response& res, context& ctx)
    {
        ctx.v.push_back("1 before");
    }

    void after_handle(request& req, response& res, context& ctx)
    {
        ctx.v.push_back("1 after");
        test_middleware_context_vector = ctx.v;
    }
};

struct SecondMW
{
    struct context {};
    template <typename AllContext>
    void before_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
    {
        all_ctx.template get<FirstMW>().v.push_back("2 before");
        if (req.url == "/break")
            res.end();
    }

    template <typename AllContext>
    void after_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
    {
        all_ctx.template get<FirstMW>().v.push_back("2 after");
    }
};

struct ThirdMW
{
    struct context {};
    template <typename AllContext>
    void before_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
    {
        all_ctx.template get<FirstMW>().v.push_back("3 before");
    }

    template <typename AllContext>
    void after_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
    {
        all_ctx.template get<FirstMW>().v.push_back("3 after");
    }
};

TEST(middleware_context)
{

    static char buf[2048];
    // SecondMW depends on FirstMW (it uses all_ctx.get<FirstMW>)
    // so it leads to compile error if we remove FirstMW from definition
    // App<IntSettingMiddleware, SecondMW> app;
    // or change the order of FirstMW and SecondMW
    // App<IntSettingMiddleware, SecondMW, FirstMW> app;

    App<IntSettingMiddleware, FirstMW, SecondMW, ThirdMW> app;

    int x{};
    CROW_ROUTE(app, "/")([&](const request& req){
        {
            auto& ctx = app.get_context<IntSettingMiddleware>(req);
            x = ctx.val;
        }
        {
            auto& ctx = app.get_context<FirstMW>(req);
            ctx.v.push_back("handle");
        }

        return "";
    });
    CROW_ROUTE(app, "/break")([&](const request& req){
        {
            auto& ctx = app.get_context<FirstMW>(req);
            ctx.v.push_back("handle");
        }

        return "";
    });

    decltype(app)::server_t server(&app, 45451);
    auto _ = async(launch::async, [&]{server.run();});
    std::string sendmsg = "GET /\r\n\r\n";
    asio::io_service is;
    {
        asio::ip::tcp::socket c(is);
        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));


        c.send(asio::buffer(sendmsg));

        c.receive(asio::buffer(buf, 2048));
        c.close();
    }
    {
        auto& out = test_middleware_context_vector;
        ASSERT_EQUAL(1, x);
        ASSERT_EQUAL(7, out.size());
        ASSERT_EQUAL("1 before", out[0]);
        ASSERT_EQUAL("2 before", out[1]);
        ASSERT_EQUAL("3 before", out[2]);
        ASSERT_EQUAL("handle", out[3]);
        ASSERT_EQUAL("3 after", out[4]);
        ASSERT_EQUAL("2 after", out[5]);
        ASSERT_EQUAL("1 after", out[6]);
    }
    std::string sendmsg2 = "GET /break\r\n\r\n";
    {
        asio::ip::tcp::socket c(is);
        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));

        c.send(asio::buffer(sendmsg2));

        c.receive(asio::buffer(buf, 2048));
        c.close();
    }
    {
        auto& out = test_middleware_context_vector;
        ASSERT_EQUAL(4, out.size());
        ASSERT_EQUAL("1 before", out[0]);
        ASSERT_EQUAL("2 before", out[1]);
        ASSERT_EQUAL("2 after", out[2]);
        ASSERT_EQUAL("1 after", out[3]);
    }
    server.stop();
}

TEST(middleware_cookieparser)
{
    static char buf[2048];

    App<CookieParser> app;

    std::string value1;
    std::string value2;

    CROW_ROUTE(app, "/")([&](const request& req){
        {
            auto& ctx = app.get_context<CookieParser>(req);
            value1 = ctx.get_cookie("key1");
            value2 = ctx.get_cookie("key2");
        }

        return "";
    });

    decltype(app)::server_t server(&app, 45451);
    auto _ = async(launch::async, [&]{server.run();});
    std::string sendmsg = "GET /\r\nCookie: key1=value1; key2=\"val\\\"ue2\"\r\n\r\n";
    asio::io_service is;
    {
        asio::ip::tcp::socket c(is);
        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));

        c.send(asio::buffer(sendmsg));

        c.receive(asio::buffer(buf, 2048));
        c.close();
    }
    {
        ASSERT_EQUAL("value1", value1);
        ASSERT_EQUAL("val\"ue2", value2);
    }
    server.stop();
}

TEST(bug_quick_repeated_request)
{
    static char buf[2048];

    SimpleApp app;

    CROW_ROUTE(app, "/")([&]{
        return "hello";
    });

    decltype(app)::server_t server(&app, 45451);
    auto _ = async(launch::async, [&]{server.run();});
    std::string sendmsg = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    asio::io_service is;
    {
        std::vector<std::future<void>> v;
        for(int i = 0; i < 5; i++)
        {
            v.push_back(async(launch::async, 
                [&]
                {
                    asio::ip::tcp::socket c(is);
                    c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));

                    for(int j = 0; j < 5; j ++)
                    {
                        c.send(asio::buffer(sendmsg));

                        size_t received = c.receive(asio::buffer(buf, 2048));
                        ASSERT_EQUAL("hello", std::string(buf + received - 5, buf + received));
                    }
                    c.close();
                }));
        }
    }
    server.stop();
}

TEST(simple_url_params)
{
    static char buf[2048];

    SimpleApp app;

    query_string last_url_params;

    CROW_ROUTE(app, "/params")
    ([&last_url_params](const crow::request& req){
        last_url_params = move(req.url_params);
        return "OK";
    });

    ///params?h=1&foo=bar&lol&count[]=1&count[]=4&pew=5.2

    decltype(app)::server_t server(&app, 45451);
    auto _ = async(launch::async, [&]{server.run();});
    asio::io_service is;
    std::string sendmsg;

    // check single presence
    sendmsg = "GET /params?foobar\r\n\r\n";
    {
        asio::ip::tcp::socket c(is);
        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));
        c.send(asio::buffer(sendmsg));
        c.receive(asio::buffer(buf, 2048));
        c.close();

        ASSERT_TRUE(last_url_params.get("missing") == nullptr);
        ASSERT_TRUE(last_url_params.get("foobar") != nullptr);
    }
    // check multiple presence
    sendmsg = "GET /params?foo&bar&baz\r\n\r\n";
    {
        asio::ip::tcp::socket c(is);
        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));
        c.send(asio::buffer(sendmsg));
        c.receive(asio::buffer(buf, 2048));
        c.close();

        ASSERT_TRUE(last_url_params.get("missing") == nullptr);
        ASSERT_TRUE(last_url_params.get("foo") != nullptr);
        ASSERT_TRUE(last_url_params.get("bar") != nullptr);
        ASSERT_TRUE(last_url_params.get("baz") != nullptr);
    }
    // check single value
    sendmsg = "GET /params?hello=world\r\n\r\n";
    {
        asio::ip::tcp::socket c(is);
        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));
        c.send(asio::buffer(sendmsg));
        c.receive(asio::buffer(buf, 2048));
        c.close();        

        ASSERT_EQUAL(string(last_url_params.get("hello")), "world");
    }
    // check multiple value
    sendmsg = "GET /params?hello=world&left=right&up=down\r\n\r\n";
    {
        asio::ip::tcp::socket c(is);
        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));
        c.send(asio::buffer(sendmsg));
        c.receive(asio::buffer(buf, 2048));
        c.close();

        ASSERT_EQUAL(string(last_url_params.get("hello")), "world");
        ASSERT_EQUAL(string(last_url_params.get("left")), "right");
        ASSERT_EQUAL(string(last_url_params.get("up")),  "down");
    }
    // check multiple value, multiple types
    sendmsg = "GET /params?int=100&double=123.45&boolean=1\r\n\r\n";
    {
        asio::ip::tcp::socket c(is);
        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));
        c.send(asio::buffer(sendmsg));
        c.receive(asio::buffer(buf, 2048));        
        c.close();    

        ASSERT_EQUAL(boost::lexical_cast<int>(last_url_params.get("int")), 100);
        ASSERT_EQUAL(boost::lexical_cast<double>(last_url_params.get("double")), 123.45);
        ASSERT_EQUAL(boost::lexical_cast<bool>(last_url_params.get("boolean")), true);
    }
    // check single array value
    sendmsg = "GET /params?tmnt[]=leonardo\r\n\r\n";
    {
        asio::ip::tcp::socket c(is);

        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));
        c.send(asio::buffer(sendmsg));
        c.receive(asio::buffer(buf, 2048));
        c.close();        
        
        ASSERT_TRUE(last_url_params.get("tmnt") == nullptr);
        ASSERT_EQUAL(last_url_params.get_list("tmnt").size(), 1);
        ASSERT_EQUAL(string(last_url_params.get_list("tmnt")[0]), "leonardo");
    }
    // check multiple array value
    sendmsg = "GET /params?tmnt[]=leonardo&tmnt[]=donatello&tmnt[]=raphael\r\n\r\n";
    {
        asio::ip::tcp::socket c(is);

        c.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), 45451));
        c.send(asio::buffer(sendmsg));
        c.receive(asio::buffer(buf, 2048));
        c.close();        
        
        ASSERT_EQUAL(last_url_params.get_list("tmnt").size(), 3);
        ASSERT_EQUAL(string(last_url_params.get_list("tmnt")[0]), "leonardo");
        ASSERT_EQUAL(string(last_url_params.get_list("tmnt")[1]), "donatello");
        ASSERT_EQUAL(string(last_url_params.get_list("tmnt")[2]), "raphael");
    }
    server.stop();
}

int main()
{
    return testmain();
}
