#pragma once
#include "crow.h"

namespace crow
{

class IMiddlewareHandler;

class middleware {

public:
	//
	typedef std::function<response(const request&)> RoutedResponseCallback;

	//
	class context {
		friend class middleware;
	public:
		typedef std::function<response(const request&, context&)>  NextCallback;
	private:
		//
		RoutedResponseCallback m_routedResponse;
		NextCallback m_next;
		const request& m_req;
		//
		context(RoutedResponseCallback fetchRoutedResponse, NextCallback nextCallback, const request& req);
		response callRoutedResponse();

	public:
		//
		response next();
	};


private:
	//
	typedef std::vector<std::shared_ptr<IMiddlewareHandler>> HandlersList;
	static HandlersList handlers;
	static HandlersList::iterator handlersIter;

	//
	static response goToNextHandler(const request& req, context& c);

public:
	//
	static void use(std::shared_ptr<IMiddlewareHandler> middlewareObj);
	static int count();
	static response processHandlers(const request& req, RoutedResponseCallback fetchRoutedResponse);

};

class IMiddlewareHandler {
	public:
		virtual response handle(const request& req, middleware::context* c) = 0;
};

middleware::HandlersList middleware::handlers;
middleware::HandlersList::iterator middleware::handlersIter;

/////////////

middleware::context::context(middleware::RoutedResponseCallback fetchRoutedResponse, NextCallback nextCallback, const request& req)
: m_routedResponse(fetchRoutedResponse)
, m_next(nextCallback)
, m_req(req)
{
	
}

response middleware::context::next()
{
	return m_next(m_req, *this);
}

response middleware::context::callRoutedResponse()
{
	return m_routedResponse(m_req);
}

////////

void middleware::use(std::shared_ptr<IMiddlewareHandler> middlewareObj){
	handlers.push_back(middlewareObj);
}

int middleware::count() {
	return (int)handlers.size(); 
}

response middleware::goToNextHandler(const request& req, middleware::context& c) {
	if(handlersIter != handlers.end()){
		auto iter = handlersIter;
		handlersIter++;
		return (*iter)->handle(req, &c);
	}

	return c.callRoutedResponse();
}

response middleware::processHandlers(const request& req, middleware::RoutedResponseCallback fetchRoutedResponse) {
	handlersIter = handlers.begin();
	context c(fetchRoutedResponse, goToNextHandler, req);
	return goToNextHandler(req, c);
}

}
