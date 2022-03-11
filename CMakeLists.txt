cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----

# Note: update this to your new project's name and version
project(
  Asserts
  VERSION 1.0
  LANGUAGES CXX)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

include(cmake/CPM.cmake)

cpmaddpackage("gh:TheLartians/PackageProject.cmake@1.8.0")

add_library(${PROJECT_NAME} include/assert.hpp src/assert.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

# Link dependencies
if(MSVC)
  target_link_libraries(assert PRIVATE dbghelp)
endif()

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>)

string(TOLOWER ${PROJECT_NAME}/version.hpp VERSION_HEADER_LOCATION)

packageproject(
  NAME
  ${PROJECT_NAME}
  VERSION
  ${PROJECT_VERSION}
  NAMESPACE
  ${PROJECT_NAME}
  BINARY_DIR
  ${PROJECT_BINARY_DIR}
  INCLUDE_DIR
  ${PROJECT_SOURCE_DIR}/include
  INCLUDE_DESTINATION
  include/${PROJECT_NAME}-${PROJECT_VERSION}
  VERSION_HEADER
  "${VERSION_HEADER_LOCATION}"
  COMPATIBILITY
  SameMajorVersion)
