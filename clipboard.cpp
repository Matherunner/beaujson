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

#include "clipboard.hpp"
#include "vendor/clip/clip.h"

void clipboard::get_clipboard_text(std::vector<char> &content, size_t padding_size)
{
    clip::get_text(content, padding_size);
}

void clipboard::set_clipboard_text(std::string_view s) { clip::set_text(s); }
