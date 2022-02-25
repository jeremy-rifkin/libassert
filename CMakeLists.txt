cmake_minimum_required(VERSION 2.8)

# This is all hastily thrown together, will improve

project(asserts)
set(CMAKE_CXX_STANDARD 17)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(assert SHARED src/assert.cpp)
target_link_libraries(assert dbghelp)
add_library(assert_static STATIC src/assert.cpp)
target_link_libraries(assert_static dbghelp)

project(demo)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "/DASSERT_FAIL=custom_fail")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_executable(demo demo.cpp foo.cpp bar.cpp)
target_link_libraries(demo assert_static)
