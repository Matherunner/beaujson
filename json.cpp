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

namespace sjo = simdjson::ondemand;

// The following doesn't work!

// static void doc_to_view_model_iterative(view_model &model, sjo::value doc)
// {
//     struct stack_entry
//     {
//         stack_entry(int level, std::optional<std::string_view> key, sjo::value node)
//             : level(level), key(key), node(node)
//         {
//         }
//         ~stack_entry() = default;
//         DISABLE_COPY(stack_entry)
//         DEFAULT_MOVE(stack_entry)

//         int level;
//         std::optional<std::string_view> key;
//         sjo::value node;
//         std::optional<sjo::object_iterator> object_it;
//         std::optional<sjo::array_iterator> array_it;
//     };

//     std::vector<stack_entry> stack;
//     stack.emplace_back(stack_entry(0, std::nullopt, std::move(doc)));
//     while (!stack.empty())
//     {
//         std::cout << "START\n";
//         auto &cur = stack.back();
//         std::cout << "  TYPE = " << cur.node.type() << "  LEVEL = " << cur.level
//                   << "  KEY = " << (cur.key.has_value() ? cur.key.value() : "NULL") << "\n";
//         switch (cur.node.type())
//         {
//         case sjo::json_type::object:
//         {
//             sjo::object obj = cur.node.get_object();
//             if (!cur.object_it.has_value())
//             {
//                 model.append(view_entry(cur.level, view_entry_kind::object_open, cur.key, "{"));
//                 cur.object_it = obj.begin();
//                 obj.count_fields();
//                 std::cout << "  Set object ID   equals end? = " << (obj.begin() == obj.end()) << " \n";
//             }
//             sjo::object_iterator it = cur.object_it.value();
//             if (it != obj.end())
//             {
//                 auto field = *it;
//                 // std::cout << "  put first object, field = " << field << "   fieldkey = " << field.key() <<
//                 "\n"; cur.object_it = ++it; stack.emplace_back(stack_entry(cur.level + 1,
//                 field.unescaped_key().value(), field.value()));
//             }
//             else
//             {
//                 std::cout << "  pop back\n";
//                 stack.pop_back();
//             }
//             break;
//         }
//         case sjo::json_type::array:
//             // TODO:
//             // model.append(view_entry(cur.level, view_entry_kind::array_open, cur.key, "["));
//             // cur.node.get_array().begin()
//             stack.pop_back();
//             break;
//         case sjo::json_type::boolean:
//             model.append(view_entry(cur.level, view_entry_kind::boolean, cur.key, cur.node.raw_json_token()));
//             stack.pop_back();
//             break;
//         case sjo::json_type::number:
//             model.append(view_entry(cur.level, view_entry_kind::number, cur.key, cur.node.raw_json_token()));
//             stack.pop_back();
//             break;
//         case sjo::json_type::string:
//             model.append(view_entry(cur.level, view_entry_kind::string, cur.key, cur.node.raw_json_token()));
//             stack.pop_back();
//             break;
//         case sjo::json_type::null:
//             model.append(view_entry(cur.level, view_entry_kind::null, cur.key, cur.node.raw_json_token()));
//             stack.pop_back();
//             break;
//         default:
//             throw std::logic_error("unknown doc type");
//         }
//     }
// }

namespace json
{
    static void doc_to_view_model(view_model &model, sjo::value doc, std::optional<std::string_view> key, size_t level,
                                  size_t idx_parent)
    {
        switch (doc.type())
        {
        case sjo::json_type::object:
            idx_parent = model.append(
                view_entry(key.value_or(""), "{", level, view_entry_kind::object_open, key.has_value()), idx_parent);
            for (auto elem : doc.get_object())
            {
                doc_to_view_model(model, elem.value(), elem.escaped_key(), level + 1, idx_parent);
            }
            break;
        case sjo::json_type::array:
            idx_parent = model.append(
                view_entry(key.value_or(""), "[", level, view_entry_kind::array_open, key.has_value()), idx_parent);
            for (auto elem : doc.get_array())
            {
                doc_to_view_model(model, elem.value(), std::nullopt, level + 1, idx_parent);
            }
            break;
        case sjo::json_type::boolean:
            model.append(view_entry(key.value_or(""), util::trim_space(doc.raw_json_token()), level,
                                    view_entry_kind::boolean, key.has_value()),
                         idx_parent);
            break;
        case sjo::json_type::number:
            model.append(view_entry(key.value_or(""), util::trim_space(doc.raw_json_token()), level,
                                    view_entry_kind::number, key.has_value()),
                         idx_parent);
            break;
        case sjo::json_type::string:
            model.append(view_entry(key.value_or(""), util::trim_space(doc.raw_json_token()), level,
                                    view_entry_kind::string, key.has_value()),
                         idx_parent);
            break;
        case sjo::json_type::null:
            model.append(view_entry(key.value_or(""), util::trim_space(doc.raw_json_token()), level,
                                    view_entry_kind::null, key.has_value()),
                         idx_parent);
            break;
        default:
            throw std::logic_error("unknown doc type");
        }
    }

    static inline void doc_to_view_model(view_model &model, sjo::document doc)
    {
        doc_to_view_model(model, doc, std::nullopt, 0, INVALID_IDX);
        // Add sentinel (tail)
        model.append(json::view_entry(), INVALID_IDX);
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
        for (auto idx : stack)
        {
            model.at(idx).idx_skip = model.idx_tail();
        }
    }

    view_model load(const std::vector<char> &content)
    {
        view_model model(sjo::parser{});
        sjo::document doc = model.parser().iterate(content.data(), content.size(), content.capacity());
        doc_to_view_model(model, std::move(doc));
        add_skips(model);
        model.set_line_nums();
        return model;
    }
}
