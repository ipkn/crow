#include "routing.h"

namespace crow
{
    bool Trie::Node::IsSimpleNode() const
    {
        return 
            !rule_index &&
            std::all_of(
                    std::begin(param_childrens), 
                    std::end(param_childrens), 
                    [](unsigned x){ return !x; });
    }

    Trie::Trie()
        : nodes_(1)
    {
    }

    void Trie::optimizeNode(Trie::Node* node)
    {
        for(auto x : node->param_childrens)
        {
            if (!x)
                continue;
            Trie::Node* child = &nodes_[x];
            optimizeNode(child);
        }
        if (node->children.empty())
            return;
        bool mergeWithChild = true;
        for(auto& kv : node->children)
        {
            Trie::Node* child = &nodes_[kv.second];
            if (!child->IsSimpleNode())
            {
                mergeWithChild = false;
                break;
            }
        }
        if (mergeWithChild)
        {
            decltype(node->children) merged;
            for(auto& kv : node->children)
            {
                Trie::Node* child = &nodes_[kv.second];
                for(auto& child_kv : child->children)
                {
                    merged[kv.first + child_kv.first] = child_kv.second;
                }
            }
            node->children = std::move(merged);
            optimizeNode(node);
        }
        else
        {
            for(auto& kv : node->children)
            {
                Trie::Node* child = &nodes_[kv.second];
                optimizeNode(child);
            }
        }
    }

    void Trie::optimize()
    {
        optimizeNode(head());
    }

    void Trie::validate()
    {
        if (!head()->IsSimpleNode())
            throw std::runtime_error("Internal error: Trie header should be simple!");
        optimize();
    }

    std::pair<unsigned, routing_params> Trie::find(
        const request& req, 
        const Trie::Node* node /*= nullptr*/, 
        unsigned pos /*= 0*/, 
        routing_params* params /*= nullptr*/) const
    {
        routing_params empty;
        if (params == nullptr)
            params = &empty;

        unsigned found{};
        routing_params match_params;

        if (node == nullptr)
            node = head();
        if (pos == req.url.size())
            return {node->rule_index, *params};

        auto update_found = [&found, &match_params](std::pair<unsigned, routing_params>& ret)
        {
            if (ret.first && (!found || found > ret.first))
            {
                found = ret.first;
                match_params = std::move(ret.second);
            }
        };

        if (node->param_childrens[(int)ParamType::INT])
        {
            char c = req.url[pos];
            if ((c >= '0' && c <= '9') || c == '+' || c == '-')
            {
                char* eptr;
                errno = 0;
                long long int value = strtoll(req.url.data()+pos, &eptr, 10);
                if (errno != ERANGE && eptr != req.url.data()+pos)
                {
                    params->int_params.push_back(value);
                    auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::INT]], eptr - req.url.data(), params);
                    update_found(ret);
                    params->int_params.pop_back();
                }
            }
        }

        if (node->param_childrens[(int)ParamType::UINT])
        {
            char c = req.url[pos];
            if ((c >= '0' && c <= '9') || c == '+')
            {
                char* eptr;
                errno = 0;
                unsigned long long int value = strtoull(req.url.data()+pos, &eptr, 10);
                if (errno != ERANGE && eptr != req.url.data()+pos)
                {
                    params->uint_params.push_back(value);
                    auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::UINT]], eptr - req.url.data(), params);
                    update_found(ret);
                    params->uint_params.pop_back();
                }
            }
        }

        if (node->param_childrens[(int)ParamType::DOUBLE])
        {
            char c = req.url[pos];
            if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.')
            {
                char* eptr;
                errno = 0;
                double value = strtod(req.url.data()+pos, &eptr);
                if (errno != ERANGE && eptr != req.url.data()+pos)
                {
                    params->double_params.push_back(value);
                    auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::DOUBLE]], eptr - req.url.data(), params);
                    update_found(ret);
                    params->double_params.pop_back();
                }
            }
        }

        if (node->param_childrens[(int)ParamType::STRING])
        {
            size_t epos = pos;
            for(; epos < req.url.size(); epos ++)
            {
                if (req.url[epos] == '/')
                    break;
            }

            if (epos != pos)
            {
                params->string_params.push_back(req.url.substr(pos, epos-pos));
                auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::STRING]], epos, params);
                update_found(ret);
                params->string_params.pop_back();
            }
        }

        if (node->param_childrens[(int)ParamType::PATH])
        {
            size_t epos = req.url.size();

            if (epos != pos)
            {
                params->string_params.push_back(req.url.substr(pos, epos-pos));
                auto ret = find(req, &nodes_[node->param_childrens[(int)ParamType::PATH]], epos, params);
                update_found(ret);
                params->string_params.pop_back();
            }
        }

        for(auto& kv : node->children)
        {
            const std::string& fragment = kv.first;
            const Trie::Node* child = &nodes_[kv.second];

            if (req.url.compare(pos, fragment.size(), fragment) == 0)
            {
                auto ret = find(req, child, pos + fragment.size(), params);
                update_found(ret);
            }
        }

        return {found, match_params};
    }

    void Trie::add(const std::string& url, unsigned rule_index)
    {
        unsigned idx{0};

        for(unsigned i = 0; i < url.size(); i ++)
        {
            char c = url[i];
            if (c == '<')
            {
                static struct ParamTraits
                {
                    ParamType type;
                    std::string name;
                } paramTraits[] =
                {
                    { ParamType::INT, "<int>" },
                    { ParamType::UINT, "<uint>" },
                    { ParamType::DOUBLE, "<float>" },
                    { ParamType::DOUBLE, "<double>" },
                    { ParamType::STRING, "<str>" },
                    { ParamType::STRING, "<string>" },
                    { ParamType::PATH, "<path>" },
                };

                for(auto& x:paramTraits)
                {
                    if (url.compare(i, x.name.size(), x.name) == 0)
                    {
                        if (!nodes_[idx].param_childrens[(int)x.type])
                        {
                            auto new_node_idx = new_node();
                            nodes_[idx].param_childrens[(int)x.type] = new_node_idx;
                        }
                        idx = nodes_[idx].param_childrens[(int)x.type];
                        i += x.name.size();
                        break;
                    }
                }

                i --;
            }
            else
            {
                std::string piece(&c, 1);
                if (!nodes_[idx].children.count(piece))
                {
                    auto new_node_idx = new_node();
                    nodes_[idx].children.emplace(piece, new_node_idx);
                }
                idx = nodes_[idx].children[piece];
            }
        }
        if (nodes_[idx].rule_index)
            throw std::runtime_error("handler already exists for " + url);
        nodes_[idx].rule_index = rule_index;
    }

    void Trie::debug_node_print(Trie::Node* n, int level)
    {
        for(int i = 0; i < (int)ParamType::MAX; i ++)
        {
            if (n->param_childrens[i])
            {
                CROW_LOG_DEBUG << std::string(2*level, ' ') /*<< "("<<n->param_childrens[i]<<") "*/;
                switch((ParamType)i)
                {
                    case ParamType::INT:
                        CROW_LOG_DEBUG << "<int>";
                        break;
                    case ParamType::UINT:
                        CROW_LOG_DEBUG << "<uint>";
                        break;
                    case ParamType::DOUBLE:
                        CROW_LOG_DEBUG << "<float>";
                        break;
                    case ParamType::STRING:
                        CROW_LOG_DEBUG << "<str>";
                        break;
                    case ParamType::PATH:
                        CROW_LOG_DEBUG << "<path>";
                        break;
                    default:
                        CROW_LOG_DEBUG << "<ERROR>";
                        break;
                }

                debug_node_print(&nodes_[n->param_childrens[i]], level+1);
            }
        }
        for(auto& kv : n->children)
        {
            CROW_LOG_DEBUG << std::string(2*level, ' ') /*<< "(" << kv.second << ") "*/ << kv.first;
            debug_node_print(&nodes_[kv.second], level+1);
        }
    }

    void Trie::debug_print()
    {
        debug_node_print(head(), 0);
    }

    const Trie::Node* Trie::head() const
    {
        return &nodes_.front();
    }

    Trie::Node* Trie::head()
    {
        return &nodes_.front();
    }

    unsigned Trie::new_node()
    {
        nodes_.resize(nodes_.size()+1);
        return nodes_.size() - 1;
    }

    Router::Router()
        : rules_(1)
    {
    }

    void Router::validate()
    {
        trie_.validate();
        for(auto& rule:rules_)
        {
            if (rule)
                rule->validate();
        }
    }

    void Router::handle(const request& req, response& res)
    {
        auto found = trie_.find(req);

        unsigned rule_index = found.first;

        if (!rule_index)
        {
            CROW_LOG_DEBUG << "Cannot match rules " << req.url;
            res = response(404);
            res.end();
            return;
        }

        if (rule_index >= rules_.size())
            throw std::runtime_error("Trie internal structure corrupted!");

        CROW_LOG_DEBUG << "Matched rule '" << ((TaggedRule<>*)rules_[rule_index].get())->rule_ << "'";

        rules_[rule_index]->handle(req, res, found.second);
    }

    void Router::debug_print()
    {
        trie_.debug_print();
    }
}
