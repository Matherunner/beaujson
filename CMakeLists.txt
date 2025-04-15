cmake_minimum_required(VERSION 3.16)
project(BeauJson LANGUAGES CXX)

cmake_policy(SET CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Disable unnecessary features in clip library
set(CLIP_ENABLE_IMAGE OFF CACHE BOOL "" FORCE)
set(CLIP_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLIP_TESTS OFF CACHE BOOL "" FORCE)
set(CLIP_INSTALL OFF CACHE BOOL "" FORCE)

find_package(Catch2 3 REQUIRED)

add_executable(tests
    json.cpp
    json_test.cpp
)

target_link_libraries(tests PRIVATE
    simdjson
    Catch2::Catch2WithMain
)

add_subdirectory(vendor/clip)
add_subdirectory(vendor/simdjson)

add_executable(beaujson
    main.cpp
    json.cpp
    clipboard.cpp
)

find_package(Curses REQUIRED)

include_directories(${CURSES_INCLUDE_DIR})

if(CMAKE_BUILD_TYPE MATCHES "Release")
    set_property(TARGET beaujson PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET clip PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET simdjson PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

target_link_libraries(beaujson PRIVATE
    clip
    simdjson
    ${CURSES_LIBRARIES}
)
