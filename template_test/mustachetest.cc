#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include "../mustache.h"
#include "../json.h"
using namespace std;
using namespace crow;
using namespace crow::mustache;

string read_all(const string& filename)
{
    ifstream is(filename);
    string ret;
    copy(istreambuf_iterator<char>(is), istreambuf_iterator<char>(), back_inserter(ret));
    return ret;
}

int main()
{
    auto data = json::load(read_all("data")); 
    auto templ = compile(read_all("template"));
    context ctx(data);
    cout << templ.render(ctx);
    return 0;
}
