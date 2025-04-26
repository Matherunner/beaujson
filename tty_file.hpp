#pragma once

#include <cstdio>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

#include "util.hpp"

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
    DEFAULT_MOVE(tty_file)
    DISABLE_COPY(tty_file)

    inline FILE *file() const { return _file; }
};
