#pragma once

#include <bitset>

namespace east_asian_width
{
    // constexpr doesn't work because the compiler refuses to compile it due to the size
    extern const std::bitset<262142> TABLE;
}
