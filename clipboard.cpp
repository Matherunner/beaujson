#include "clipboard.hpp"
#include "vendor/clip/clip.h"

void clipboard::get_clipboard_text(std::vector<char> &content, size_t padding_size)
{
    clip::get_text(content, padding_size);
}

void clipboard::set_clipboard_text(std::string_view s) { clip::set_text(s); }
