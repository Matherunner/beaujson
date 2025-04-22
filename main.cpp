#include <clocale>
#include <csignal>
#include <cstdio>
#include <format>
#include <variant>

#include <ncurses.h>

#include "clipboard.hpp"
#include "json.hpp"
#include "simdjson.h"
#include "vendor/CLI11/CLI11.hpp"

class mouse_event
{
private:
    MEVENT _event;

public:
    mouse_event() {}
    DISABLE_COPY(mouse_event)

    inline MEVENT *data() { return &_event; }
    inline bool left_down() const { return _event.bstate & BUTTON1_PRESSED; }
    inline bool left_up() const { return _event.bstate & BUTTON1_RELEASED; }
    inline bool mid_down() const { return _event.bstate & BUTTON2_PRESSED; }
    inline bool mid_up() const { return _event.bstate & BUTTON2_RELEASED; }
    inline bool right_down() const { return _event.bstate & BUTTON3_PRESSED; }
    inline bool right_up() const { return _event.bstate & BUTTON3_RELEASED; }
    inline bool move() const { return _event.bstate & REPORT_MOUSE_POSITION; }
    inline bool scroll_up() const { return _event.bstate & BUTTON4_PRESSED; }
    inline bool scroll_down() const { return _event.bstate & BUTTON5_PRESSED; }
    inline bool ctrl() const { return _event.bstate & BUTTON_CTRL; }
    inline bool alt() const { return _event.bstate & BUTTON_ALT; }
    inline bool shift() const { return _event.bstate & BUTTON_SHIFT; }
    inline int x() const { return _event.x; }
    inline int y() const { return _event.y; }
};

enum class app_control : int
{
    ok,
    stop,
};

class app_state
{
private:
    int _rows = 0;
    int _cols = 0;

public:
    app_state() {}
    ~app_state() {}
    DISABLE_COPY(app_state)

    inline void set_rows(int rows) { _rows = rows; }
    inline void set_cols(int cols) { _cols = cols; }
    inline int rows() const { return _rows; }
    inline int cols() const { return _cols; }
};

template <typename Handler>
class main_app
{
private:
    Handler _handler;
    app_state _state;

    void init()
    {
        initscr();
        cbreak();
        noecho();
        raw();
        mouseinterval(0);
        keypad(stdscr, TRUE);
        mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
        curs_set(0);
        enable_mouse_move();
        update_dimensions();
    }

    void cleanup()
    {
        disable_mouse_move();
        endwin();
    }

    void enable_mouse_move()
    {
        puts("\e[?1003h");
        fflush(stdout);
    }

    void disable_mouse_move()
    {
        puts("\e[?1003l");
        fflush(stdout);
    }

    void update_dimensions()
    {
        _state.set_rows(getmaxy(stdscr));
        _state.set_cols(getmaxx(stdscr));
    }

    inline app_control call_handler_start(const app_state &state) { return _handler.start(state); }
    inline app_control call_handler_mouse(const app_state &state, const mouse_event &event)
    {
        return _handler.mouse(state, event);
    }
    inline app_control call_handler_resize(const app_state &state) { return _handler.resize(state); }
    inline app_control call_handler_key(const app_state &state, int c) { return _handler.key(state, c); }

public:
    main_app(Handler &&handler) : _handler(std::move(handler)) { init(); }
    ~main_app() { cleanup(); }
    DISABLE_COPY(main_app)

    void run()
    {
        mouse_event mevent;
        app_control ret = call_handler_start(_state);
        if (ret == app_control::stop)
        {
            return;
        }
        for (;;)
        {
            int c = getch();
            switch (c)
            {
            case KEY_MOUSE:
                if (getmouse(mevent.data()) == OK)
                {
                    ret = call_handler_mouse(_state, mevent);
                    if (ret == app_control::stop)
                    {
                        return;
                    }
                }
                break;

            case KEY_RESIZE:
                update_dimensions();
                ret = call_handler_resize(_state);
                if (ret == app_control::stop)
                {
                    return;
                }
                break;

            case 3:
                goto out;

            default:
                ret = call_handler_key(_state, c);
                if (ret == app_control::stop)
                {
                    return;
                }
            }
        }
    out:;
    }
};

class main_handler
{
private:
    std::string _file_name;
    std::string _print_buffer;
    std::vector<char> _content;
    json::view_model _view_model;
    json::view_model_node *_view_model_cur;
    int _row_highlight = -1;

    void print_json(int rows)
    {
        erase();

        auto *p = _view_model_cur;
        auto *last = _view_model_cur;
        int i = 0;
        for (; i < rows - 1 && p != _view_model.tail(); ++i)
        {
            last = p;
            _print_buffer.clear();
            for (int j = 0; j < p->entry.indent; ++j)
            {
                _print_buffer.push_back(' ');
            }
            if (p->entry.flags.has_key())
            {
                _print_buffer += p->entry.key;
                _print_buffer += ": ";
            }
            _print_buffer += p->entry.value;
            if (p->entry.flags.collapsible())
            {
                if (p->collapsed())
                {
                    _print_buffer += " [+]";
                }
                else
                {
                    _print_buffer += " [-]";
                }
            }
            mvaddstr(i, 0, _print_buffer.c_str());
            p = p->forward();
        }

        attr_on(A_BOLD, nullptr);
        for (; i < rows - 1; ++i)
        {
            mvaddstr(i, 0, "~");
        }
        attr_off(A_BOLD, nullptr);

        if (_row_highlight >= 0)
        {
            mvchgat(_row_highlight, 0, -1, A_STANDOUT, 0, nullptr);
        }

        _print_buffer.clear();
        auto it = std::back_inserter(_print_buffer);
        it = std::format_to(it, "{}-", _view_model_cur->entry.model_line_num);
        if (last)
        {
            it = std::format_to(it, "{}", last->entry.model_line_num);
        }
        else
        {
            *it++ = '?';
            *it++ = '?';
        }
        it = std::format_to(it, "/{} - {}", _view_model.tail()->prev->entry.model_line_num, _file_name);
        mvaddstr(rows - 1, 0, _print_buffer.c_str());
        mvchgat(rows - 1, 0, -1, A_STANDOUT, 0, nullptr);
    }

    json::view_model load_view_model_from_clipboard()
    {
        _file_name = "<CLIPBOARD>";
        clipboard::get_clipboard_text(_content, simdjson::SIMDJSON_PADDING);
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
        for (int i = 0; i < n && _view_model_cur->forward() != _view_model.tail(); ++i)
        {
            _view_model_cur = _view_model_cur->forward();
        }
    }

    void scroll_backward(int n)
    {
        for (int i = 0; i < n && _view_model_cur != _view_model.head(); ++i)
        {
            _view_model_cur = _view_model_cur->backward();
        }
    }

    inline void scroll_to_top() { _view_model_cur = _view_model.head(); }

    inline void scroll_to_bottom() { _view_model_cur = _view_model.tail()->backward(); }

    inline bool at_top() const { return _view_model_cur == _view_model.head(); }

    inline bool at_bottom() const { return _view_model_cur->forward() == _view_model.tail(); }

public:
    main_handler() : _view_model(load_view_model_from_clipboard()) {}
    main_handler(const std::string &file_path) : _view_model(load_view_model_from_file(file_path)) {}
    DISABLE_COPY(main_handler)
    DEFAULT_MOVE(main_handler)

    app_control start(const app_state &state)
    {
        _view_model_cur = _view_model.head();
        print_json(state.rows());
        return app_control::ok;
    }

    app_control mouse(const app_state &state, const mouse_event &event)
    {
        if (event.left_down())
        {
            auto *p = _view_model_cur;
            int i = 0;
            for (; i < state.rows() - 1 && i < event.y() && p != _view_model.tail(); ++i)
            {
                p = p->forward();
            }
            if (p->entry.flags.collapsible())
            {
                if (p->collapsed())
                {
                    p->set_expand();
                }
                else
                {
                    p->set_collapse();
                }
                print_json(state.rows());
            }
        }
        if (event.move())
        {
            if (_row_highlight != event.y())
            {
                if (_row_highlight >= 0)
                {
                    mvchgat(_row_highlight, 0, -1, A_NORMAL, 0, nullptr);
                }
                mvchgat(event.y(), 0, -1, A_STANDOUT, 0, nullptr);
            }
            _row_highlight = event.y();
        }
        return app_control::ok;
    }

    app_control resize(const app_state &state)
    {
        print_json(state.rows());
        return app_control::ok;
    }

    app_control key(const app_state &state, int ch)
    {
        switch (ch)
        {
        case '-':
        {
            // Collapse all
            auto *p = _view_model.head();
            while (p != _view_model.tail())
            {
                p->set_collapse();
                p = p->next;
            }
            print_json(state.rows());
            break;
        }
        case '+':
        {
            // Expand all
            auto *p = _view_model.head();
            while (p != _view_model.tail())
            {
                p->set_expand();
                p = p->next;
            }
            print_json(state.rows());
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
                print_json(state.rows());
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
                print_json(state.rows());
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
                scroll_forward(state.rows());
                print_json(state.rows());
            }
            break;
        case 'b':
            if (at_top())
            {
                beep();
            }
            else
            {
                scroll_backward(state.rows());
                print_json(state.rows());
            }
            break;
        case 'd':
            if (at_bottom())
            {
                beep();
            }
            else
            {
                scroll_forward(state.rows() / 2);
                print_json(state.rows());
            }
            break;
        case 'u':
            if (at_top())
            {
                beep();
            }
            else
            {
                scroll_backward(state.rows() / 2);
                print_json(state.rows());
            }
            break;
        case 'g':
            scroll_to_top();
            print_json(state.rows());
            break;
        case 'G':
            scroll_to_bottom();
            print_json(state.rows());
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
        return main_handler();
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

    main_app app(make_main_handler(opts));
    app.run();
    return 0;
}