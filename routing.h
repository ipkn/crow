#pragma once

#include <cstdint>
#include <utility>
#include <string>
#include <tuple>

#include "http_response.h"

namespace flask
{
    class Rule
    {
    public:
        explicit Rule(std::string&& rule)
            : rule_(std::move(rule))
        {
        }
        
        template <typename Func>
        void operator()(Func&& f)
        {
            handler_ = [f = std::move(f)]{
                return response(f());
            };
        }

        template <typename Func>
        void operator()(std::string&& name, Func&& f)
        {
            name_ = std::move(name);
            handler_ = [f = std::move(f)]{
                return response(f());
            };
        }

        bool match(const request& req)
        {
            // FIXME need url parsing
            return req.url == rule_;
        }

        Rule& name(const std::string& name)
        {
            name_ = name;
            return *this;
        }
        void validate()
        {
            if (!handler_)
            {
                throw std::runtime_error("no handler for url " + rule_);
            }
        }

        response handle(const request&)
        {
            return handler_();
        }

    private:
        std::string rule_;
        std::string name_;
        std::function<response()> handler_;
    };

    class Router
    {
    public:
        Rule& new_rule(std::string&& rule)
        {
            rules_.emplace_back(std::move(rule));
            return rules_.back();
        }

        void validate()
        {
            for(auto& rule:rules_)
            {
                rule.validate();
            }
        }

        response handle(const request& req)
        {
            for(auto& rule : rules_)
            {
                if (rule.match(req))
                {
                    return rule.handle(req);
                }
            }
            return response(404);
        }
    private:
        std::vector<Rule> rules_;
    };
}
