#pragma once

#include <cstdint>
#include <utility>
#include <string>
#include <tuple>
#include <unordered_map>
#include <memory>
#include <boost/lexical_cast.hpp>

#include "http_response.h"

//TEST
#include <iostream>

namespace flask
{
    class BaseRule
    {
    public:
        BaseRule(std::string rule)
            : rule_(std::move(rule))
        {
        }

        virtual ~BaseRule()
        {
        }
        
        BaseRule& name(std::string name)
        {
            name_ = std::move(name);
            return *this;
        }

        virtual void validate()
        {
        }
        virtual response handle(const request&)
        {
            return response(400);
        }

    protected:
        std::string rule_;
        std::string name_;
    };

    class Rule : public BaseRule
    {
    public:
        Rule(std::string rule)
            : BaseRule(std::move(rule))
        {
        }
        
        Rule& name(std::string name)
        {
            name_ = std::move(name);
            return *this;
        }

        template <typename Func>
        void operator()(Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<>>::value, 
                "Handler type is mismatched with URL paramters");
            handler_ = [f = std::move(f)]{
                return response(f());
            };
        }

        template <typename Func>
        void operator()(std::string name, Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<>>::value, 
                "Handler type is mismatched with URL paramters");
            name_ = std::move(name);
            handler_ = [f = std::move(f)]{
                return response(f());
            };
        }

        void validate()
        {
            if (!handler_)
            {
                throw std::runtime_error(name_ + (!name_.empty() ? ": " : "") + "no handler for url " + rule_);
            }
        }

        response handle(const request&)
        {
            return handler_();
        }

    protected:
        std::function<response()> handler_;
    };

    template <typename ... Args>
    class TaggedRule : public BaseRule
    {
    public:
        TaggedRule(std::string rule)
            : BaseRule(std::move(rule))
        {
        }
        
        TaggedRule<Args...>& name(std::string name)
        {
            name_ = std::move(name);
            return *this;
        }

        template <typename Func>
        void operator()(Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<Args...>>::value, 
                "Handler type is mismatched with URL paramters");
            handler_ = [f = std::move(f)](Args ... args){
                return response(f(args...));
            };
        }

        template <typename Func>
        void operator()(std::string name, Func&& f)
        {
            static_assert(black_magic::CallHelper<Func, black_magic::S<Args...>>::value, 
                "Handler type is mismatched with URL paramters");
            name_ = std::move(name);
            handler_ = [f = std::move(f)](Args ... args){
                return response(f(args...));
            };
        }

        response handle(const request&)
        {
            //return handler_();
            return response(500);
        }

    private:
        std::function<response(Args...)> handler_;
    };

    constexpr const size_t INVALID_RULE_INDEX{static_cast<size_t>(-1)};
    class Trie
    {
    public:
        struct Node
        {
            std::unordered_map<std::string, size_t> children;
            size_t rule_index{INVALID_RULE_INDEX};
            size_t child_for_int{INVALID_RULE_INDEX};
            size_t child_for_uint{INVALID_RULE_INDEX};
            size_t child_for_float{INVALID_RULE_INDEX};
            size_t child_for_str{INVALID_RULE_INDEX};
            size_t child_for_path{INVALID_RULE_INDEX};
            bool IsSimpleNode() const
            {
                return 
                    rule_index == INVALID_RULE_INDEX &&
                    child_for_int == INVALID_RULE_INDEX &&
                    child_for_uint == INVALID_RULE_INDEX &&
                    child_for_float == INVALID_RULE_INDEX &&
                    child_for_str == INVALID_RULE_INDEX &&
                    child_for_path == INVALID_RULE_INDEX;
            }
        };

        Trie() : nodes_(1)
        {
        }

private:
        void optimizeNode(Node* node)
        {
            for(auto& kv : node->children)
            {
                Node* child = &nodes_[kv.second];
                optimizeNode(child);
            }
        }

public:
        void optimize()
        {
            optimizeNode(head());
        }

        size_t find(const request& req, const Node* node = nullptr, size_t pos = 0) const
        {
            size_t found = INVALID_RULE_INDEX;

            if (node == nullptr)
                node = head();
            if (pos == req.url.size())
                return node->rule_index;

            for(auto& kv : node->children)
            {
                const std::string& fragment = kv.first;
                const Node* child = &nodes_[kv.second];

                if (req.url.compare(pos, fragment.size(), fragment) == 0)
                {
                    size_t ret = find(req, child, pos + fragment.size());
                    if (ret != INVALID_RULE_INDEX && (found == INVALID_RULE_INDEX || found > ret))
                    {
                        found = ret;
                    }
                }
            }

            return found;
        }

        void add(const std::string& url, size_t rule_index)
        {
            size_t idx{0};

            for(size_t i = 0; i < url.size(); i ++)
            {
                char c = url[i];
                if (c == '<')
                {
                    if (url.compare(i, 5, "<int>") == 0)
                    {
                        idx = nodes_[idx].child_for_int = new_node();
                        i += 5;
                    }
                    else if (url.compare(i, 6, "<uint>") == 0)
                    {
                        idx = nodes_[idx].child_for_uint = new_node();
                        i += 6;
                    }
                    else if (url.compare(i, 7, "<float>") == 0 || 
                        url.compare(i, 8, "<double>") == 0)
                    {
                        idx = nodes_[idx].child_for_float = new_node();
                        i += 7;
                    }
                    else if (url.compare(i, 5, "<str>") == 0)
                    {
                        idx = nodes_[idx].child_for_str = new_node();
                        i += 5;
                    }
                    else if (url.compare(i, 6, "<path>") == 0)
                    {
                        idx = nodes_[idx].child_for_path = new_node();
                        i += 6;
                    }
                    else
                    {
                        throw std::runtime_error("Invalid url: " + url + " (" + boost::lexical_cast<std::string>(i) + ")");
                    }
                    i --;
                }
                else
                {
                    std::string piece(&c, 1);
                    if (!nodes_[idx].children.count(piece))
                    {
                        nodes_[idx].children.emplace(piece, new_node());
                    }
                    idx = nodes_[idx].children[piece];
                }
            }
            if (nodes_[idx].rule_index != INVALID_RULE_INDEX)
                throw std::runtime_error("handler already exists for " + url);
            nodes_[idx].rule_index = rule_index;
        }
    private:
        const Node* head() const
        {
            return &nodes_.front();
        }

        Node* head()
        {
            return &nodes_.front();
        }

        size_t new_node()
        {
            nodes_.resize(nodes_.size()+1);
            return nodes_.size() - 1;
        }

        std::vector<Node> nodes_;
    };

    class Router
    {
    public:
        template <uint64_t N>
        typename black_magic::arguments<N>::type::template rebind<TaggedRule>& new_rule_tagged(const std::string& rule)
        {
            using RuleT = typename black_magic::arguments<N>::type::template rebind<TaggedRule>;
            auto ruleObject = new RuleT(rule);
            rules_.emplace_back(ruleObject);
            trie_.add(rule, rules_.size() - 1);
            return *ruleObject;
        }

        Rule& new_rule(const std::string& rule)
        {
            Rule* r(new Rule(rule));
            rules_.emplace_back(r);
            trie_.add(rule, rules_.size() - 1);
            return *r;
        }

        void validate()
        {
            trie_.optimize();
            for(auto& rule:rules_)
            {
                rule->validate();
            }
        }

        response handle(const request& req)
        {
            size_t rule_index = trie_.find(req);

            if (rule_index == INVALID_RULE_INDEX)
                return response(404);

            if (rule_index >= rules_.size())
                throw std::runtime_error("Trie internal structure corrupted!");

            return rules_[rule_index]->handle(req);
        }
    private:
        std::vector<std::unique_ptr<BaseRule>> rules_;
        Trie trie_;
    };
}
