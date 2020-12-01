Crow comes with a simple and easy to use logging system.<br><br>

##Setting up logging level
You can set up the level at which crow displays logs by using the app's `loglevel(crow::logLevel)` method.<br><br>

The available log levels are as follows (please not that setting a level will also display all logs below this level):

- Debug
- Info
- Warning
- Error
- Critical
<br><br>

To set a logLevel, just use `#!cpp app.loglevel(crow::logLevel::Warning)`, This will not show any debug or info logs. It will however still show error and critical logs.<br><br>

Please note that setting the Macro `CROW_ENABLE_DEBUG` during compilation will also set the log level to `Debug`.

##Writing a log
Writing a log is as simple as `#!cpp CROW_LOG_<LOG LEVEL> << "Hello";` (replace&lt;LOG LEVEL&gt; with the actual level in all caps, so you have `CROW_LOG_WARNING`).