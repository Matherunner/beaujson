// This file is part of beaujson.
//
// beaujson is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// beaujson is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with beaujson.  If not, see <https://www.gnu.org/licenses/>.

#include "json.hpp"

#include "simdjson.h"
#include <stdexcept>
#include <thread>

namespace sjo = simdjson::ondemand;

namespace json
{
    namespace string_views
    {
        constexpr auto empty = std::string_view();
        constexpr auto open_brace = std::string_view("{");
        constexpr auto open_bracket = std::string_view("[");
    }

    static void doc_to_view_model(view_model &model, const std::string_view key, const bool has_key, sjo::value doc,
                                  const size_t level)
    {
        switch (doc.type())
        {
        case sjo::json_type::object:
            model.append(view_entry(key, string_views::open_brace, level, view_entry_kind::object_open, has_key));
            for (auto elem : doc.get_object())
            {
                doc_to_view_model(model, elem.escaped_key(), true, elem.value().value(), level + 1);
            }
            break;
        case sjo::json_type::array:
            model.append(view_entry(key, string_views::open_bracket, level, view_entry_kind::array_open, has_key));
            for (auto elem : doc.get_array())
            {
                doc_to_view_model(model, string_views::empty, false, elem.value(), level + 1);
            }
            break;
        case sjo::json_type::boolean:
            model.append(view_entry(key, doc.raw_json_token(), level, view_entry_kind::boolean, has_key));
            break;
        case sjo::json_type::number:
            model.append(view_entry(key, doc.raw_json_token(), level, view_entry_kind::number, has_key));
            break;
        case sjo::json_type::string:
            model.append(view_entry(key, doc.raw_json_token(), level, view_entry_kind::string, has_key));
            break;
        case sjo::json_type::null:
            model.append(view_entry(key, doc.raw_json_token(), level, view_entry_kind::null, has_key));
            break;
        default:
            throw std::logic_error("unknown doc type");
        }
    }

    static void doc_to_view_model(view_model &model, sjo::document doc)
    {
        doc_to_view_model(model, string_views::empty, false, doc, 0);
        // Add sentinel (tail)
        model.append(view_entry());
    }

    static void add_parents(view_model &model)
    {
        std::vector<size_t> parents;
        parents.emplace_back(INVALID_IDX);
        const auto end = model.idx_tail();
        for (size_t i = 0; i < end; ++i)
        {
            auto &node = model.at(i);
            if (node.entry.indent() > parents.size() - 1)
            {
                parents.emplace_back(i - 1);
            }
            else
            {
                while (node.entry.indent() < parents.size() - 1)
                {
                    parents.pop_back();
                }
            }
            node.idx_parent = parents.back();
        }
    }

    static void add_skips(view_model &model)
    {
        size_t indent = 0;
        std::vector<size_t> stack;
        for (size_t i = 0; i < model.idx_tail(); ++i)
        {
            auto &cur = model.at(i);
            if (cur.entry.indent() < indent)
            {
                while (!stack.empty())
                {
                    auto &top = model.at(stack.back());
                    if (top.entry.indent() < cur.entry.indent())
                    {
                        break;
                    }
                    top.idx_skip = i;
                    stack.pop_back();
                }
                indent = cur.entry.indent();
            }
            if (cur.entry.flags().collapsible())
            {
                stack.emplace_back(i);
                ++indent;
            }
        }
        for (const auto idx : stack)
        {
            model.at(idx).idx_skip = model.idx_tail();
        }
    }

    view_model load(const std::vector<char> &content)
    {
        view_model model(sjo::parser{});
        sjo::document doc = model.parser().iterate(content.data(), content.size(), content.capacity());
        doc_to_view_model(model, std::move(doc));

        std::jthread t1(add_parents, std::ref(model));
        std::jthread t2(add_skips, std::ref(model));
        std::jthread t3(&view_model::set_line_nums, &model);

        return model;
    }
}
