#include "crow.h"
using namespace crow;
int main() {
  SimpleApp app;
  CROW_ROUTE(app,"/")([]() {
	return "Hello world!";
  });
  app.port(8080).run();
}