#pragma once

#include <string_view>

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
}