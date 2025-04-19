#pragma once

#include "common.hpp"
#include "simdjson.h"
#include "util.hpp"
#include <iostream>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

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

    class view_model_node
    {
    private:
        std::map<int, view_model_node *> _backward_skips;
        bool _collapsed = false;

    public:
        view_model_node *forward_skip = nullptr;
        view_model_node *next;
        view_model_node *prev;

        view_entry entry;

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

    class view_model
    {
    private:
        simdjson::ondemand::parser _parser;
        view_model_node _dummy_head;
        view_model_node _dummy_tail;
        view_model_node *_head;
        view_model_node *_tail;

    public:
        view_model(simdjson::ondemand::parser &&parser)
            : _head(&_dummy_head), _tail(&_dummy_tail), _parser(std::move(parser))
        {
            _dummy_head.next = &_dummy_tail;
            _dummy_head.prev = nullptr;
            _dummy_tail.next = nullptr;
            _dummy_tail.prev = &_dummy_head;
        }

        ~view_model()
        {
            // FIXME: the following might not be complete?
            if (!_head)
            {
                return;
            }
            auto *cur = _head->next;
            while (cur != _tail->prev)
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
            _head = &_dummy_head;
            _tail = &_dummy_tail;
            model._head = nullptr;
            model._tail = nullptr;
            _head->next->prev = _head;
            _tail->prev->next = _tail;
            return *this;
        }

        view_model(view_model &&model) { *this = std::move(model); }

        DISABLE_COPY(view_model)

        inline simdjson::ondemand::parser &parser() { return _parser; }
        inline view_model_node *head() { return _head->next; }
        inline view_model_node *tail() { return _tail; }

        void append(view_entry &&entry)
        {
            auto *node = new view_model_node(std::move(entry));
            node->prev = _tail->prev;
            node->next = _tail;
            _tail->prev->next = node;
            _tail->prev = node;
        }

        void debug_print()
        {
            auto *cur = _head->next;
            while (cur != &_dummy_tail)
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

    view_model load(const std::vector<char> &content);
}
