cmake_minimum_required(VERSION 3.8)

project(demo_project VERSION 0.0.1 LANGUAGES CXX)

add_executable(main main.cpp)

set(LIBASSERT_TAG "" CACHE STRING "libassert git tag")

include(FetchContent)
FetchContent_Declare(
  assert
  GIT_REPOSITORY https://github.com/jeremy-rifkin/libassert.git
  GIT_TAG        ${LIBASSERT_TAG}
)
FetchContent_MakeAvailable(assert)
target_link_libraries(main libassert::assert)

if(WIN32)
  add_custom_command(
    TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:libassert::assert>
    $<TARGET_FILE_DIR:main>
  )
  add_custom_command(
    TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:cpptrace::cpptrace>
    $<TARGET_FILE_DIR:main>
  )
endif()
