#include "util.hpp"
#include <cstdio>

constexpr const char *JSON_WHITESPACES = " \t\r\n";

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

void util::read_all_stdin(std::vector<char> &content, size_t padding_size)
{
    content.reserve(1 << 20);
    size_t bytes;
    char temp[1 << 20];
    while ((bytes = std::fread(temp, 1, sizeof(temp), stdin)) > 0)
    {
        content.insert(content.end(), temp, temp + bytes);
    }
    size_t min_size = content.size() + padding_size;
    if (content.capacity() < min_size)
    {
        content.reserve(min_size);
    }
}
