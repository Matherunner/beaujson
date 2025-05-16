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

#include "util.hpp"
#include <cstdio>

constexpr auto JSON_WHITESPACES = " \t\r\n";

std::string_view util::trim_space(std::string_view &&input)
{
    auto idx = input.find_last_not_of(JSON_WHITESPACES);
    if (idx != std::string_view::npos)
    {
        input.remove_suffix(input.length() - idx - 1);
    }
    else
    {
        input.remove_suffix(input.length());
    }
    idx = input.find_first_not_of(JSON_WHITESPACES);
    if (idx != std::string_view::npos)
    {
        input.remove_prefix(idx);
    }
    return input;
}

void util::read_all_stdin(std::vector<char> &content, const size_t padding_size)
{
    content.reserve(1 << 20);
    size_t bytes;
    char temp[1 << 20];
    while ((bytes = std::fread(temp, 1, sizeof(temp), stdin)) > 0)
    {
        content.insert(content.end(), temp, temp + bytes);
    }
    const size_t min_size = content.size() + padding_size;
    if (content.capacity() < min_size)
    {
        content.reserve(min_size);
    }
}
