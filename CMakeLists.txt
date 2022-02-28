# Since CMake 2.8 will be deprecated, maybe we should set this version to at leat 3.1?
cmake_minimum_required(VERSION 3.1)

# This is all hastily thrown together, will improve

project(asserts)

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
        CXX_EXTENSIONS OFF
)

if (MSVC)
    target_link_libraries(assert PRIVATE dbghelp)
endif ()

# Let's put it aside for a moment :) We will deal with it later
# I had a problem with compilation of this part

#project(demo)
#set(CMAKE_CXX_STANDARD 17)
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
#add_executable(demo demo.cpp foo.cpp bar.cpp)
#target_link_libraries(demo assert_static)
#target_compile_options(demo PRIVATE "/DASSERT_FAIL=custom_fail" "/W4")
