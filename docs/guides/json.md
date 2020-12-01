Crow has built in support for JSON data.<br><br>

##rvalue
JSON read value, used for taking a JSON string and parsing it into `crow::json`.<br><br>

You can read individual items of the rvalue, but you cannot add items to it.<br>
To do that, you need to convert it to a `wvalue`, which can be done by simply writing `#!cpp crow::json::wvalue wval (rval);` (assuming `rval` is your `rvalue`).<br><br>

For more info on read values go [here](/reference/classcrow_1_1json_1_1rvalue.html).<br><br>

#wvalue
JSON write value, used for creating, editing and converting JSON to a string.<br><br>

The types of values that `wvalue` can take are as follows:<br>

- `False`: from type `bool`.
- `True`: from type `bool`.
- `Number`
    - `Floating_point`: from type `double`.
    - `Signed_integer`: from type `int`.
    - `Unsigned_integer`: from type `unsigned int`.
- `String`: from type `std::string`.
- `List`: from type `std::vector`.
- `Object`: from type `crow::json::wvalue`.<br>
This last type means that `wvalue` can have keys, this is done by simply assigning a value to whatever string key you like, something like `#!cpp wval["key1"] = val1;`. Keep in mind that val1 can be any of the above types.<br><br>

A JSON wvalue can be returned directly inside a route handler, this will cause the `content-type` header to automatically be set to `Application/json` and the JSON value will be converted to string and placed in the response body. For more information go to [Routes](../routes).<br><br>

For more info on write values go [here](../../reference/classcrow_1_1json_1_1wvalue.html).
