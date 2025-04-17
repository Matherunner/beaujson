#pragma once

#include "common.hpp"
#include "simdjson.h"
#include "util.hpp"
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace json
{
    namespace sjo = simdjson::ondemand;

    enum class view_entry_kind : int
    {
        object_open,
        array_open,
        string,
        number,
        boolean,
        null,
    };

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

    struct view_model_node
    {
        view_model_node *next;
        view_model_node *prev;
        view_model_node *skip = nullptr;
        bool collapsed = false;
        view_entry entry;

        view_model_node() {}
        view_model_node(view_entry &&e) : entry(std::move(e)) {}
        ~view_model_node() = default;
        DISABLE_COPY(view_model_node)
        DEFAULT_MOVE(view_model_node)
    };

    class view_model
    {
    private:
        view_model_node *_head;
        view_model_node *_tail;
        view_model_node _dummy_head;
        view_model_node _dummy_tail;
        sjo::document _doc;

    public:
        view_model(sjo::document &&doc) : _head(&_dummy_head), _tail(&_dummy_tail), _doc(std::move(doc))
        {
            _dummy_head.next = &_dummy_tail;
            _dummy_head.prev = nullptr;
            _dummy_tail.next = nullptr;
            _dummy_tail.prev = &_dummy_head;
        }
        virtual ~view_model()
        {
            auto *cur = _head->next;
            while (cur != &_dummy_tail)
            {
                auto *next = cur->next;
                delete cur;
                cur = next;
            }
        }
        DISABLE_COPY(view_model)
        DEFAULT_MOVE(view_model)

        inline view_model_node *head() { return _head->next; }
        inline view_model_node *tail() { return _tail->prev; }
        inline sjo::document &document() { return _doc; }

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
                if (cur->skip)
                {
                    std::cout << " (skip to " << cur->skip->entry.key.value_or("<no key>") << ")";
                }
                if (cur->collapsed)
                {
                    std::cout << " [COLAPPSED]";
                }
                std::cout << '\n';
                if (cur->collapsed)
                {
                    cur = cur->skip;
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