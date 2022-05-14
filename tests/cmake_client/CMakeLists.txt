cmake_minimum_required(VERSION 3.16)

project(cmake_find_package_test)

find_package(
  assert
  REQUIRED
  NO_PACKAGE_ROOT_PATH
  NO_SYSTEM_ENVIRONMENT_PATH
  NO_CMAKE_PACKAGE_REGISTRY
  NO_CMAKE_SYSTEM_PATH
  NO_CMAKE_SYSTEM_PACKAGE_REGISTRY
)

add_executable(
  main
  main.cpp
)

target_link_libraries(
  main
  PUBLIC
  assert::assert
)
