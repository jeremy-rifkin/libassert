cmake_minimum_required(VERSION 3.8...3.23)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(
  libassert
  VERSION 1.1.0
  LANGUAGES CXX
)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

### libassert uses relocs, -fpic has to be used to allow shared libs to link libassert as a static library.
##set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
include(FetchContent)

add_library(assert SHARED src/assert.cpp include/assert.hpp)

target_include_directories(
  assert
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/assert/assert>
)

set(CMAKE_CXX_FLAGS_REL_WITH_ASSERTS "-O3")

# TODO
target_compile_features(
  assert
  PUBLIC
  cxx_std_17
)

set_target_properties(
  assert
  PROPERTIES
  CXX_STANDARD_REQUIRED TRUE
  CXX_EXTENSIONS OFF
)

target_compile_options(
  assert
  PRIVATE
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror=return-type -Wshadow -Wundef -Wuseless-cast -Wnonnull-compare>
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive->
)

FetchContent_Declare(
  cpptrace
  GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
  GIT_TAG        2d867703486443d0cc50970481147fa907ec3d73
)
FetchContent_MakeAvailable(cpptrace)

target_link_libraries(
  assert
  PRIVATE
  cpptrace
)

option(ASSERT_DECOMPOSE_BINARY_LOGICAL "Enables expression decomposition of && and || (this prevents short circuiting)" OFF)
option(ASSERT_LOWERCASE "Enables assert alias for ASSERT" OFF)
option(ASSERT_USE_MAGIC_ENUM "Use the MagicEnum library to print better diagnostics for enum classes" ON)
set(ASSERT_FAIL "" CACHE STRING "ASSERT_FAIL")

option(ASSERT_BUILD_TESTS "Build tests" OFF)
option(ASSERT_BUILD_BASIC_TEST "Build basic tests" OFF)

if(ASSERT_DECOMPOSE_BINARY_LOGICAL)
  target_compile_definitions(assert PUBLIC ASSERT_DECOMPOSE_BINARY_LOGICAL)
endif()

if(ASSERT_LOWERCASE)
  target_compile_definitions(assert PUBLIC ASSERT_LOWERCASE)
endif()

if(ASSERT_USE_MAGIC_ENUM)
  target_compile_definitions(assert PUBLIC ASSERT_USE_MAGIC_ENUM)
endif()

if(NOT "${ASSERT_FAIL}" STREQUAL "")
  target_compile_definitions(assert PUBLIC ASSERT_FAIL=${ASSERT_FAIL})
endif()

if(CMAKE_BUILD_TYPE STREQUAL "")
  message(FATAL_ERROR "Setting CMAKE_BUILD_TYPE is required")
endif()

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(CMakePackageConfigHelpers)

  install(
    TARGETS assert
    EXPORT assert_targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

  install(
    FILES
    include/assert.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/assert/assert
  )

  install(
    FILES
    third_party/magic_enum.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/assert/third_party
  )

  export(
    EXPORT assert_targets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/assert/assert_targets.cmake
    NAMESPACE assert::
  )

  configure_package_config_file(
    cmake/assert-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/assert/assert-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/assert
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/assert/assert-config-version.cmake
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(
    EXPORT assert_targets
    FILE assert_targets.cmake
    NAMESPACE assert::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/assert
  )

  install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/assert/assert-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/assert/assert-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/assert
  )
endif()

# Don't run tests when library is used with add_subdirectory
if(PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  if(ASSERT_BUILD_TESTS)
    include(CTest)

    file(GLOB_RECURSE demo_sources tests/demo/*.cpp)
    add_executable(demo ${demo_sources})
    target_link_libraries(demo PRIVATE assert)
    target_compile_options(
      demo
      PRIVATE
      "-DASSERT_USE_MAGIC_ENUM"
      "-DASSERT_FAIL=custom_fail"
      "-DASSERT_LOWERCASE"
    )

    file(GLOB_RECURSE integration_sources tests/integration/*.cpp)
    add_executable(integration ${integration_sources})
    target_link_libraries(integration PRIVATE assert)
    target_compile_options(
      integration
      PRIVATE
      "-DASSERT_USE_MAGIC_ENUM"
      "-DASSERT_FAIL=custom_fail"
      "-DASSERT_LOWERCASE"
    )
    add_test(
      NAME integration
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests
      COMMAND
      python3 run-tests.py $<TARGET_FILE:integration> ${CMAKE_BUILD_TYPE} ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_STANDARD}
    )

    file(GLOB test_sources tests/unit/*.cpp)
    foreach(test_file ${test_sources})
      get_filename_component(test_name ${test_file} NAME_WE)
      add_executable(${test_name} ${test_file})
      target_link_libraries(${test_name} assert)
      add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach(test_file ${test_sources})
  endif()
  if(ASSERT_BUILD_BASIC_TEST)
    add_executable(basic tests/basic/basic_test.cpp)
    target_link_libraries(basic PRIVATE assert)
  endif()
endif()
