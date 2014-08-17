#pragma once
#include "http_request.h"
#include "http_response.h"

namespace crow
{

class IMiddleware {
	public:
		virtual void before_handle(const request& req, response& res) = 0;
		virtual void after_handle(const request& req, response& res) = 0;
};

class Middleware {

private:
	//
	typedef std::vector<IMiddleware*> HandlersList;
	HandlersList handlers;

public:
	//
	void use(IMiddleware* middlewareObj)
	{
		handlers.push_back(middlewareObj);
	}

	int count()
	{
		return (int)handlers.size(); 
	}

	unsigned int processBeforeHandlers(const request& req, response& res) 
	{
		unsigned int depth = 0;
		for(auto i : handlers)
		{
			++depth;
			i->before_handle(req, res);
			if(res.is_completed())
			{
				break;
			}		
		}
		return depth;
	}	

	void processAfterHandlers(const request& req, response& res, unsigned int depth = 0)
	{
		auto i = handlers.rbegin();
		if (depth > 0){
			i += (handlers.size() - depth);
		}
		for(; i != handlers.rend(); ++i)
		{
			(*i)->after_handle(req, res);
		}
	}	
};

/////////////

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
