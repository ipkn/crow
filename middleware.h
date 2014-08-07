#pragma once
#include "crow.h"

namespace crow
{

class IMiddleware;

class Middleware {

private:
	//
	typedef std::vector<std::shared_ptr<IMiddleware>> HandlersList;
	HandlersList handlers;

public:
	//
	void use(std::shared_ptr<IMiddleware> middlewareObj);
	int count();
	void processBeforeHandlers(const request& req, response& res);
	void processAfterHandlers(const request& req, response& res);
};

class IMiddleware {
	public:
		virtual void before_handle(const request& req, response& res) = 0;
		virtual void after_handle(const request& req, response& res) = 0;
};

/////////////


void Middleware::use(std::shared_ptr<IMiddleware> middlewareObj){
	handlers.push_back(middlewareObj);
}

int Middleware::count() {
	return (int)handlers.size(); 
}

void Middleware::processBeforeHandlers(const request& req, response& res) {
	for(auto i : handlers)
	{
		i->before_handle(req, res);
		if(res.isCompleted())
		{
			break;
		}
	}
}

void Middleware::processAfterHandlers(const request& req, response& res) {
	for(auto i = handlers.rbegin(); i != handlers.rend(); ++i)
	{
		(*i)->after_handle(req, res);
	}
}

/*class LambdaMiddlewareHandler : public IMiddleware {
	public:
		typedef std::function<response(const request&, Middleware::Context*)> LambbdaMiddlewareFunc;
	private:
		LambbdaMiddlewareFunc m_func;
	public:
		LambdaMiddlewareHandler(LambbdaMiddlewareFunc func) : m_func(func) {}
		response handle(const request& req, Middleware::Context* c) {
			return m_func(req, c);
		}
};*/


}
