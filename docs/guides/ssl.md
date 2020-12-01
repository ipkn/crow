Crow supports HTTPS though SSL or TLS.<br><br>

When mentioning SSL in this documentation, it is often a reference to openSSL, which includes TLS. Don't worry, we don't use obsolete security standards :)<br><br>

To enable SSL, first your application needs to define either a `.crt` and `.key` files, or a `.pem` file. Once you have your files, you can add them to your app like this:<br>
`#!cpp app.ssl_file("/path/to/cert.crt", "/path/to/keyfile.key")` or `#!cpp app.ssl_file("/path/to/pem_file.pem")`. Please note that this method can be part of the app method chain, which means it can be followed by `.run()` or any other method.<br><br>

You can also set your own SSL context (by using `boost::asio::ssl::context ctx`) and then applying it via the `#!cpp app.ssl(ctx)` method.<br><br>

**IMPORTANT NOTICE**: If you plan on using a proxy like Nginx or Apache2, **DO NOT** use SSL in crow, instead define it in your proxy instead and keep the connection between the proxy and Crow non-SSL.