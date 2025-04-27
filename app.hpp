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

#pragma once

#include <ncurses.h>

#include "tty_file.hpp"
#include "util.hpp"

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
