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

typedef std::function<void(const request& req, response& res)> MiddlewareHandlerFunc;

class LambdaMiddlewareHandler : public IMiddleware {
		
	private:
		MiddlewareHandlerFunc m_before;
		MiddlewareHandlerFunc m_after;
	public:
		LambdaMiddlewareHandler(MiddlewareHandlerFunc before, MiddlewareHandlerFunc after) : m_before(before), m_after(after) {}
		void before_handle(const request& req, response& res) {
			if(m_before != nullptr)
				m_before(req, res);
		}
		void after_handle(const request& req, response& res) {
			if(m_after != nullptr)
				m_after(req, res);
		}
};


}
