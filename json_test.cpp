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
        in.read(data.data(), static_cast<std::streamsize>(file_size));
        CHECK_FALSE(in.bad());
        return data;
    }
}

TEST_CASE("Factorials are computed", "[factorial]")
{
    // auto data = testutil::load_file("../data/file2.json");
    // auto view_model = json::load(data);
    // view_model.debug_print();
    // auto *cur = view_model.head();
    // while (cur->entry.key != "address")
    // {
    //     cur = cur->next;
    // }
    // cur->collapsed = true;
    // view_model.debug_print();
}

TEST_CASE("view_entry size", "[view_entry]") { REQUIRE(sizeof(json::view_entry) == 56); }