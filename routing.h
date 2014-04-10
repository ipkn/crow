#pragma once

#include <cstdint>
#include <utility>
#include <string>
#include <tuple>
#include <unordered_map>

#include "http_response.h"

//TEST
#include <iostream>

namespace flask
{
    class Rule
    {
    public:
        explicit Rule(std::string rule)
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
        void operator()(std::string name, Func&& f)
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

        Rule& name(std::string name)
        {
            name_ = std::move(name);
            return *this;
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

    private:
        std::string rule_;
        std::string name_;
        std::function<response()> handler_;
    };

    constexpr const size_t INVALID_RULE_INDEX{static_cast<size_t>(-1)};
    class Trie
    {
    public:
        struct Node
        {
            std::unordered_map<std::string, size_t> children;
            size_t rule_index{INVALID_RULE_INDEX};
        };

        Trie() : nodes_(1)
        {
        }

        void optimize()
        {
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
            size_t idx = 0;
            for(char c:url)
            {
                std::string piece(&c, 1);
                //std::string piece("/");
                if (!nodes_[idx].children.count(piece))
                {
                    nodes_[idx].children.emplace(piece, new_node());
                }
                idx = nodes_[idx].children[piece];
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
        Rule& new_rule(const std::string& rule)
        {
            rules_.emplace_back(rule);
            trie_.add(rule, rules_.size() - 1);
            return rules_.back();
        }

        void validate()
        {
            trie_.optimize();
            for(auto& rule:rules_)
            {
                rule.validate();
            }
        }

        response handle(const request& req)
        {
            size_t rule_index = trie_.find(req);

            if (rule_index == INVALID_RULE_INDEX)
                return response(404);

            if (rule_index >= rules_.size())
                throw std::runtime_error("Trie internal structure corrupted!");

            return rules_[rule_index].handle(req);
        }
    private:
        std::vector<Rule> rules_;
        Trie trie_;
    };
}
