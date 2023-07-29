cmake_minimum_required(VERSION 3.8)

project(demo_project VERSION 0.0.1 LANGUAGES CXX)

add_executable(main main.cpp)

set(LIBASSERT_TAG "" CACHE STRING "libassert git tag")

if(WIN32)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

include(FetchContent)
FetchContent_Declare(
  assert
  GIT_REPOSITORY https://github.com/jeremy-rifkin/libassert.git
  GIT_TAG        ${LIBASSERT_TAG}
)
FetchContent_MakeAvailable(assert)
target_link_libraries(main assert)
