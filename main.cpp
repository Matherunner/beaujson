#include <cstdio>
#include <csignal>
#include <ncurses.h>

#include "clipboard.hpp"

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

int main()
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

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    char buf[1024];

    for (;;)
    {
        int ch = getch();
        if (ch == KEY_MOUSE)
        {
            MEVENT event;
            if (getmouse(&event) == OK)
            {
                mvprintw(0, 0, "MOUSE: x=%d y=%d z=%d id=%d bstate=%d", event.x, event.y, event.z, event.id, event.bstate);
            }
        }
        else if (ch == KEY_RESIZE)
        {
            getmaxyx(stdscr, rows, cols);
            mvprintw(2, 0, "RESIZE: rows=%d cols=%d", rows, cols);
        }
        else if (ch == 3)
        {
            break;
        }
        else if (ch == 'v')
        {
            std::string content;
            clipboard::get_clipboard_text(content);
            mvaddstr(5, 0, content.c_str());
        }
        else
        {
            mvprintw(1, 0, "CHAR: %d", ch);
        }
        refresh();
    }

    disable_mouse_move();
    endwin();

    return 0;
}