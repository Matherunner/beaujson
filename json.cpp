#include "json.hpp"

#include "common.hpp"
#include "simdjson.h"
#include "util.hpp"
#include <iostream>
#include <stdexcept>

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
    view_model_node() {}
    view_model_node(view_entry &&e) : entry(std::move(e)) {}
    ~view_model_node() = default;
    DISABLE_COPY(view_model_node)
    DEFAULT_MOVE(view_model_node)

    view_model_node *next;
    view_model_node *prev;
    view_model_node *skip;
    view_entry entry;
};

class view_model
{
private:
    view_model_node *_head;
    view_model_node *_tail;
    view_model_node _dummy_head;
    view_model_node _dummy_tail;

public:
    view_model() : _head(&_dummy_head), _tail(&_dummy_tail)
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

    void append(view_entry &&entry)
    {
        auto *node = new view_model_node(std::move(entry));
        node->prev = _tail->prev;
        node->next = _tail;
        node->skip = nullptr;
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
            std::cout << '\n';
            cur = cur->next;
        }
    }
};

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
//                 // std::cout << "  put first object, field = " << field << "   fieldkey = " << field.key() << "\n";
//                 cur.object_it = ++it;
//                 stack.emplace_back(stack_entry(cur.level + 1, field.unescaped_key().value(), field.value()));
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

static void doc_to_view_model(view_model &model, sjo::value doc, std::optional<std::string_view> key = std::nullopt,
                              int level = 0)
{
    switch (doc.type())
    {
    case sjo::json_type::object:
        model.append(view_entry(level, view_entry_kind::object_open, key, "{"));
        for (auto elem : doc.get_object())
        {
            doc_to_view_model(model, elem.value(), elem.unescaped_key(), level + 1);
        }
        break;
    case sjo::json_type::array:
        model.append(view_entry(level, view_entry_kind::array_open, key, "["));
        for (auto elem : doc.get_array())
        {
            doc_to_view_model(model, elem.value(), std::nullopt, level + 1);
        }
        break;
    case sjo::json_type::boolean:
        model.append(view_entry(level, view_entry_kind::boolean, key, util::trim_space(doc.raw_json_token())));
        break;
    case sjo::json_type::number:
        model.append(view_entry(level, view_entry_kind::number, key, util::trim_space(doc.raw_json_token())));
        break;
    case sjo::json_type::string:
        model.append(view_entry(level, view_entry_kind::string, key, util::trim_space(doc.raw_json_token())));
        break;
    case sjo::json_type::null:
        model.append(view_entry(level, view_entry_kind::null, key, util::trim_space(doc.raw_json_token())));
        break;
    default:
        throw std::logic_error("unknown doc type");
    }
}

static void add_skips(view_model &model)
{
    std::vector<view_model_node *> stack;
    auto *cur = model.head();
    int indent = 0;
    while (cur)
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
                top->skip = cur;
                stack.pop_back();
            }
            indent = cur->entry.indent;
        }
        switch (cur->entry.kind)
        {
        case view_entry_kind::object_open:
        case view_entry_kind::array_open:
            stack.emplace_back(cur);
            ++indent;
            break;
        }
        cur = cur->next;
    }
    for (auto elem : stack)
    {
        // FIXME: this creates problem if obj/arr is at the root: it has nothing to skip to even though it should be
        // collapsible!
        elem->skip = nullptr;
    }
}

void json::load(const std::vector<char> &content)
{
    sjo::parser parser;
    sjo::document doc = parser.iterate(content.data(), content.size(), content.capacity());
    view_model model;
    doc_to_view_model(model, std::move(doc));
    add_skips(model);
    model.debug_print();
}
