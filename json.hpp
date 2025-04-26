#pragma once

#include "simdjson.h"
#include "util.hpp"
#include <iostream>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

namespace json
{
    namespace entry_flag
    {
        constexpr uint32_t OBJECT_OPEN_KIND = 1 << 0;
        constexpr uint32_t ARRAY_OPEN_KIND = 1 << 1;
        constexpr uint32_t STRING_KIND = 1 << 2;
        constexpr uint32_t NUMBER_KIND = 1 << 3;
        constexpr uint32_t BOOLEAN_KIND = 1 << 4;
        constexpr uint32_t NULL_KIND = 1 << 5;
        constexpr uint32_t HAS_KEY = 1 << 6;
    }

    enum class view_entry_kind : int
    {
        object_open,
        array_open,
        string,
        number,
        boolean,
        null,
    };

    constexpr uint32_t entry_kind_to_bits(view_entry_kind kind)
    {
        switch (kind)
        {
        case view_entry_kind::object_open:
            return entry_flag::OBJECT_OPEN_KIND;
        case view_entry_kind::array_open:
            return entry_flag::ARRAY_OPEN_KIND;
        case view_entry_kind::string:
            return entry_flag::STRING_KIND;
        case view_entry_kind::number:
            return entry_flag::NUMBER_KIND;
        case view_entry_kind::boolean:
            return entry_flag::BOOLEAN_KIND;
        case view_entry_kind::null:
            return entry_flag::NULL_KIND;
        default:
            throw std::logic_error("unknown view entry kind");
        }
    }

    struct view_entry
    {
        class flags_t
        {
        private:
            uint32_t _b;

        public:
            flags_t() : _b(0) {}
            flags_t(uint32_t b) : _b(b) {}

            inline uint32_t bits() const { return _b; }
            inline void set_bits(uint32_t b) { _b = b; }

            inline bool object_open() const { return _b & entry_flag::OBJECT_OPEN_KIND; }
            inline bool array_open() const { return _b & entry_flag::ARRAY_OPEN_KIND; }

            inline bool primitive() const
            {
                return _b & (entry_flag::NULL_KIND | entry_flag::NUMBER_KIND | entry_flag::STRING_KIND |
                             entry_flag::BOOLEAN_KIND);
            }

            inline bool collapsible() const
            {
                return _b & (entry_flag::OBJECT_OPEN_KIND | entry_flag::ARRAY_OPEN_KIND);
            }

            inline bool has_key() const { return _b & entry_flag::HAS_KEY; }
        };

        view_entry() {}
        view_entry(std::string_view key, std::string_view value, size_t indent, view_entry_kind kind, bool has_key)
            : key(key), value(value), indent(indent), model_line_num(0),
              flags(flags_t(entry_kind_to_bits(kind) | (entry_flag::HAS_KEY & -has_key)))
        {
        }
        DEFAULT_MOVE(view_entry)

        std::string_view key;
        std::string_view value;
        size_t indent;
        size_t model_line_num;
        flags_t flags;
    };

    class view_model_node
    {
    private:
        std::map<int, view_model_node *> _backward_skips;
        bool _collapsed = false;

    public:
        view_entry entry;
        view_model_node *forward_skip = nullptr;
        view_model_node *parent;
        view_model_node *next;
        view_model_node *prev;

        view_model_node() {};
        view_model_node(view_entry &&e) : entry(std::move(e)) {}
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
            if (_collapsed || !entry.flags.collapsible())
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
        std::unique_ptr<view_model_node> _dummy_head;
        std::unique_ptr<view_model_node> _dummy_tail;

    public:
        view_model(simdjson::ondemand::parser &&parser)
            : _parser(std::move(parser)), _dummy_head(std::make_unique<view_model_node>(view_model_node())),
              _dummy_tail(std::make_unique<view_model_node>(view_model_node()))
        {
            _dummy_head->next = _dummy_tail.get();
            _dummy_head->prev = nullptr;
            _dummy_head->parent = nullptr;
            _dummy_head->entry.key = "[HEAD]";
            _dummy_tail->next = nullptr;
            _dummy_tail->prev = _dummy_head.get();
            _dummy_tail->parent = nullptr;
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
        inline view_model_node *head() const { return _dummy_head->next; }
        inline view_model_node *tail() const { return _dummy_tail.get(); }

        view_model_node *append(view_entry &&entry, view_model_node *parent)
        {
            auto *node = new view_model_node(std::move(entry));
            node->prev = _dummy_tail->prev;
            node->next = _dummy_tail.get();
            node->parent = parent;
            _dummy_tail->prev->next = node;
            _dummy_tail->prev = node;
            return node;
        }

        void debug_print()
        {
            auto *cur = _dummy_head->next;
            while (cur != _dummy_tail.get())
            {
                std::cout << "LINE: ";
                for (size_t i = 0; i < cur->entry.indent; ++i)
                {
                    std::cout << "  ";
                }
                if (cur->entry.flags.has_key())
                {
                    std::cout << cur->entry.key << ": ";
                }
                std::cout << cur->entry.value;
                if (cur->forward_skip)
                {
                    std::cout << " (skip to " << cur->forward_skip->entry.key << ")";
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
