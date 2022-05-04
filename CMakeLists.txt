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
    
option(ASSERT_DECOMPOSE_BINARY_LOGICAL "Enables expression decomposition of && and ||" OFF)
option(ASSERT_LOWERCASE "Enables assert alias for ASSERT" OFF)
option(ASSERT_USE_MAGIC_ENUM "Use the MagicEnum library to print better diagnostics for enum classes" ON)
set(ASSERT_FAIL "" CACHE STRING "ASSERT_FAIL")

if (ASSERT_DECOMPOSE_BINARY_LOGICAL)
    target_compile_definitions(assert PUBLIC ASSERT_DECOMPOSE_BINARY_LOGICAL)
endif()

if (ASSERT_LOWERCASE)
    target_compile_definitions(assert PUBLIC ASSERT_LOWERCASE)
endif()

if (ASSERT_USE_MAGIC_ENUM)
    target_compile_definitions(assert PUBLIC ASSERT_USE_MAGIC_ENUM)
endif()

if (NOT ${ASSERT_FAIL} STREQUAL "")
    target_compile_definitions(assert PUBLIC ASSERT_FAIL=${ASSERT_FAIL})
endif()

set_target_properties(assert
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED TRUE
        CXX_EXTENSIONS OFF)

target_link_libraries(assert PRIVATE asserts_library_warnings)

if (MSVC)
    target_link_libraries(assert PRIVATE dbghelp)
endif ()

