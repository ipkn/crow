//#define FLASK_ENABLE_LOGGING
#include <iostream>
#include <vector>
#include "routing.h"
#include "utility.h"
#include "flask.h"
#include "json.h"
using namespace std;
using namespace flask;

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

#define ASSERT_TRUE(x) if (!(x)) fail("Assert fail: expected ", #x, " is true, at " __FILE__ ":",__LINE__)
#define ASSERT_EQUAL(a, b) if ((a) != (b)) fail("Assert fail: expected ", (a), " actual " , (b),  ", " #a " == " #b ", at " __FILE__ ":",__LINE__)
#define ASSERT_NOTEQUAL(a, b) if ((a) == (b)) fail("Assert fail: not expected ", (a), ", " #a " != " #b ", at " __FILE__ ":",__LINE__)
#define ASSERT_THROW(x) \
    try \
    { \
        x; \
        fail("Assert fail: exception should be thrown"); \
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

    // executing handler
    ASSERT_EQUAL(0, x);
    r.handle(request(), routing_params());
    ASSERT_EQUAL(1, x);

    // registering handler with request argument
    r([&x](const flask::request&){x = 2;return "";});

    r.validate();

    // executing handler
    ASSERT_EQUAL(1, x);
    r.handle(request(), routing_params());
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
    Flask app;
    int A{};
    uint32_t B{};
    double C{};
    string D{};
    string E{};

    FLASK_ROUTE(app, "/0/<uint>")
    ([&](uint32_t b){
        B = b;
        return "OK";
    });

    FLASK_ROUTE(app, "/1/<int>/<uint>")
    ([&](int a, uint32_t b){
        A = a; B = b;
        return "OK";
    });

    FLASK_ROUTE(app, "/4/<int>/<uint>/<double>/<string>")
    ([&](int a, uint32_t b, double c, string d){
        A = a; B = b; C = c; D = d;
        return "OK";
    });

    FLASK_ROUTE(app, "/5/<int>/<uint>/<double>/<string>/<path>")
    ([&](int a, uint32_t b, double c, string d, string e){
        A = a; B = b; C = c; D = d; E = e;
        return "OK";
    });

    app.validate();
    //app.debug_print();

    {
        request req;

        req.url = "/0/1001999";

        auto res = app.handle(req);

        ASSERT_EQUAL(200, res.code);

        ASSERT_EQUAL(1001999, B);
    }

    {
        request req;

        req.url = "/1/-100/1999";

        auto res = app.handle(req);

        ASSERT_EQUAL(200, res.code);

        ASSERT_EQUAL(-100, A);
        ASSERT_EQUAL(1999, B);
    }
    {
        request req;

        req.url = "/4/5000/3/-2.71828/hellhere";
        req.headers["TestHeader"] = "Value";

        auto res = app.handle(req);

        ASSERT_EQUAL(200, res.code);

        ASSERT_EQUAL(5000, A);
        ASSERT_EQUAL(3, B);
        ASSERT_EQUAL(-2.71828, C);
        ASSERT_EQUAL("hellhere", D);
    }
    {
        request req;

        req.url = "/5/-5/999/3.141592/hello_there/a/b/c/d";
        req.headers["TestHeader"] = "Value";

        auto res = app.handle(req);

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

TEST(server_handling_error_request)
{
    static char buf[2048];
    Flask app;
    FLASK_ROUTE(app, "/")([]{return "A";});
    Server<Flask> server(&app, 45451);
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
            std::cerr << e.what() << std::endl;
        }
    }
    server.stop();
}

TEST(multi_server)
{
    static char buf[2048];
    Flask app1, app2;
    FLASK_ROUTE(app1, "/")([]{return "A";});
    FLASK_ROUTE(app2, "/")([]{return "B";});

    Server<Flask> server1(&app1, 45451);
    Server<Flask> server2(&app2, 45452);

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
            std::cerr << ch << '(' << (int)ch<<')'<<std::endl;
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
    ASSERT_THROW(3 == x["message"]);
    ASSERT_THROW(x["message"].size());

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
            failed = true;
        }
        else
            cerr << ".";
    }
    cerr<<endl;
    return failed ? -1 : 0;
}

int main()
{
    return testmain();
}
