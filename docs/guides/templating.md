Templating is when you return an html page with custom data. You can probably tell why that's useful.<br><br>

Crow supports [mustache](http://mustache.github.io) for templates through its own implementation `crow::mustache`.<br><br>

##Components of mustache

There are 2 components of a mustache template implementation:

- Page
- Context

###Page
The HTML page (including the mustache tags). It is usually loaded into `crow::mustache::template_t`. It needs to be placed in `templates` directory (relative to where the crow executable is).<br><br>

For more information on how to formulate a template, see [this mustache manual](http://mustache.github.io/mustache.5.html).

###Context
A JSON object containing the tags as keys and their values. `crow::mustache::context` is actually a [crow::json::wvalue](../json#wvalue).

##Returning a template
To return a mustache template, you need to load a page using `#!cpp auto page = crow::mustache::load("path/to/template.html");`, keep in mind that the path is relative to the templates directory.<br>
You also need to set up the context by using `#!cpp crow::mustache::context ctx;`. Then you need to assign the keys and values, this can be done the same way you assign values to a json write value (`ctx["key"] = value;`).<br>
With your context and page ready, just `#!cpp return page.render(ctx);`. This will use the context data to return a filled template.<br>
Alternatively you could just render the page without a context using `#!cpp return page.render();`.
