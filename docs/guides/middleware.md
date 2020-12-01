Any middleware requires following 3 members:
##struct context
Storing data for the middleware; can be read from another middleware or handlers


##before_handle
Called before handling the request.<br>
If `res.end()` is called, the operation is halted. (`after_handle` will still be called)<br>
2 signatures:<br>
`#!cpp void before_handle(request& req, response& res, context& ctx)`
    if you only need to access this middleware's context.
``` cpp
template <typename AllContext>
void before_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
```
You can access other middlewares' context by calling `#!cpp all_ctx.template get<MW>()`<br>
`#!cpp ctx == all_ctx.template get<CurrentMiddleware>()`


##after_handle
Called after handling the request.<br>

`#!cpp void after_handle(request& req, response& res, context& ctx)`
``` cpp
template <typename AllContext>
void after_handle(request& req, response& res, context& ctx, AllContext& all_ctx)
```
<br><br>
This was pulled from `cookie_parser.h`. Further Editing required, possibly use parts of [@ipkn's wiki page](https://github.com/ipkn/crow/wiki/Middleware).