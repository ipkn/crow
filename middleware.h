#pragma once
#include "crow.h"

namespace crow
{

class IMiddlewareHandler;

class Middleware {

private:
	//
	typedef std::vector<std::shared_ptr<IMiddlewareHandler>> HandlersList;
	HandlersList handlers;

public:
	//
	typedef std::function<response(const request&)> RoutedResponseCallback;

	//
	class Context {
		friend class Middleware;
	public:
		typedef std::function<response(const request&, Context&)>  NextCallback;
	private:
		//
		RoutedResponseCallback m_routedResponse;
		NextCallback m_next;
		const request& m_req;
		HandlersList::iterator m_handlersIter;
		//
		Context(RoutedResponseCallback fetchRoutedResponse, NextCallback nextCallback, const request& req, HandlersList::iterator iter);
		response callRoutedResponse();

	public:
		//
		response next();
	};


private:
	//
	response goToNextHandler(const request& req, Context& c);

public:
	//
	void use(std::shared_ptr<IMiddlewareHandler> middlewareObj);
	int count();
	response processHandlers(const request& req, RoutedResponseCallback fetchRoutedResponse);

};

class IMiddlewareHandler {
	public:
		virtual response handle(const request& req, Middleware::Context* c) = 0;
};

/////////////

Middleware::Context::Context(Middleware::RoutedResponseCallback fetchRoutedResponse, NextCallback nextCallback, const request& req, HandlersList::iterator iter)
: m_routedResponse(fetchRoutedResponse)
, m_next(nextCallback)
, m_req(req)
, m_handlersIter(iter)
{
	
}

response Middleware::Context::next()
{
	return m_next(m_req, *this);
}

response Middleware::Context::callRoutedResponse()
{
	return m_routedResponse(m_req);
}

////////

void Middleware::use(std::shared_ptr<IMiddlewareHandler> middlewareObj){
	handlers.push_back(middlewareObj);
}

int Middleware::count() {
	return (int)handlers.size(); 
}

response Middleware::goToNextHandler(const request& req, Middleware::Context& c) {
	if(c.m_handlersIter != handlers.end()){
		auto iter = c.m_handlersIter;
		c.m_handlersIter++;
		return (*iter)->handle(req, &c);
	}

	return c.callRoutedResponse();
}

response Middleware::processHandlers(const request& req, Middleware::RoutedResponseCallback fetchRoutedResponse) {
	Context c(fetchRoutedResponse, std::bind(std::mem_fn(&Middleware::goToNextHandler), this,  placeholders::_1,  placeholders::_2), req, handlers.begin());
	return goToNextHandler(req, c);
}

class LambdaMiddlewareHandler : public IMiddlewareHandler {
	public:
		typedef std::function<response(const request&, Middleware::Context*)> LambbdaMiddlewareFunc;
	private:
		LambbdaMiddlewareFunc m_func;
	public:
		LambdaMiddlewareHandler(LambbdaMiddlewareFunc func) : m_func(func) {}
		response handle(const request& req, Middleware::Context* c) {
			return m_func(req, c);
		}
};


}
