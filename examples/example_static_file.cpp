//#define CROW_STATIC_DRIECTORY "alternative_directory/"
//#define CROW_STATIC_ENDPOINT "/alternative_endpoint/<path>"
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

/// You can also use the `/static` directory and endpoint (the directory needs to have the same path as your executable).
/// Any file inside the static directory will have the same path it would in your filesystem.

/// TO change the static directory or endpoint, use the macros above (replace `alternative_directory` and/or `alternative_endpoint` with your own)
