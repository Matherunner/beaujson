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
    void read_all_stdin(std::vector<char> &content, size_t padding_size);

    inline bool is_full_width(int32_t ch)
    {
        if (static_cast<size_t>(ch) < east_asian_width::TABLE.size())
        {
            return east_asian_width::TABLE[ch];
        }
        return false;
    }
}