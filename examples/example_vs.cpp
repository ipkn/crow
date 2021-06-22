#include "crow.h"
#include "middleware.h"
#include <sstream>
using namespace crow;
class ExampleLogHandler : public ILogHandler {
  public:void log(std::string message,LogLevel level) override {
	//cerr << "ExampleLogHandler -> " << message;
  }
};
int main() {
  //This is the default and can be omitted, just for demonstration
  mustache::set_directory("./static");
  App<ExampleMiddleware,Cors> app;
  app.get_middleware<ExampleMiddleware>().setMessage("hello");
  //SSR server rendering
  CROW_ROUTE(app,"/")([] {
	char name[256];gethostname(name,256);
	mustache::Ctx x;x["servername"]=name;
	auto page=mustache::load("index.html");
	return page.render(x);
  });
  app.route_dynamic("/fav")([](const Req&,Res& res) {
	res.set_static_file_info("favicon.ico");res.end();
  });
  app.route_dynamic("/cat")([](const Req&,Res& res) {
	res.set_static_file_info("cat.jpg");res.end();
  });
  app.route_dynamic("/about")([]() {
	return "About Crow example.";
  });
  // a request to /path should be forwarded to /path/
  app.route_dynamic("/path/")([]() {
	return "Trailing slash test case..";
  });
  app.route_dynamic("/json")([] {
	json::wvalue x;
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
	auto x=json::load(req.body);
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
  logger::setLogLevel(LogLevel::INFO);
  //logger::setHandler(std::make_shared<ExampleLogHandler>());
  app.port(8080).multithreaded().run();
}