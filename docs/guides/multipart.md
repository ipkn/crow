Multipart is a way of forming HTTP requests or responses to contain multiple distinct parts.<br>
Such an approach allows a request to contain multiple different pieces of data with potentially conflicting data types in a single response payload.<br>
It is typically used either in html forms, or when uploading multiple files.<br><br>

The structure of a multipart request is typically consistent of:<br>

- A Header: Typically `multipart/form-data;boundary=<boundary>`, This defines the HTTP message as being multipart, as well as defining the separator used to distinguish the different parts.<br>
- 1 or more parts:
    - `--<boundary>`
    - Part header: typically `content-disposition: mime/type; name="<fieldname>"` (`mime/type` should be replaced with the actual mime-type), can also contain a `filename` property (separated from the rest by a `;` and structured similarly to the `name` property)
    - Value
- `--<boundary>--`<br><br>

Crow supports multipart requests and responses though `crow::multipart::message`.<br>
A message can be created either by defining the headers, boundary, and individual parts and using them to create the message. or simply by reading a `crow::request`.<br><br>

Once a multipart message has been made, the individual parts can be accessed throught `mpmes.parts`, `parts` is an `std::vector`, so accessing the individual parts should be straightforward.<br>
In order to access the individual part's name or filename, something like `#!cpp mpmes.parts[0].headers[0].params["name"]` sould do the trick.<br><br>

For more info on Multipart messages, go [here](../../reference/namespacecrow_1_1multipart.html)
