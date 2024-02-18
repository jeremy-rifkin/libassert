cmake_minimum_required(VERSION 3.8)

project(demo_project VERSION 0.0.1 LANGUAGES CXX)

add_executable(main main.cpp)

find_package(libassert REQUIRED)
target_link_libraries(main libassert::assert)
