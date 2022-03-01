cmake_minimum_required(VERSION 3.1)

# This is all hastily thrown together, will improve

project(asserts)

add_library(asserts_library_warnings INTERFACE)
target_compile_options(asserts_library_warnings
    INTERFACE -Wall -Wextra -Werror=return-type)

# there should be an option to build either shared or static library
# and the client should choose one of the options at the beginning
# https://cmake.org/cmake/help/latest/guide/tutorial/Selecting%20Static%20or%20Shared%20Libraries.html
add_library(assert src/assert.cpp)
target_include_directories(assert PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>)

set_target_properties(assert
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED TRUE
        CXX_EXTENSIONS OFF)

target_link_libraries(assert PRIVATE asserts_library_warnings)

if (MSVC)
    target_link_libraries(assert PRIVATE dbghelp)
endif ()

add_executable(demo demo.cpp foo.cpp bar.cpp)
target_link_libraries(demo PRIVATE assert)

set_target_properties(demo
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED TRUE
        CXX_EXTENSIONS OFF)

target_link_libraries(demo PRIVATE asserts_library_warnings)
