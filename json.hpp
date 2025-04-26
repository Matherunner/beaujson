#pragma once

#include "simdjson.h"
#include "util.hpp"
#include <map>
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
        DISABLE_COPY(view_entry)

        std::string_view key;
        std::string_view value;
        size_t indent;
        size_t model_line_num;
        flags_t flags;
    };

    constexpr size_t INVALID_IDX = static_cast<size_t>(-1);

    class view_model_node
    {
    private:
        // TODO: indents should be size_t
        std::map<int, size_t> _backward_skips;
        bool _collapsed = false;

    public:
        view_entry entry;
        size_t idx_skip = INVALID_IDX;
        size_t idx_parent = INVALID_IDX;

        view_model_node() {}
        view_model_node(view_entry &&e) : entry(std::move(e)) {}
        DISABLE_COPY(view_model_node)
        DEFAULT_MOVE(view_model_node)

        inline bool collapsed() const { return _collapsed; }

        inline void set_collapsed(bool collapsed) { _collapsed = collapsed; }

        inline size_t backward() const
        {
            auto it = _backward_skips.cbegin();
            if (it != _backward_skips.cend())
            {
                return it->second;
            }
            return INVALID_IDX;
        }

        inline void add_backward(int indent, size_t idx) { _backward_skips.insert({indent, idx}); }

        inline void remove_backward(int indent) { _backward_skips.erase(indent); }
    };

    class view_model
    {
    private:
        simdjson::ondemand::parser _parser;
        std::vector<view_model_node> _nodes;

    public:
        view_model(simdjson::ondemand::parser &&parser) : _parser(std::move(parser)) {}

        DEFAULT_MOVE(view_model)
        DISABLE_COPY(view_model)

        inline simdjson::ondemand::parser &parser() { return _parser; }

        inline view_model_node &at(size_t i) { return _nodes[i]; }

        inline const view_model_node &at(size_t i) const { return _nodes[i]; }

        inline size_t idx_tail() const { return _nodes.size() - 1; }

        inline size_t forward(size_t idx) const
        {
            const auto &node = at(idx);
            if (!node.collapsed())
            {
                return idx + 1;
            }
            return node.idx_skip;
        }

        inline size_t backward(size_t idx) const
        {
            auto prev = at(idx).backward();
            if (prev == INVALID_IDX)
            {
                return idx - 1;
            }
            return prev;
        }

        size_t append(view_entry &&entry, size_t idx_parent)
        {
            auto &node = _nodes.emplace_back(view_model_node(std::move(entry)));
            node.idx_parent = idx_parent;
            return _nodes.size() - 1;
        }

        void set_expand(size_t idx)
        {
            auto &node = at(idx);
            if (!node.collapsed())
            {
                return;
            }
            node.set_collapsed(false);
            if (node.idx_skip != INVALID_IDX)
            {
                at(node.idx_skip).remove_backward(node.entry.indent);
            }
        }

        void set_collapse(size_t idx)
        {
            auto &node = at(idx);
            if (node.collapsed() || !node.entry.flags.collapsible())
            {
                return;
            }
            node.set_collapsed(true);
            if (node.idx_skip != INVALID_IDX)
            {
                at(node.idx_skip).add_backward(node.entry.indent, idx);
            }
        }

        void set_line_nums()
        {
            size_t line_num = 0;
            for (auto &node : _nodes)
            {
                node.entry.model_line_num = ++line_num;
            }
        }
    };

    view_model load(const std::vector<char> &content);
}
