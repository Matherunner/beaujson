#pragma once

#define DISABLE_COPY(A)                                                                                                \
    A(const A &) = delete;                                                                                             \
    A &operator=(const A &) = delete;

#define DEFAULT_MOVE(A)                                                                                                \
    A(A &&) = default;                                                                                                 \
    A &operator=(A &&) = default;
