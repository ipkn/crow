#include "crow.h"
#include "mustache.h"
#include "middleware.h"
#include <sstream>
using namespace crow;
int main() {
  App<Cors> app;
  app.set_directory("./static");
  //Server rendering
  CROW_ROUTE(app,"/")([] {
	char name[64];gethostname(name,64);
	mustache::Ctx x;x["servername"]=name;
	auto page=mustache::load("index.html");
	return page.render(x);
  });
  //Single path access to files
  app.route_dynamic("/cat")([](const Req&,Res& res) {
	res.set_static_file_info("1.jpg");res.end();
  });
  // a request to /path should be forwarded to /path/
  app.route_dynamic("/path/")([]() {
	return "Trailing slash test case..";
  });
  app.route_dynamic("/json")([] {
	json::value x;
	x["message"]="Hello, World!";
	return x;
  });
  app.route_dynamic("/hello/<int>")([](int count) {
	if (count>100)
	  return Res(400);
	std::ostringstream os;
	os<<count<<" bottles of beer!";
	return Res(os.str());
  });
  app.route_dynamic("/add/<int>/<int>")([](const Req& req,Res& res,int a,int b) {
	std::ostringstream os;
	os<<a+b;
	res.write(os.str());
	res.end();
  });
  // Compile error with message "Handler type is mismatched with URL paramters"
  //CROW_ROUTE(app,"/another/<int>")([](int a, int b){
	  //return response(500);
  //});
  // more json example
  app.route_dynamic("/add_json").methods(HTTPMethod::POST)([](const Req& req) {
	auto x=json::parse(req.body);
	if (!x) return Res(400);
	auto sum=x["a"].i()+x["b"].i();
	std::ostringstream os; os<<sum;
	return Res{os.str()};
  });
  app.route_dynamic("/params")([](const Req& req) {
	std::ostringstream os;
	os<<"Params: "<<req.url_params<<"\n\n";
	os<<"The key 'foo' was "<<(req.url_params.get("foo")==nullptr?"not ":"")<<"found.\n";
	if (req.url_params.get("pew")!=nullptr) {
	  double countD=boost::lexical_cast<double>(req.url_params.get("pew"));
	  os<<"The value of 'pew' is "<<countD<<'\n';
	}
	auto count=req.url_params.get_list("count");
	os<<"The key 'count' contains "<<count.size()<<" value(s).\n";
	for (const auto& countVal:count) os<<" - "<<countVal<<'\n';
	return Res{os.str()};
  });
  logger::setLogLevel(LogLevel::WARNING);
  //logger::setHandler(std::make_shared<ExampleLogHandler>());
  app.port(8080).multithreaded().run();
}