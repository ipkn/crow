#pragma once
#include <boost/algorithm/string/trim.hpp>
#include "crow/http_request.h"
#include "crow/http_response.h"
namespace crow {
  struct Cors {
	struct Ctx {};
	void before_handle(Req& req,Res& res,Ctx&) {
	  res.set_header("Access-Control-Allow-Origin","*");
	  res.set_header("Access-Control-Allow-Headers","content-type,cache-control,x-requested-with,authorization");
	  res.set_header("Access-Control-Allow-Credentials","true");
	  res.set_header("cache-control","max-age=179,immutable");
	  res.set_header("X-Content-Type-Options","nosniff");
	  if (req.method==HTTPMethod::OPTIONS) { res.code=204;res.end(); }
	}
	void after_handle(Req&,Res&,Ctx&) {}
  };
  struct CookieParser {
	struct Ctx {
	  std::unordered_map<std::string,std::string> jar;
	  std::unordered_map<std::string,std::string> cookies_to_add;
	  std::string get_cookie(const std::string& key) const {
		auto cookie=jar.find(key);
		if (cookie!=jar.end()) return cookie->second;
		return {};
	  }
	  void set_cookie(const std::string& key,const std::string& value) {
		cookies_to_add.emplace(key,value);
	  }
	};
	void before_handle(Req& req,Res& res,Ctx& ctx) {
	  int count=req.headers.count("Cookie");
	  if (!count) return;
	  if (count>1) { res.code=400; res.end(); return; }
	  std::string cookies=req.get_header_value("Cookie");
	  size_t pos=0;
	  while (pos<cookies.size()) {
		size_t pos_equal=cookies.find('=',pos);
		if (pos_equal==cookies.npos) break;
		std::string name=cookies.substr(pos,pos_equal-pos);
		boost::trim(name);
		pos=pos_equal+1;
		while (pos<cookies.size()&&cookies[pos]==' ') pos++;
		if (pos==cookies.size()) break;

		size_t pos_semicolon=cookies.find(';',pos);
		std::string value=cookies.substr(pos,pos_semicolon-pos);

		boost::trim(value);
		if (value[0]=='"'&&value[value.size()-1]=='"') value=value.substr(1,value.size()-2);

		ctx.jar.emplace(std::move(name),std::move(value));
		pos=pos_semicolon;
		if (pos==cookies.npos) break;
		pos++;
		while (pos<cookies.size()&&cookies[pos]==' ') pos++;
	  }
	}

	void after_handle(Req&,Res& res,Ctx& ctx) {
	  for (auto& cookie:ctx.cookies_to_add) {
		if (cookie.second.empty())
		  res.add_header("Set-Cookie",cookie.first+"=\"\"");
		else
		  res.add_header("Set-Cookie",cookie.first+"="+cookie.second);
	  }
	}
  };

  /*
  App<CookieParser, Cors> app;
  A B C
  A::Ctx

  ctx1 : public A::Ctx
  ctx2 : public ctx1, public B::Ctx
  ctx3 : public ctx2, public C::Ctx
  C depends on A
  C::handle
	  Ctx.aaa

  App::Ctx : private CookieParser::contetx, ... {
	  jar
  }
  SimpleApp
  */
  struct ExampleMiddleware {
	std::string message;
	ExampleMiddleware() { message="fastify"; }
	void setMessage(std::string s) { message=s; }
	struct Ctx {};
	void before_handle(Req& req,Res& res,Ctx& ctx) { CROW_LOG_DEBUG<<" - MESSAGE: "<<message; }
	void after_handle(Req& req,Res& res,Ctx& ctx) {}
  };
}
// Any middleware requires following 3 members:
// struct Ctx;
//      storing data for the middleware; can be read from another middleware or handlers
// before_handle
//      called before handling the request.
//      if res.end() is called, the operation is halted. 
//      (still call after_handle of this middleware)
//      2 signatures:
//      void before_handle(Req& req, Res& res, Ctx& ctx)
//          if you only need to access this middlewares Ctx.
//      template <typename AllContext>
//      void before_handle(Req& req, Res& res, Ctx& ctx, AllContext& all_ctx)
//          you can access another middlewares' Ctx by calling `all_ctx.template get<MW>()'
//          ctx == all_ctx.template get<CurrentMiddleware>()

// after_handle
//      called after handling the request.
//      void after_handle(Req& req, Res& res, Ctx& ctx)
//      template <typename AllContext>
//      void after_handle(Req& req, Res& res, Ctx& ctx, AllContext& all_ctx)
