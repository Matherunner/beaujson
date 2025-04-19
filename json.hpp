#pragma once

#include "common.hpp"
#include "simdjson.h"
#include "util.hpp"
#include <iostream>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
    namespace sjo = simdjson::ondemand;
}

namespace json
{
    enum class view_entry_kind : int
    {
        object_open,
        array_open,
        string,
        number,
        boolean,
        null,
    };

    constexpr inline bool is_collapsible(view_entry_kind kind)
    {
        return kind == view_entry_kind::object_open || kind == view_entry_kind::array_open;
    }

    struct view_entry
    {
        view_entry() {}
        view_entry(int indent, view_entry_kind kind, std::optional<std::string_view> key, std::string_view value)
            : indent(indent), kind(kind), key(key), value(value)
        {
        }
        ~view_entry() = default;
        DISABLE_COPY(view_entry)
        DEFAULT_MOVE(view_entry)

        int indent;
        view_entry_kind kind;
        std::optional<std::string_view> key;
        std::string_view value;
    };

    template <typename M>
    class view_model_node
    {
    private:
        M _metadata;
        std::map<int, view_model_node *> _backward_skips;
        bool _collapsed = false;

    public:
        view_entry entry;
        view_model_node *forward_skip = nullptr;
        view_model_node *next;
        view_model_node *prev;

        view_model_node() {}
        view_model_node(view_entry &&e) : entry(std::move(e)) {}
        ~view_model_node() = default;
        DISABLE_COPY(view_model_node)
        DEFAULT_MOVE(view_model_node)

        inline bool collapsed() const { return _collapsed; }
        inline view_model_node *forward() const { return _collapsed ? forward_skip : next; }
        inline view_model_node *backward() const
        {
            auto it = _backward_skips.cbegin();
            if (it == _backward_skips.cend())
            {
                return prev;
            }
            return it->second;
        }

        void set_collapse()
        {
            if (_collapsed)
            {
                return;
            }
            _collapsed = true;
            if (forward_skip)
            {
                forward_skip->_backward_skips.insert({entry.indent, this});
            }
        }

        void set_expand()
        {
            if (!_collapsed)
            {
                return;
            }
            _collapsed = false;
            if (forward_skip)
            {
                forward_skip->_backward_skips.erase(entry.indent);
            }
        }
    };

    template <typename M>
    class view_model
    {
    private:
        using node_type = view_model_node<M>;

        simdjson::ondemand::parser _parser;
        std::unique_ptr<node_type> _dummy_head;
        std::unique_ptr<node_type> _dummy_tail;

    public:
        view_model(simdjson::ondemand::parser &&parser)
            : _parser(std::move(parser)), _dummy_head(std::make_unique<node_type>(node_type())),
              _dummy_tail(std::make_unique<node_type>(node_type()))
        {
            _dummy_head->next = _dummy_tail.get();
            _dummy_head->prev = nullptr;
            _dummy_head->entry.key = "[HEAD]";
            _dummy_tail->next = nullptr;
            _dummy_tail->prev = _dummy_head.get();
            _dummy_tail->entry.key = "[TAIL]";
        }

        ~view_model()
        {
            if (!_dummy_head)
            {
                return;
            }
            auto *cur = _dummy_head->next;
            while (cur != _dummy_tail.get())
            {
                auto *next = cur->next;
                delete cur;
                cur = next;
            }
        }

        view_model &operator=(view_model &&model)
        {
            std::swap(_parser, model._parser);
            std::swap(_dummy_head, model._dummy_head);
            std::swap(_dummy_tail, model._dummy_tail);
            return *this;
        }

        view_model(view_model &&model) { *this = std::move(model); }

        DISABLE_COPY(view_model)

        inline simdjson::ondemand::parser &parser() { return _parser; }
        inline node_type *head() const { return _dummy_head->next; }
        inline node_type *tail() const { return _dummy_tail.get(); }

        void append(view_entry &&entry)
        {
            auto *node = new node_type(std::move(entry));
            node->prev = _dummy_tail->prev;
            node->next = _dummy_tail.get();
            _dummy_tail->prev->next = node;
            _dummy_tail->prev = node;
        }

        void debug_print()
        {
            auto *cur = _dummy_head->next;
            while (cur != _dummy_tail.get())
            {
                std::cout << "LINE: ";
                for (int i = 0; i < cur->entry.indent; ++i)
                {
                    std::cout << "  ";
                }
                if (cur->entry.key.has_value())
                {
                    std::cout << cur->entry.key.value() << ": ";
                }
                std::cout << cur->entry.value;
                if (cur->forward_skip)
                {
                    std::cout << " (skip to " << cur->forward_skip->entry.key.value_or("<no key>") << ")";
                }
                if (cur->collapsed())
                {
                    std::cout << " [COLLAPSED]";
                }
                std::cout << '\n';
                if (cur->collapsed())
                {
                    cur = cur->forward_skip;
                }
                else
                {
                    cur = cur->next;
                }
            }
        }
    };
}

namespace
{
    template <typename M>
    void doc_to_view_model(json::view_model<M> &model, sjo::value doc, std::optional<std::string_view> key, int level)
    {
        switch (doc.type())
        {
        case sjo::json_type::object:
            model.append(json::view_entry(level, json::view_entry_kind::object_open, key, "{"));
            for (auto elem : doc.get_object())
            {
                doc_to_view_model(model, elem.value(), elem.unescaped_key(), level + 1);
            }
            break;
        case sjo::json_type::array:
            model.append(json::view_entry(level, json::view_entry_kind::array_open, key, "["));
            for (auto elem : doc.get_array())
            {
                doc_to_view_model(model, elem.value(), std::nullopt, level + 1);
            }
            break;
        case sjo::json_type::boolean:
            model.append(
                json::view_entry(level, json::view_entry_kind::boolean, key, util::trim_space(doc.raw_json_token())));
            break;
        case sjo::json_type::number:
            model.append(
                json::view_entry(level, json::view_entry_kind::number, key, util::trim_space(doc.raw_json_token())));
            break;
        case sjo::json_type::string:
            model.append(
                json::view_entry(level, json::view_entry_kind::string, key, util::trim_space(doc.raw_json_token())));
            break;
        case sjo::json_type::null:
            model.append(
                json::view_entry(level, json::view_entry_kind::null, key, util::trim_space(doc.raw_json_token())));
            break;
        default:
            throw std::logic_error("unknown doc type");
        }
    }

    template <typename M>
    inline void doc_to_view_model(json::view_model<M> &model, sjo::document doc)
    {
        doc_to_view_model(model, doc, std::nullopt, 0);
    }

    template <typename M>
    void add_skips(json::view_model<M> &model)
    {
        std::vector<json::view_model_node<M> *> stack;
        auto *cur = model.head();
        int indent = 0;
        while (cur != model.tail())
        {
            if (cur->entry.indent < indent)
            {
                while (!stack.empty())
                {
                    auto top = stack.back();
                    if (top->entry.indent < cur->entry.indent)
                    {
                        break;
                    }
                    top->forward_skip = cur;
                    stack.pop_back();
                }
                indent = cur->entry.indent;
            }
            switch (cur->entry.kind)
            {
            case json::view_entry_kind::object_open:
            case json::view_entry_kind::array_open:
                stack.emplace_back(cur);
                ++indent;
                break;
            }
            cur = cur->next;
        }
        for (auto *elem : stack)
        {
            elem->forward_skip = model.tail();
        }
    }
}

namespace json
{
    template <typename M>
    view_model<M> load(const std::vector<char> &content)
    {
        view_model<M> model(sjo::parser{});
        simdjson::ondemand::document doc = model.parser().iterate(content.data(), content.size(), content.capacity());
        doc_to_view_model(model, std::move(doc));
        add_skips(model);
        return model;
    }
}