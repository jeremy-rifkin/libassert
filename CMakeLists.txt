cmake_minimum_required(VERSION 3.15...4.0)

include(cmake/PreventInSourceBuilds.cmake)


# ---- Initialize Project ----

# used to support find_package
set(package_name "libassert")


# create base project
project(
  libassert
  VERSION 2.2.0
  DESCRIPTION "The most over-engineered C++ assertion library"
  HOMEPAGE_URL "https://github.com/jeremy-rifkin/libassert"
  LANGUAGES CXX
)

# don't change include order, OptionVariables checks if project is top level
include(cmake/ProjectIsTopLevel.cmake)
include(cmake/OptionVariables.cmake)

if(PROJECT_IS_TOP_LEVEL)
  find_program(CCACHE_FOUND ccache)
  if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  endif()
endif()

if(PROJECT_IS_TOP_LEVEL)
  if(CMAKE_GENERATOR STREQUAL "Ninja")
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
    endif()
    if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
      SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdiagnostics-color=always")
    elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
      SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcolor-diagnostics")
    endif()
  endif()
endif()

set(HAS_CXX20_MODULES FALSE)
if(NOT LIBASSERT_DISABLE_CXX_20_MODULES)
  if(CMAKE_CXX_STANDARD GREATER_EQUAL 20)
    # https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html#compiler-support
    # msvc 14.34+/19.34+
    # clang 16+
    # gcc 15 and newer
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.34)
        set(HAS_CXX20_MODULES TRUE)
      endif()
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16.0)
        set(HAS_CXX20_MODULES TRUE)
      endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0)
        set(HAS_CXX20_MODULES TRUE)
      endif()
    endif()
  endif()
  if(HAS_CXX20_MODULES)
    message(STATUS "Libassert HAS_CXX20_MODULES: Yes")
  else()
    message(STATUS "Libassert HAS_CXX20_MODULES: No")
  endif()
endif()


# ---- Project Dependencies ----

set(LIBASSERT_MAGIC_ENUM_REPO "https://github.com/Neargye/magic_enum.git")
set(LIBASSERT_MAGIC_ENUM_TAG "e046b69a3736d314fad813e159b1c192eaef92cd") # v0.9.7

set(LIBASSERT_ENCHANTUM_REPO "https://github.com/ZXShady/enchantum.git")
set(LIBASSERT_ENCHANTUM_TAG "c460fbf0fcfe01eb49cd8e5790f05ced7e2a6bb6") # 0.3.0

set(LIBASSERT_CPPTRACE_REPO "https://github.com/jeremy-rifkin/cpptrace.git")
set(LIBASSERT_CPPTRACE_TAG "3db8da80111171c219ab5839905771386bee06b3") # 1.0.4

# obtain cpptrace
if(LIBASSERT_USE_EXTERNAL_CPPTRACE)
  find_package(cpptrace REQUIRED)
else()
  include(FetchContent)
  FetchContent_Declare(
    cpptrace
    GIT_REPOSITORY "${LIBASSERT_CPPTRACE_REPO}"
    GIT_TAG        "${LIBASSERT_CPPTRACE_TAG}"
  )
  FetchContent_MakeAvailable(cpptrace)
endif()

# cpptrace potentially does not have an alias target
if(NOT TARGET cpptrace::cpptrace)
  add_library(cpptrace::cpptrace ALIAS cpptrace)
endif()


# ---- Declare Library ----

# target that we can modify (can't modify ALIAS targets)
# target name should not be the same as ${PROJECT_NAME}, causes add_subdirectory issues
set(target_name "libassert-lib")
add_library(${target_name} ${build_type})

# alias to cause error at configuration time instead of link time if target is missing
add_library(libassert::assert ALIAS ${target_name})

# add /include files to target
# this is solely for IDE benefit, doesn't affect building, so unconditionally list magic_enum
target_sources(
  ${target_name} PRIVATE
  # include
  include/libassert/assert.hpp
  include/libassert/platform.hpp
)

# add /src files to target
target_sources(
  ${target_name} PRIVATE
  # src
  src/assert.cpp
  src/analysis.cpp
  src/utils.cpp
  src/stringification.cpp
  src/platform.cpp
  src/printing.cpp
  src/paths.cpp
  src/tokenizer.cpp
)

if(HAS_CXX20_MODULES)
  target_sources(
    ${target_name} PUBLIC
    FILE_SET CXX_MODULES
    FILES "src/libassert.cppm"
    BASE_DIRS "src"
  )
endif()

# link dependencies
target_link_libraries(
  ${target_name} PUBLIC
  cpptrace::cpptrace
)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND "${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "MSVC")
  set(IS_CLANG_CL 1)
else()
  set(IS_CLANG_CL 0)
endif()

set(
  warning_options
  $<$<NOT:$<OR:$<CXX_COMPILER_ID:MSVC>,$<STREQUAL:${IS_CLANG_CL},1>>>:-Wall -Wextra -Werror=return-type -Wundef>
  $<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast -Wmaybe-uninitialized>
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if(LIBASSERT_WERROR_BUILD)
  set(
    warning_options
    ${warning_options}
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/WX>
  )
endif()

target_compile_options(
  ${target_name}
  PRIVATE
  ${warning_options}
)

set(LIBASSERT_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
set(LIBASSERT_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
set(LIBASSERT_VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH})
configure_file("${PROJECT_SOURCE_DIR}/cmake/in/version-hpp.in" "${PROJECT_BINARY_DIR}/include/libassert/version.hpp")

# ---- Generate Build Info Headers ----

# used in export header generated below
if(build_type STREQUAL "STATIC")
  target_compile_definitions(${target_name} PUBLIC LIBASSERT_STATIC_DEFINE)
  set(LIBASSERT_STATIC_DEFINE TRUE)
endif()

# ---- Library Properties ----

# hide all symbols by default
# use SameMajorVersion versioning for shared library runtime linker lookup
set_target_properties(
  ${target_name} PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  VERSION "${PROJECT_VERSION}"
  SOVERSION "${PROJECT_VERSION_MAJOR}"
  EXPORT_NAME "assert"
  OUTPUT_NAME "assert"
)

# header files generated by CMake
target_include_directories(
  ${target_name} SYSTEM PUBLIC
  "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>"
)

# header files from /include
target_include_directories(
  ${target_name} ${warning_guard} PUBLIC
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(
  ${target_name}
  PUBLIC ${LIBASSERT_DESIRED_CXX_STANDARD}
)

if(LIBASSERT_SANITIZER_BUILD)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

if(
  LIBASSERT_BUILD_TESTING
  AND NOT ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND "${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 9.0)
  AND NOT LIBASSERT_USE_ENCHANTUM
)
  set(LIBASSERT_USE_MAGIC_ENUM ON)
endif()

if(LIBASSERT_USE_MAGIC_ENUM AND LIBASSERT_USE_ENCHANTUM)
    message(FATAL_ERROR "You cannot use both magic_enum and enchantum at once!")
endif()

if(LIBASSERT_USE_MAGIC_ENUM)
  set(MAGIC_ENUM_OPT_INSTALL ON)
  if(LIBASSERT_USE_EXTERNAL_MAGIC_ENUM)
    find_package(magic_enum REQUIRED)
  else()
    include(FetchContent)
    FetchContent_Declare(magic_enum
      GIT_REPOSITORY "${LIBASSERT_MAGIC_ENUM_REPO}"
      GIT_TAG        "${LIBASSERT_MAGIC_ENUM_TAG}")
    FetchContent_MakeAvailable(magic_enum)
  endif()
  target_link_libraries(${target_name} PUBLIC magic_enum::magic_enum)
  target_compile_definitions(
    ${target_name} PUBLIC
    # $<$<BOOL:"${ASSERT_USE_MAGIC_ENUM}">:ASSERT_USE_MAGIC_ENUM>
    LIBASSERT_USE_MAGIC_ENUM
  )
endif()

if(NOT LIBASSERT_STD_FORMAT)
  target_compile_definitions(${target_name} PUBLIC LIBASSERT_NO_STD_FORMAT)
endif()

if(LIBASSERT_USE_ENCHANTUM)
  if(LIBASSERT_USE_EXTERNAL_ENCHANTUM)
    find_package(enchantum REQUIRED)
  else()
    include(FetchContent)
    FetchContent_Declare(enchantum
      GIT_REPOSITORY "${LIBASSERT_ENCHANTUM_REPO}"
      GIT_TAG        "${LIBASSERT_ENCHANTUM_TAG}")
    FetchContent_MakeAvailable(enchantum)
  endif()
  target_link_libraries(${target_name} PUBLIC enchantum::enchantum)
  target_compile_definitions(
    ${target_name} PUBLIC
    LIBASSERT_USE_ENCHANTUM
  )
endif()

# ---- Install Rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/InstallRules.cmake)
endif()


# ---- Setup Tests ----

if(LIBASSERT_BUILD_TESTING)
  target_compile_definitions(${target_name} PRIVATE LIBASSERT_BUILD_TESTING)

  # need to enable testing in case BUILD_TESTING is disabled
  # ctest expects that the top level project enables testing
  if(PROJECT_IS_TOP_LEVEL)
      enable_testing()
  endif()

  # tell unit tests where our files are
  set(LIBASSERT_BINARY_DIR "${PROJECT_BINARY_DIR}")
  set(LIBASSERT_SOURCE_DIR "${PROJECT_SOURCE_DIR}")

  # include test project
  # add_subdirectory(tests)
  include(tests/CMakeLists.txt)
endif()
