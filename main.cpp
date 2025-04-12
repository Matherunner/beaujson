#include <csignal>
#include <cstdio>
#include <ncurses.h>

#include "clipboard.hpp"

#define DISABLE_COPY(A)                                                                                                \
    A(const A &) = delete;                                                                                             \
    A &operator=(const A &) = delete;

class AppState
{
private:
    int _rows = 0;
    int _cols = 0;

public:
    AppState() {}
    ~AppState() {}
    DISABLE_COPY(AppState)

    inline void set_rows(int rows) { _rows = rows; }
    inline void set_cols(int cols) { _cols = cols; }
    inline int rows() const { return _rows; }
    inline int cols() const { return _cols; }
};

class MouseEvent
{
private:
    MEVENT _event;

public:
    MouseEvent() {}
    MouseEvent(const MouseEvent &) = delete;
    MouseEvent &operator=(const MouseEvent &) = delete;

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

template <typename Handler>
class App
{
private:
    Handler _handler;
    AppState _state;

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

    inline void call_handler_mouse(const AppState &state, const MouseEvent &event) { _handler.mouse(state, event); }
    inline void call_handler_resize(const AppState &state) { _handler.resize(state); }

public:
    App(Handler &&handler) : _handler(std::move(handler)) { init(); }
    ~App() { cleanup(); }
    DISABLE_COPY(App)

    void run()
    {
        bool active = true;
        MouseEvent mevent;
        while (active)
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
                _state.set_rows(getmaxy(stdscr));
                _state.set_cols(getmaxx(stdscr));
                call_handler_resize(_state);
                break;

            case 3:
                active = false;
                break;
            }
        }
    }
};

struct MainHandler
{
    void mouse(const AppState &state, const MouseEvent &event)
    {
        move(0, 0);
        clrtoeol();
        mvprintw(0, 0,
                 "MOUSE: x=%d y=%d ctrl=%b alt=%b shift=%b move=%b bldown=%b blup=%b brdown=%b brup=%b sup=%b sdown=%b",
                 event.x(), event.y(), event.ctrl(), event.alt(), event.shift(), event.move(), event.left_down(),
                 event.left_up(), event.right_down(), event.right_up(), event.scroll_up(), event.scroll_down());
    }

    void resize(const AppState &state) {}
};

int main()
{
    App app(MainHandler{});
    app.run();
    return 0;
}