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

#include "utf8.h"
#include "util.hpp"
#include <catch2/catch_test_macros.hpp>

namespace
{
    int32_t get_code_point(const std::string &s)
    {
        auto it = s.cbegin();
        return utf8::next(it, s.cend());
    }
}

TEST_CASE("Trim space successfully", "[trim_space]")
{
    REQUIRE(util::trim_space("  abc  ") == "abc");
    REQUIRE(util::trim_space("abc  ") == "abc");
    REQUIRE(util::trim_space("   abc") == "abc");
}

TEST_CASE("Trim all-space string successfully", "[trim_space]")
{
    REQUIRE(util::trim_space("     ") == "");
    REQUIRE(util::trim_space("  \t\n    ") == "");
}

TEST_CASE("Identifies full-width characters successfully", "[is_full_width]")
{
    REQUIRE(util::is_full_width(get_code_point("这")));
    REQUIRE(util::is_full_width(get_code_point("是")));
    REQUIRE(util::is_full_width(get_code_point("一")));
    REQUIRE(util::is_full_width(get_code_point("个")));
    REQUIRE(util::is_full_width(get_code_point("测")));
    REQUIRE(util::is_full_width(get_code_point("试")));
    REQUIRE(util::is_full_width(get_code_point("새")));
    REQUIRE(util::is_full_width(get_code_point("로")));
    REQUIRE(util::is_full_width(get_code_point("고")));
    REQUIRE_FALSE(util::is_full_width(get_code_point("t")));
    REQUIRE_FALSE(util::is_full_width(get_code_point("0")));
    REQUIRE_FALSE(util::is_full_width(get_code_point("z")));
    REQUIRE_FALSE(util::is_full_width(get_code_point("a")));
    REQUIRE_FALSE(util::is_full_width(get_code_point("]")));
    REQUIRE_FALSE(util::is_full_width(get_code_point(".")));
    REQUIRE_FALSE(util::is_full_width(get_code_point(" ")));
}