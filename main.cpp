#include <csignal>
#include <cstdio>
#include <ncurses.h>

#include "clipboard.hpp"
#include "common.hpp"
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

    inline void call_handler_start(const app_state &state) { _handler.start(state); }
    inline void call_handler_mouse(const app_state &state, const mouse_event &event) { _handler.mouse(state, event); }
    inline void call_handler_resize(const app_state &state) { _handler.resize(state); }

public:
    main_app(Handler &&handler) : _handler(std::move(handler)) { init(); }
    ~main_app() { cleanup(); }
    DISABLE_COPY(main_app)

    void run()
    {
        mouse_event mevent;
        call_handler_start(_state);
        for (;;)
        {
            int c = getch();
            switch (c)
            {
            case KEY_MOUSE:
                if (getmouse(mevent.data()) == OK)
                {
                    call_handler_mouse(_state, mevent);
                }
                break;

            case KEY_RESIZE:
                update_dimensions();
                call_handler_resize(_state);
                break;

            case 3:
                goto out;
            }
        }
    out:;
    }
};

class main_handler
{
private:
    std::vector<char> _clipboard_content;
    simdjson::ondemand::parser _json_parser;

public:
    main_handler() {}
    DISABLE_COPY(main_handler)
    DEFAULT_MOVE(main_handler)

    void start(const app_state &state)
    {
        clipboard::get_clipboard_text(_clipboard_content, simdjson::SIMDJSON_PADDING);
        mvprintw(3, 0, "STRING: size=%ld cap=%ld", _clipboard_content.size(), _clipboard_content.capacity());
        // -1 to the size to exclude null terminator
        simdjson::ondemand::document doc = _json_parser.iterate(
            _clipboard_content.data(), _clipboard_content.size() - 1, _clipboard_content.capacity());
        simdjson::ondemand::value value = doc;
        mvprintw(4, 0, "IS OBJ = %b", value.type().take_value() == simdjson::ondemand::json_type::object);
    }

    void mouse(const app_state &state, const mouse_event &event)
    {
        move(0, 0);
        clrtoeol();
        mvprintw(0, 0,
                 "MOUSE: x=%d y=%d ctrl=%b alt=%b shift=%b move=%b bldown=%b blup=%b brdown=%b brup=%b sup=%b sdown=%b",
                 event.x(), event.y(), event.ctrl(), event.alt(), event.shift(), event.move(), event.left_down(),
                 event.left_up(), event.right_down(), event.right_up(), event.scroll_up(), event.scroll_down());
    }

    void resize(const app_state &state) { mvprintw(2, 0, "RESIZED: rows=%d cols=%d", state.rows(), state.cols()); }
};

int main()
{
    main_app app(main_handler{});
    app.run();
    return 0;
}