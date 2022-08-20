cmake_minimum_required(VERSION 3.16)

project(cmake_find_package_test LANGUAGES CXX)

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

add_custom_command(TARGET main POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:assert::assert> $<TARGET_FILE_DIR:main>
)
