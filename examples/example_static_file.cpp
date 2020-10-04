#include "crow.h"

int main()
{

	//Crow app initialization
	crow::SimpleApp app;

    //
CROW_ROUTE(app, "/")
([](const crow::request&, crow::response& res) {
    //replace cat.jpg with your file path
    res.set_static_file_info("cat.jpg");
    res.end();
});

	
	app.port(18080).run();
	
	
	return 0;
}
