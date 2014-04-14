#include <iostream>
#include <vector>
#include "routing.h"
#include "utility.h"
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
void error_print(A a, Args...args)
{
    cerr<<a;
    error_print(args...);
}

template <typename ...Args>
void fail(Args...args) { error_print(args...);failed__ = true; }

#define ASSERT_EQUAL(a, b) if (a != b) fail("Assert fail: expected ", (a), " actual " , b,  ", " #a " == " #b ", at " __FILE__ ":",__LINE__)
#define ASSERT_NOTEQUAL(a, b) if (a != b) fail("Assert fail: not expected ", (a), ", " #a " != " #b ", at " __FILE__ ":",__LINE__)
#define TEST(x) struct test##x:public Test{void test();}x##_; \
    void test##x::test()

TEST(Rule)
{
    Rule r("/http/");
    r.name("abc");

    // empty handler - fail to validate
    try 
    {
        r.validate();
        fail();
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
}

TEST(ParameterTagging)
{
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
    static_assert(is_same<black_magic::S<uint64_t, double, int64_t>, black_magic::arguments<6*6+6*3+2>::type>::value, "tag to type container");
}

TEST(response)
{
    ASSERT_EQUAL(100, response(100).code);
    ASSERT_EQUAL(200, response("Hello there").code);
}

int testmain()
{
    bool failed = false;
    for(auto t:tests)
    {
        failed__ = false;
        t->test();
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
