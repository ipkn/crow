#pragma once
#include <string>
#include <vector>
#include "json.h"
namespace crow
{
    namespace mustache
    {
        using context = json::wvalue;

        class invalid_template_exception : public std::exception
        {
            public:
            invalid_template_exception(const std::string& msg)
                : msg("crow::mustache error: " + msg)
            {
            }
            virtual const char* what() const throw()
            {
                return msg.c_str();
            }
            std::string msg;
        };

        enum class ActionType
        {
            Ignore,
            Tag,
            UnescapeTag,
            OpenBlock,
            CloseBlock,
            ElseBlock,
            Partial,
        };

        struct Action
        {
            int start;
            int end;
            int pos;
            ActionType t;
            Action(ActionType t, int start, int end, int pos = 0) 
                : start(start), end(end), pos(pos), t(t)
            {}
        };

        class template_t 
        {
        public:
            template_t(std::string body)
                : body_(std::move(body))
            {
                // {{ {{# {{/ {{^ {{! {{> {{=
                parse();
            }

            std::string render(context& ctx)
            {
				std::vector<context*> stack;
				stack.emplace_back(&ctx);
				auto tag_name = [&](const Action& action)
				{
					return body_.substr(action.start, action.end - action.start);
				};
				auto find_context = [&](const std::string& name)->std::pair<bool, context&>
				{
					for(auto it = stack.rbegin(); it != stack.rend(); ++it)
					{
						std::cerr << "finding " << name << " on " << (int)(*it)->t() << std::endl;
						if ((*it)->t() == json::type::Object)
						{
							for(auto jt = (*it)->o->begin(); jt != (*it)->o->end(); ++jt)
							{
								std::cerr << '\t' << jt->first << ' ' << json::dump(jt->second) << std::endl;
							}
							if ((*it)->count(name))
								return {true, (**it)[name]};
						}
					}
					
					static json::wvalue empty_str;
					empty_str = "";
					return {false, empty_str};
				};
                int current = 0;
                std::string ret;
                while(current < fragments_.size())
                {
					auto& fragment = fragments_[current];
					auto& action = actions_[current];
                    ret += body_.substr(fragment.first, fragment.second-fragment.first);
                    switch(action.t)
                    {
					case ActionType::Ignore:
						// do nothing
						break;
					case ActionType::Tag:
						{
							auto optional_ctx = find_context(tag_name(action));
							auto& ctx = optional_ctx.second;
							switch(ctx.t())
							{
								case json::type::Number:
									ret += json::dump(ctx);
									break;
								case json::type::String:
									ret += ctx.s;
									break;
								default:
									throw std::runtime_error("not implemented tag type" + boost::lexical_cast<std::string>((int)ctx.t()));
							}
						}
						break;
					case ActionType::OpenBlock:
						{
							std::cerr << tag_name(action) << std::endl;
							auto optional_ctx = find_context(tag_name(action));
							std::cerr << optional_ctx.first << std::endl;
							if (!optional_ctx.first)
								current = action.pos;
							auto& ctx = optional_ctx.second;
							if (ctx.t() == json::type::Null || ctx.t() == json::type::False)
								current = action.pos;
							stack.push_back(&ctx);
							break;
						}
					case ActionType::CloseBlock:
						stack.pop_back();
						break;
					default:
						throw std::runtime_error("not implemented " + boost::lexical_cast<std::string>((int)action.t));
                    }
					current++;
                }
                return ret;
            }

        private:

            void parse()
            {
                std::string tag_open = "{{";
                std::string tag_close = "}}";

                std::vector<int> blockPositions;
                
                size_t current = 0;
                while(1)
                {
                    size_t idx = body_.find(tag_open, current);
                    if (idx == body_.npos)
                    {
                        fragments_.emplace_back(current, body_.size());
                        actions_.emplace_back(ActionType::Ignore, 0, 0);
                        break;
                    }
                    fragments_.emplace_back(current, idx);

                    idx += tag_open.size();
                    size_t endIdx = body_.find(tag_close, idx);
                    if (endIdx == idx)
                    {
                        throw invalid_template_exception("empty tag is not allowed");
                    }
                    if (endIdx == body_.npos)
                    {
                        // error, no matching tag
                        throw invalid_template_exception("not matched opening tag");
                    }
                    current = endIdx + tag_close.size();
                    switch(body_[idx])
                    {
                        case '#':
                            idx++;
                            blockPositions.emplace_back(actions_.size());
                            actions_.emplace_back(ActionType::OpenBlock, idx, endIdx);
                            break;
                        case '/':
                            idx++;
                            {
                                auto& matched = actions_[blockPositions.back()];
                                if (body_.compare(idx, endIdx-idx, 
                                        body_, matched.start, matched.end - matched.start) != 0)
                                {
                                    throw invalid_template_exception("not matched {{# {{/ pair: " + 
                                        body_.substr(matched.start, matched.end - matched.start) + ", " + 
                                        body_.substr(idx, endIdx-idx));
                                }
								matched.pos = actions_.size();
                            }
                            actions_.emplace_back(ActionType::CloseBlock, idx, endIdx, blockPositions.back());
                            blockPositions.pop_back();
                            break;
                        case '^':
                            blockPositions.emplace_back(actions_.size());
                            actions_.emplace_back(ActionType::ElseBlock, idx+1, endIdx);
                            break;
                        case '!':
                            // do nothing action
                            actions_.emplace_back(ActionType::Ignore, idx+1, endIdx);
                            break;
                        case '>': // partial
                            actions_.emplace_back(ActionType::Partial, idx+1, endIdx);
                            throw invalid_template_exception("{{>: partial not implemented: " + body_.substr(idx+1, endIdx-idx-1));
                            break;
                        case '{':
                            if (tag_open != "{{" || tag_close != "}}")
                                throw invalid_template_exception("cannot use triple mustache when delimiter changed");

                            idx ++;
                            if (body_[endIdx+2] != '}')
                            {
                                throw invalid_template_exception("{{{: }}} not matched");
                            }
                            actions_.emplace_back(ActionType::UnescapeTag, idx, endIdx);
                            current++;
                            break;
                        case '&':
                            idx ++;
                            actions_.emplace_back(ActionType::UnescapeTag, idx, endIdx);
                            break;
                        case '=':
                            // tag itself is no-op
                            idx ++;
                            actions_.emplace_back(ActionType::Ignore, idx, endIdx);
                            endIdx --;
                            if (body_[endIdx] != '=')
                                throw invalid_template_exception("{{=: not matching = tag: "+body_.substr(idx, endIdx-idx));
                            endIdx --;
                            while(body_[idx] == ' ') idx++;
                            while(body_[endIdx] == ' ') endIdx--;
                            endIdx++;
                            {
                                bool succeeded = false;
                                for(size_t i = idx; i < endIdx; i++)
                                {
                                    if (body_[i] == ' ')
                                    {
                                        tag_open = body_.substr(idx, i-idx);
                                        while(body_[i] == ' ') i++;
                                        tag_close = body_.substr(i, endIdx-i);
                                        if (tag_open.empty())
                                            throw invalid_template_exception("{{=: empty open tag");
                                        if (tag_close.empty())
                                            throw invalid_template_exception("{{=: empty close tag");

                                        if (tag_close.find(" ") != tag_close.npos)
                                            throw invalid_template_exception("{{=: invalid open/close tag: "+tag_open+" " + tag_close);
                                        succeeded = true;
                                        break;
                                    }
                                }
                                if (!succeeded)
                                    throw invalid_template_exception("{{=: cannot find space between new open/close tags");
                            }
                            break;
                        default:
                            // normal tag case;
                            actions_.emplace_back(ActionType::Tag, idx, endIdx);
                            break;
                    }
                }
            }
            
            std::vector<std::pair<int,int>> fragments_;
            std::vector<Action> actions_;
            std::string body_;
        };

        template_t compile(const std::string& body)
        {
            return template_t(body);
        }
    }
}
