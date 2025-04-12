#include "clipboard.hpp"
#include "vendor/clip/clip.h"

void clipboard::get_clipboard_text(std::string &content)
{
    clip::get_text(content);
}