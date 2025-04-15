#include "util.hpp"

static const char *const JSON_WHITESPACES = " \t\r\n";

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
