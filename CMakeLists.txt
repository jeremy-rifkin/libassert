cmake_minimum_required(VERSION 3.1)

project(asserts)

add_library(asserts_library_warnings INTERFACE)

if(MSVC)
    target_compile_options(asserts_library_warnings INTERFACE /W4 /WX /permissive-)
else()
    target_compile_options(asserts_library_warnings INTERFACE -Wall -Wextra -Werror=return-type -Wshadow)
endif()

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

