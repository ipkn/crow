#include <crow.h>
int main() {
  crow::SimpleApp app;
  CROW_ROUTE(app,"/")([]() {return "Hello";});
  //Setting a custom route for any URL that isn't defined, instead of a simple 404.
  CROW_CATCHALL_ROUTE(app)
	([](crow::response& res) {
	res.body="The URL does not seem to be correct.";
	res.end();
  });

  app.port(18080).run();
}
