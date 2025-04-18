#include <csignal>
#include <cstdio>
#include <format>
#include <ncurses.h>

#include "clipboard.hpp"
#include "common.hpp"
#include "json.hpp"
#include "simdjson.h"

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
    std::optional<json::view_model> _view_model;
    json::view_model_node *_view_model_cur;
    int _row_highlight = -1;

    void print_json(int rows)
    {
        erase();
        auto *p = _view_model_cur;
        int i = 0;
        for (; i < rows && p != _view_model->tail(); ++i)
        {
            std::string indent;
            for (int j = 0; j < p->entry.indent; ++j)
            {
                indent += ' ';
            }
            // TODO: don't allocate strings here!
            auto key = p->entry.key.has_value() ? std::string(p->entry.key.value()) + ": " : "";
            auto value = std::string(p->entry.value);
            mvprintw(i, 0, "%s%s%s", indent.c_str(), key.c_str(), value.c_str());
            p = p->collapsed ? p->skip : p->next;
        }
        attr_on(A_BOLD, nullptr);
        for (; i < rows; ++i)
        {
            mvaddstr(i, 0, "~");
        }
        attr_off(A_BOLD, nullptr);
        if (_row_highlight >= 0)
        {
            mvchgat(_row_highlight, 0, -1, A_STANDOUT, 0, nullptr);
        }
    }

public:
    main_handler() {}
    DISABLE_COPY(main_handler)
    DEFAULT_MOVE(main_handler)

    app_control start(const app_state &state)
    {
        std::vector<char> _clipboard_content;
        clipboard::get_clipboard_text(_clipboard_content, simdjson::SIMDJSON_PADDING);
        _view_model = json::load(_clipboard_content);
        _view_model_cur = _view_model->head();
        print_json(state.rows());
        return app_control::ok;
    }

    app_control mouse(const app_state &state, const mouse_event &event)
    {
        if (event.left_down())
        {
            auto *p = _view_model_cur;
            int i = 0;
            for (; i < state.rows() && i < event.y() && p != _view_model->tail(); ++i)
            {
                p = p->collapsed ? p->skip : p->next;
            }
            if (json::is_collapsible(p->entry.kind))
            {
                p->collapsed = !p->collapsed;
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
        mvprintw(2, 0, "RESIZED: rows=%d cols=%d", state.rows(), state.cols());
        return app_control::ok;
    }

    app_control key(const app_state &state, int ch)
    {
        switch (ch)
        {
        case 'j':
        case KEY_DOWN:
            if (_view_model_cur->next != _view_model->tail())
            {
                _view_model_cur = _view_model_cur->next;
                print_json(state.rows());
            }
            else
            {
                beep();
            }
            break;
        case 'k':
        case KEY_UP:
            if (_view_model_cur != _view_model->head())
            {
                _view_model_cur = _view_model_cur->prev;
                print_json(state.rows());
            }
            else
            {
                beep();
            }
            break;
        case 'q':
            return app_control::stop;
        }
        return app_control::ok;
    }
};

int main()
{
    main_app app(main_handler{});
    app.run();
    return 0;
}