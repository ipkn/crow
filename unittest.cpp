#include <iostream>
#include <vector>
#include "routing.h"
using namespace std;
using namespace flask;

struct Test { Test(); virtual void test() = 0; };
vector<Test*> tests;
Test::Test() { tests.push_back(this); }

bool failed__ = false;
void fail() { failed__ = true; }

#define TEST(x) struct test##x:public Test{void test();}x##_; \
    void test##x::test()

TEST(Rule)
{
    Rule r("/http/");
    r.name("abc");
    try 
    {
        r.validate();
        fail();
    }
    catch(runtime_error& e)
    {
    }

    int x = 0;

    r([&x]{x = 1;return "";});
    r.validate();
    if (x!=0)
        fail();
    r.handle(request(), routing_params());
    if (x == 0)
        fail();

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
