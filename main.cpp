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

#include <clocale>
#include <csignal>
#include <cstdio>
#include <format>
#include <variant>

#include "CLI11.hpp"
#include "app.hpp"
#include "clipboard.hpp"
#include "json.hpp"
#include "simdjson.h"
#include "utf8.h"

enum class data_source
{
    clipboard,
    pipe,
};

class main_handler
{
private:
    static constexpr int MINIMUM_WIDTH = 16;

    std::string _file_name;
    std::string _print_buffer;
    std::vector<char> _content;
    json::view_model _view_model;
    size_t _idx_cur = json::INVALID_IDX;
    int _row_highlight = -1;

    inline static int content_screen_height(const app_state &state) { return state.rows() - 2; }

    inline static size_t get_key_width_limit(const app_state &state)
    {
        // Account for "<ellipses character>: "
        return state.cols() / 3 - 3;
    }

    inline static size_t get_value_max_col(const app_state &state)
    {
        // Account for "<ellipses character> [+]"
        return state.cols() - 5;
    }

    size_t idx_at_line(int line) const
    {
        if (line < 0)
        {
            return json::INVALID_IDX;
        }
        size_t cur = _idx_cur;
        for (int i = 0; i < line && cur < _view_model.idx_tail(); ++i)
        {
            cur = _view_model.forward(cur);
        }
        return cur < _view_model.idx_tail() ? cur : json::INVALID_IDX;
    }

    void print_json(const app_state &state)
    {
        erase();

        if (state.cols() < MINIMUM_WIDTH)
        {
            return;
        }

        const int row_end = content_screen_height(state);
        auto p = _idx_cur;
        auto last = _idx_cur;
        int i = 0;
        for (; i < row_end && p < _view_model.idx_tail(); ++i)
        {
            _print_buffer.clear();

            last = p;
            const auto &node = _view_model.at(p);
            size_t cur_col = node.entry.indent();

            move(i, 0);

            attr_on(A_DIM, nullptr);
            for (size_t j = 0; j < node.entry.indent(); ++j)
            {
                addch(ACS_BULLET);
            }
            attr_off(A_DIM, nullptr);

            if (node.entry.flags().has_key())
            {
                auto it = node.entry.key().cbegin();
                auto end = node.entry.key().cend();
                while (it != end)
                {
                    auto old = it;
                    int32_t c = utf8::next(it, end);
                    cur_col += util::is_full_width(c) + 1;
                    if (cur_col > get_key_width_limit(state))
                    {
                        break;
                    }
                    while (old != it)
                    {
                        _print_buffer.push_back(*old++);
                    }
                }
                if (it != end)
                {
                    _print_buffer += "…";
                    ++cur_col;
                }
                _print_buffer += ": ";
                cur_col += 2;
            }
            {
                auto it = node.entry.value().cbegin();
                auto end = node.entry.value().cend();
                while (it != end)
                {
                    auto old = it;
                    int32_t c = utf8::next(it, end);
                    cur_col += util::is_full_width(c) + 1;
                    if (cur_col > get_value_max_col(state))
                    {
                        break;
                    }
                    while (old != it)
                    {
                        _print_buffer.push_back(*old++);
                    }
                }
                if (it != end)
                {
                    _print_buffer += "…";
                    ++cur_col;
                }
            }
            if (node.entry.flags().collapsible())
            {
                if (node.collapsed())
                {
                    _print_buffer += " [+]";
                }
                else
                {
                    _print_buffer += " [-]";
                }
                cur_col += 4;
            }
            addstr(_print_buffer.c_str());
            p = _view_model.forward(p);
        }

        attr_on(A_BOLD, nullptr);
        for (; i < row_end; ++i)
        {
            mvaddstr(i, 0, "~");
        }
        attr_off(A_BOLD, nullptr);

        if (_row_highlight >= 0)
        {
            auto idx = idx_at_line(_row_highlight);
            size_t x = idx != json::INVALID_IDX ? _view_model.at(idx).entry.indent() : 0;
            if (x < static_cast<size_t>(state.cols()))
            {
                mvchgat(_row_highlight, static_cast<int>(x), -1, A_STANDOUT, 0, nullptr);
            }
        }

        print_breadcrumb(state);

        print_status_bar(state, last);
    }

    void print_breadcrumb(const app_state &state)
    {
        if (_row_highlight < 0)
        {
            return;
        }

        auto p = idx_at_line(_row_highlight);
        if (!p)
        {
            return;
        }

        std::vector<std::string_view> buffer;
        while (p < _view_model.idx_tail())
        {
            const auto &node = _view_model.at(p);
            if (node.entry.key().empty())
            {
                if (node.entry.flags().object_open())
                {
                    buffer.push_back("{");
                }
                else if (node.entry.flags().array_open())
                {
                    buffer.push_back("[");
                }
                else
                {
                    buffer.push_back(".");
                }
            }
            else
            {
                buffer.push_back(node.entry.key());
            }
            p = node.idx_parent;
        }
        std::reverse(buffer.begin(), buffer.end());

        int cur_col = 0;
        _print_buffer.clear();
        for (const auto elem : buffer)
        {
            _print_buffer.push_back('>');
            ++cur_col;
            auto it = elem.cbegin();
            auto end = elem.cend();
            while (it != end)
            {
                auto old = it;
                int32_t c = utf8::next(it, end);
                int new_col = cur_col + util::is_full_width(c) + 1;
                if (new_col > state.cols())
                {
                    break;
                }
                cur_col = new_col;
                while (old != it)
                {
                    _print_buffer.push_back(*old++);
                }
            }
            if (cur_col > state.cols())
            {
                break;
            }
        }

        move(state.rows() - 2, 0);
        clrtoeol();
        addstr(_print_buffer.c_str());
        mvchgat(state.rows() - 2, 0, -1, A_STANDOUT, 0, nullptr);
    }

    void print_status_bar(const app_state &state, size_t idx_last)
    {
        _print_buffer.clear();
        auto it = std::back_inserter(_print_buffer);
        it = std::format_to(it, "{}-", _view_model.at(_idx_cur).entry.model_line_num());
        if (idx_last != json::INVALID_IDX)
        {
            // Get the rightmost descendent
            auto idx = _view_model.forward(idx_last) - 1;
            it = std::format_to(it, "{}", _view_model.at(idx).entry.model_line_num());
        }
        else
        {
            *it++ = '?';
            *it++ = '?';
        }
        it = std::format_to(it, "/{} - {}", _view_model.at(_view_model.idx_tail() - 1).entry.model_line_num(),
                            _file_name);
        move(state.rows() - 1, 0);
        clrtoeol();
        addstr(_print_buffer.c_str());
        move(state.rows() - 1, 0);
        chgat(-1, A_STANDOUT, 0, nullptr);
    }

    json::view_model load_view_model_from_source(data_source source)
    {
        switch (source)
        {
        case data_source::clipboard:
            return load_view_model_from_clipboard();
        case data_source::pipe:
            return load_view_model_from_stdin();
        default:
            throw std::logic_error("unknown data source type");
        }
    }

    json::view_model load_view_model_from_clipboard()
    {
        _file_name = "<CLIPBOARD>";
        clipboard::get_clipboard_text(_content, simdjson::SIMDJSON_PADDING);
        return json::load(_content);
    }

    json::view_model load_view_model_from_stdin()
    {
        _file_name = "<STDIN>";
        util::read_all_stdin(_content, simdjson::SIMDJSON_PADDING);
        return json::load(_content);
    }

    json::view_model load_view_model_from_file(const std::string &file_path)
    {
        _file_name = std::filesystem::path(file_path).filename();
        auto file_size = std::filesystem::file_size(file_path);
        _content.reserve(file_size + simdjson::SIMDJSON_PADDING);
        _content.resize(file_size);
        {
            std::ifstream in(file_path, std::ios_base::in | std::ios_base::binary);
            in.read(_content.data(), file_size);
            if (in.bad())
            {
                throw std::runtime_error("unable to read file");
            }
        }
        return json::load(_content);
    }

    void scroll_forward(int n)
    {
        for (int i = 0; i < n && _view_model.forward(_idx_cur) < _view_model.idx_tail(); ++i)
        {
            _idx_cur = _view_model.forward(_idx_cur);
        }
    }

    void scroll_backward(int n)
    {
        for (int i = 0; i < n && _idx_cur != 0; ++i)
        {
            _idx_cur = _view_model.backward(_idx_cur);
        }
    }

    inline void scroll_to_top() { _idx_cur = 0; }

    inline void scroll_to_bottom() { _idx_cur = _view_model.backward(_view_model.idx_tail()); }

    inline bool at_top() const { return _idx_cur == 0; }

    inline bool at_bottom() const { return _view_model.forward(_idx_cur) == _view_model.idx_tail(); }

public:
    main_handler(data_source source) : _view_model(load_view_model_from_source(source)) {}
    main_handler(const std::string &file_path) : _view_model(load_view_model_from_file(file_path)) {}
    DISABLE_COPY(main_handler)
    DEFAULT_MOVE(main_handler)

    app_control start(const app_state &state)
    {
        _idx_cur = 0;
        print_json(state);
        return app_control::ok;
    }

    app_control mouse(const app_state &state, const mouse_event &event)
    {
        if (event.left_down())
        {
            auto p = idx_at_line(event.y());
            if (p != json::INVALID_IDX)
            {
                auto &node = _view_model.at(p);
                if (node.entry.flags().collapsible())
                {
                    if (node.collapsed())
                    {
                        _view_model.set_expand(p);
                    }
                    else
                    {
                        _view_model.set_collapse(p);
                    }
                    print_json(state);
                }
            }
        }
        if (event.move())
        {
            if (_row_highlight != event.y())
            {
                if (_row_highlight >= 0)
                {
                    auto p = idx_at_line(_row_highlight);
                    size_t x = p != json::INVALID_IDX ? _view_model.at(p).entry.indent() : 0;
                    if (x < static_cast<size_t>(state.cols()))
                    {
                        mvchgat(_row_highlight, static_cast<int>(x), -1, A_NORMAL, 0, nullptr);
                    }
                }
                _row_highlight = event.y();
                auto p = idx_at_line(_row_highlight);
                size_t x = p != json::INVALID_IDX ? _view_model.at(p).entry.indent() : 0;
                if (x < static_cast<size_t>(state.cols()))
                {
                    mvchgat(event.y(), static_cast<int>(x), -1, A_STANDOUT, 0, nullptr);
                }
                print_breadcrumb(state);
            }
        }
        return app_control::ok;
    }

    app_control resize(const app_state &state)
    {
        print_json(state);
        return app_control::ok;
    }

    app_control key(const app_state &state, int ch)
    {
        switch (ch)
        {
        case '-':
        {
            for (size_t i = 0; i < _view_model.idx_tail(); ++i)
            {
                _view_model.set_collapse(i);
            }
            print_json(state);
            break;
        }
        case '+':
        {
            for (size_t i = 0; i < _view_model.idx_tail(); ++i)
            {
                _view_model.set_expand(i);
            }
            print_json(state);
            break;
        }
        case 'j':
        case KEY_DOWN:
        case '\n':
            if (at_bottom())
            {
                beep();
            }
            else
            {
                scroll_forward(1);
                print_json(state);
            }
            break;
        case 'k':
        case KEY_UP:
            if (at_top())
            {
                beep();
            }
            else
            {
                scroll_backward(1);
                print_json(state);
            }
            break;
        case 'f':
        case ' ':
            if (at_bottom())
            {
                beep();
            }
            else
            {
                scroll_forward(content_screen_height(state));
                print_json(state);
            }
            break;
        case 'c':
        {
            auto p = idx_at_line(_row_highlight);
            if (p != json::INVALID_IDX)
            {
                auto &node = _view_model.at(p);
                if (node.entry.flags().primitive())
                {
                    clipboard::set_clipboard_text(node.entry.value());
                }
                else
                {
                    beep();
                }
            }
            break;
        }
        case 'b':
            if (at_top())
            {
                beep();
            }
            else
            {
                scroll_backward(content_screen_height(state));
                print_json(state);
            }
            break;
        case 'd':
            if (at_bottom())
            {
                beep();
            }
            else
            {
                scroll_forward(content_screen_height(state) / 2);
                print_json(state);
            }
            break;
        case 'u':
            if (at_top())
            {
                beep();
            }
            else
            {
                scroll_backward(content_screen_height(state) / 2);
                print_json(state);
            }
            break;
        case 'g':
            scroll_to_top();
            print_json(state);
            break;
        case 'G':
            scroll_to_bottom();
            print_json(state);
            break;
        case 'q':
            return app_control::stop;
        }
        return app_control::ok;
    }
};

struct cli_options
{
    cli_options() = default;
    DEFAULT_MOVE(cli_options)
    DISABLE_COPY(cli_options)

    std::string input_file;
};

static std::variant<cli_options, int> parse_cli(int argc, char **argv)
{
    cli_options opts;
    CLI::App cli{"beaujson - JSON viewer in your terminal"};
    cli.add_option("file", opts.input_file, "Input JSON file");
    try
    {
        cli.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        return cli.exit(e);
    }
    return opts;
}

static main_handler make_main_handler(const cli_options &opts)
{
    if (opts.input_file.empty())
    {
        if (isatty(STDIN_FILENO))
        {
            return main_handler(data_source::clipboard);
        }
        return main_handler(data_source::pipe);
    }
    return main_handler(opts.input_file);
}

int main(int argc, char **argv)
{
    std::setlocale(LC_ALL, "");

    auto opts_or_ret = parse_cli(argc, argv);
    if (std::holds_alternative<int>(opts_or_ret))
    {
        return std::get<int>(opts_or_ret);
    }
    auto opts = std::move(std::get<cli_options>(opts_or_ret));

    try
    {
        main_app app(make_main_handler(opts));
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Encountered an error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}