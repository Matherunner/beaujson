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

#include <cstdint>
#include <string_view>
#include <vector>

#include "east_asian_width.hpp"

#define DISABLE_COPY(A)                                                                                                \
    A(const A &) = delete;                                                                                             \
    A &operator=(const A &) = delete;

#define DEFAULT_MOVE(A)                                                                                                \
    A(A &&) = default;                                                                                                 \
    A &operator=(A &&) = default;

namespace util
{
    struct noncopyable
    {
        noncopyable() = default;
        DEFAULT_MOVE(noncopyable)
    };

    std::string_view trim_space(std::string_view &&input);
    std::string_view &trim_space(std::string_view &input);
    void read_all_stdin(std::vector<char> &content, size_t padding_size);

    inline bool is_full_width(const uint32_t ch)
    {
        if (static_cast<size_t>(ch) < east_asian_width::TABLE.size())
        {
            return east_asian_width::TABLE[ch];
        }
        return false;
    }
}