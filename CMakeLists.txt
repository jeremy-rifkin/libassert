cmake_minimum_required(VERSION 3.8...3.23)

project(
  libassert
  VERSION 1.0.0
  LANGUAGES CXX
)

option(ASSERT_BUILD_TESTS "Enables tests" OFF)
option(ASSERT_DECOMPOSE_BINARY_LOGICAL "Enables expression decomposition of && and ||" OFF)
option(ASSERT_LOWERCASE "Enables assert alias for ASSERT" OFF)
option(ASSERT_USE_MAGIC_ENUM "Use the MagicEnum library to print better diagnostics for enum classes" ON)
set(ASSERT_FAIL "" CACHE STRING "ASSERT_FAIL")

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# libassert uses relocs, -fpic has to be used to allow shared libs to link libassert as a static library.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
add_library(assert src/assert.cpp)

target_include_directories(
  assert
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/assert/assert>
)

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
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror=return-type -Wshadow>
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive->
)

target_link_libraries(
  assert
  PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:dbghelp>
  ${CMAKE_DL_LIBS}
)

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

if (PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  if(ASSERT_BUILD_TESTS)
    include(CTest)
    add_test(
      NAME run-tests.py
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests
      COMMAND
      python3 run-tests.py --build --build_type=${CMAKE_BUILD_TYPE} --compiler=${CMAKE_CXX_COMPILER_ID}
    )
  endif()
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
