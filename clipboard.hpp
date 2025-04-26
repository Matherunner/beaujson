#pragma once

#include <string_view>
#include <vector>

namespace clipboard
{
    void get_clipboard_text(std::vector<char> &content, size_t padding_size);
    void set_clipboard_text(std::string_view s);
}
