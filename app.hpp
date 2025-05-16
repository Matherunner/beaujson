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
    MEVENT _event;

public:
    mouse_event() : _event() {}
    DISABLE_COPY(mouse_event)

    MEVENT *data() { return &_event; }
    bool left_down() const { return _event.bstate & BUTTON1_PRESSED; }
    bool mid_down() const { return _event.bstate & BUTTON2_PRESSED; }
    bool right_down() const { return _event.bstate & BUTTON3_PRESSED; }
    bool move() const { return _event.bstate & REPORT_MOUSE_POSITION; }
    bool scroll_up() const { return _event.bstate & BUTTON4_PRESSED; }
    bool scroll_down() const { return _event.bstate & BUTTON5_PRESSED; }
    bool ctrl() const { return _event.bstate & BUTTON_CTRL; }
    bool alt() const { return _event.bstate & BUTTON_ALT; }
    bool shift() const { return _event.bstate & BUTTON_SHIFT; }
    int x() const { return _event.x; }
    int y() const { return _event.y; }
};

enum class app_control : int
{
    ok,
    stop,
};

class app_state
{
    int _rows = 0;
    int _cols = 0;

public:
    app_state() {}
    ~app_state() {}
    DISABLE_COPY(app_state)

    void set_rows(const int rows) { _rows = rows; }
    void set_cols(const int cols) { _cols = cols; }
    int rows() const { return _rows; }
    int cols() const { return _cols; }
};

template <typename Handler>
class main_app
{
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

    static void cleanup()
    {
        disable_mouse_move();
        endwin();
    }

    static void enable_mouse_move()
    {
        puts("\033[?1003h");
        fflush(stdout);
    }

    static void disable_mouse_move()
    {
        puts("\033[?1003l");
        fflush(stdout);
    }

    void update_dimensions()
    {
        _state.set_rows(getmaxy(stdscr));
        _state.set_cols(getmaxx(stdscr));
    }

    app_control call_handler_start(const app_state &state) { return _handler.start(state); }
    app_control call_handler_mouse(const app_state &state, const mouse_event &event)
    {
        return _handler.mouse(state, event);
    }
    app_control call_handler_resize(const app_state &state) { return _handler.resize(state); }
    app_control call_handler_key(const app_state &state, const int c) { return _handler.key(state, c); }

public:
    explicit main_app(Handler &&handler) : _handler(std::move(handler)) { init(); }
    ~main_app() { cleanup(); }
    DISABLE_COPY(main_app)

    void run()
    {
        mouse_event event;
        app_control ret = call_handler_start(_state);
        if (ret == app_control::stop)
        {
            return;
        }
        for (;;)
        {
            switch (const int c = getch())
            {
            case KEY_MOUSE:
                if (getmouse(event.data()) == OK)
                {
                    ret = call_handler_mouse(_state, event);
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
