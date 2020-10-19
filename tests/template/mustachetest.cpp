#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "crow/json.h"
#include "crow/mustache.h"

using namespace std;
using namespace crow;
using namespace crow::mustache;

string read_all(const string& filename)
{
  ifstream is(filename);
  return {istreambuf_iterator<char>(is), istreambuf_iterator<char>()};
}

int main()
{
  auto data = json::load(read_all("data"));
  auto templ = compile(read_all("template"));
  auto partials = json::load(read_all("partials"));
  set_loader([&](std::string name) -> std::string {
    if (partials.count(name)) {
      return partials[name].s();
    }
    return "";
  });
  context ctx(data);
  cout << templ.render(ctx);
  return 0;
}
