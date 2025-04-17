#include "json.hpp"
#include "simdjson.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace testutil
{
    std::vector<char> load_file(const char *path)
    {
        auto file_size = std::filesystem::file_size(path);
        std::ifstream in(path, std::ios_base::in | std::ios_base::binary);
        std::vector<char> data;
        data.reserve(file_size + simdjson::SIMDJSON_PADDING);
        data.resize(file_size);
        in.read(data.data(), file_size);
        CHECK_FALSE(in.bad());
        return data;
    }
}

TEST_CASE("Factorials are computed", "[factorial]")
{
    auto data = testutil::load_file("../data/file1.json");
    auto view_model = json::load(data);
    view_model.debug_print();
    auto *cur = view_model.head();
    while (cur->entry.key != "address")
    {
        cur = cur->next;
    }
    cur->collapsed = true;
    view_model.debug_print();
}