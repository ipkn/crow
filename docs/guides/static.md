A static file is any file that resides in the server's storage.

Crow supports returning Static files as responses in 2 ways.

##Implicit
Crow implicitly returns any static files placed in a `static` directory and any subdirectories, as long as the user calls the endpoint `/static/path/to/file`.<br><br>
The static folder or endpoint can be changed by defining the macros `CROW_STATIC_DRIECTORY "alternative_directory/"` and `CROW_STATIC_ENDPOINT "/alternative_endpoint/<path>"`.<br>
static directory changes the directory in the server's filesystem, while the endpoint changes the URL that the client needs to access.

##Explicit
You can directly return a static file by using the `crow::response` method `#!cpp response.set_static_file_info("path/to/file");`. The path is relative to the executable unless preceded by `/`, then it is an absolute path.<br>
Please keep in mind that using the `set_static_file_info` method does invalidate any data already in your response body.<br><br>

**Note**: Crow sets the `content-type` header automatically based on the file's extension, if an extension is unavailable or undefined,  Crow uses `text/plain`, if you'd like to explicitly set a `content-type`, use `#!cpp response.set_header("content-type", "mime/type");` **AFTER** calling `set_static_file_info`.