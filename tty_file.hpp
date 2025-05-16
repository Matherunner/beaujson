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

#include <cstdio>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

#include "util.hpp"

class tty_file
{
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

    void _cleanup() const
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

    FILE *file() const { return _file; }
};
