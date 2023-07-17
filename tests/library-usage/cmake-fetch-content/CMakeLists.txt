cmake_minimum_required(VERSION 3.8...3.23)

project(cmake_find_package_test)

set(TEST_TAG "null" CACHE STRING "TEST_TAG")

include(FetchContent)
FetchContent_Declare(
    assert
    GIT_REPOSITORY "https://github.com/jeremy-rifkin/libassert"
    GIT_TAG ${}
)
FetchContent_MakeAvailable(assert)

add_executable(my_executable main.cpp)
target_link_libraries(my_executable PRIVATE assert)
