#include "util.hpp"
#include <catch2/catch_test_macros.hpp>

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