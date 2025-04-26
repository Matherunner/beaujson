cmake_minimum_required(VERSION 3.16)
project(BeauJson LANGUAGES CXX)

cmake_policy(SET CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

# Disable unnecessary features in clip library
set(CLIP_ENABLE_IMAGE OFF CACHE BOOL "" FORCE)
set(CLIP_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLIP_TESTS OFF CACHE BOOL "" FORCE)
set(CLIP_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(vendor/clip)
add_subdirectory(vendor/simdjson)
add_subdirectory(vendor/catch2)

if(APPLE)
    set(NCURSES_LIBRARIES "/opt/homebrew/opt/ncurses/lib/libncursesw.a")
    set(NCURSES_INCLUDE_DIR "/opt/homebrew/opt/ncurses/include")
    set(CLIP_LIBRARIES "clip")
elseif(UNIX)
    set(NCURSES_LIBRARIES "ncursesw" "tinfo")
    set(CLIP_LIBRARIES "Xau" "Xdmcp" "clip")
endif()

include_directories(
    ${NCURSES_INCLUDE_DIR}
    vendor/CLI11
    vendor/utfcpp/source
)

add_executable(tests
    json.cpp
    json_test.cpp
    util.cpp
    util_test.cpp
    east_asian_width.cpp
)

target_link_libraries(tests PRIVATE
    simdjson
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(tests)

add_executable(beaujson
    main.cpp
    json.cpp
    clipboard.cpp
    util.cpp
    east_asian_width.cpp
)

if(CMAKE_BUILD_TYPE MATCHES "Release")
    if(UNIX AND NOT APPLE)
        # This doesn't work on Apple clang, and we don't really need it
        target_link_options(beaujson PRIVATE -static)
    endif()

    add_definitions(-DNDEBUG)

    set_property(TARGET beaujson PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET clip PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET simdjson PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

target_compile_options(beaujson PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wold-style-cast
        -Wnull-dereference
        -Wconversion
        -Wuninitialized
        -Wmaybe-uninitialized
        -Wnonnull
    >
)

target_link_libraries(beaujson PRIVATE
    simdjson
    ${CLIP_LIBRARIES}
    ${NCURSES_LIBRARIES}
)
