#include <clocale>
#include <csignal>
#include <cstdio>
#include <format>
#include <variant>

#include <fcntl.h>
#include <ncurses.h>
#include <unistd.h>

#include "clipboard.hpp"
#include "json.hpp"
#include "simdjson.h"
#include "utf8.h"
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
    inline bool mid_down() const { return _event.bstate & BUTTON2_PRESSED; }
    inline bool right_down() const { return _event.bstate & BUTTON3_PRESSED; }
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

class tty_file
{
private:
    int _fd = -1;
    FILE *_file = nullptr;

    void _open()
    {
        _fd = open("/dev/tty", O_RDWR);
        if (_fd < 0)
        {
            throw std::runtime_error("unable to open tty");
        }
        _file = fdopen(_fd, "r+");
        if (!_file)
        {
            throw std::runtime_error("unable to open tty fd");
        }
    }

    void _cleanup()
    {
        if (_fd >= 0)
        {
            close(_fd);
        }
        if (_file)
        {
            std::fclose(_file);
        }
    }

public:
    tty_file() { _open(); }
    ~tty_file() { _cleanup(); }
    tty_file &operator=(tty_file &&other)
    {
        std::swap(_fd, other._fd);
        std::swap(_file, other._file);
        return *this;
    }
    tty_file(tty_file &&other) { *this = std::move(other); }
    DISABLE_COPY(tty_file)

    inline FILE *file() const { return _file; }
};

template <typename Handler>
class main_app
{
private:
    // ALL_MOUSE_EVENTS doesn't work property on Terminal.app on macOS as the clicks aren't registered consistently when
    // the *_CLICKED events are included in the mask. Terminal.app in general has poor mouse support.
    static constexpr unsigned MOUSE_MASK =
        BUTTON1_PRESSED | BUTTON2_PRESSED | BUTTON3_PRESSED | BUTTON4_PRESSED | BUTTON5_PRESSED | REPORT_MOUSE_POSITION;

    Handler _handler;
    app_state _state;
    tty_file _tty_file;

    void init()
    {
        // Use /dev/tty instead of stdin so that we can exhaust stdin and still read from user input via the terminal.
        newterm(nullptr, _tty_file.file(), _tty_file.file());
        cbreak();
        noecho();
        raw();
        mouseinterval(0);
        keypad(stdscr, TRUE);
        mousemask(MOUSE_MASK, nullptr);
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
        puts("\033[?1003h");
        fflush(stdout);
    }

    void disable_mouse_move()
    {
        puts("\033[?1003l");
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

            case -1:
                throw std::runtime_error("unable to read input");

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
    json::view_model_node *_view_model_cur;
    int _row_highlight = -1;

    inline static int get_key_width_limit(const app_state &state)
    {
        // Account for "<ellipses character>: "
        return state.cols() / 3 - 3;
    }

    inline static int get_value_max_col(const app_state &state)
    {
        // Account for "<ellipses character> [+]"
        return state.cols() - 5;
    }

    void print_json(const app_state &state)
    {
        erase();

        if (state.cols() < MINIMUM_WIDTH)
        {
            return;
        }

        const int row_end = state.rows() - 2;
        auto *p = _view_model_cur;
        auto *last = _view_model_cur;
        int i = 0;
        for (; i < row_end && p != _view_model.tail(); ++i)
        {
            last = p;
            _print_buffer.clear();
            int cur_col = p->entry.indent;
            for (size_t j = 0; j < p->entry.indent; ++j)
            {
                _print_buffer.push_back(' ');
            }
            if (p->entry.flags.has_key())
            {
                auto it = p->entry.key.cbegin();
                auto end = p->entry.key.cend();
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
                auto it = p->entry.value.cbegin();
                auto end = p->entry.value.cend();
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
                cur_col += 4;
            }
            mvaddstr(i, 0, _print_buffer.c_str());
            p = p->forward();
        }

        attr_on(A_BOLD, nullptr);
        for (; i < row_end; ++i)
        {
            mvaddstr(i, 0, "~");
        }
        attr_off(A_BOLD, nullptr);

        if (_row_highlight >= 0)
        {
            mvchgat(_row_highlight, 0, -1, A_STANDOUT, 0, nullptr);
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

        auto *p = _view_model_cur;
        for (int i = 0; i < _row_highlight; ++i)
        {
            if (!p)
            {
                return;
            }
            p = p->forward();
        }

        std::vector<std::string_view> buffer;
        while (p)
        {
            if (p->entry.key.empty())
            {
                if (p->entry.flags.is_object_open())
                {
                    buffer.push_back("{");
                }
                else if (p->entry.flags.is_array_open())
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
                buffer.push_back(p->entry.key);
            }
            p = p->parent;
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

    void print_status_bar(const app_state &state, const json::view_model_node *last)
    {
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
    main_handler(data_source source) : _view_model(load_view_model_from_source(source)) {}
    main_handler(const std::string &file_path) : _view_model(load_view_model_from_file(file_path)) {}
    DISABLE_COPY(main_handler)
    DEFAULT_MOVE(main_handler)

    app_control start(const app_state &state)
    {
        _view_model_cur = _view_model.head();
        print_json(state);
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
                print_json(state);
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
                _row_highlight = event.y();
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
            // Collapse all
            auto *p = _view_model.head();
            while (p != _view_model.tail())
            {
                p->set_collapse();
                p = p->next;
            }
            print_json(state);
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
                scroll_forward(state.rows());
                print_json(state);
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
                scroll_forward(state.rows() / 2);
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
                scroll_backward(state.rows() / 2);
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