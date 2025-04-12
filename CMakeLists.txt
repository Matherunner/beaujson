cmake_minimum_required(VERSION 3.16)
project(BeauJson LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Disable unnecessary features in clip library
set(CLIP_ENABLE_IMAGE OFF CACHE BOOL "" FORCE)
set(CLIP_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLIP_TESTS OFF CACHE BOOL "" FORCE)
set(CLIP_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(
    vendor/clip
)

add_executable(beaujson
    main.cpp
    clipboard.cpp
)

find_package(Curses REQUIRED)

include_directories(${CURSES_INCLUDE_DIR})

target_link_libraries(beaujson PRIVATE
    clip
    ${CURSES_LIBRARIES}
)
