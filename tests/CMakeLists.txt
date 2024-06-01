# Don't run tests when library is used with add_subdirectory
if(PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    if(WIN32)
      add_custom_command(
        TARGET libassert-lib POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:cpptrace::cpptrace>
        $<TARGET_FILE_DIR:libassert-lib>
      )
    endif()
    include(CTest)

    add_executable(
      demo
      tests/demo/bar.cpp
      tests/demo/baz/demo.cpp
      tests/demo/demo.cpp
      tests/demo/foo.cpp
    )
    target_link_libraries(demo PRIVATE libassert-lib)
    target_compile_definitions(demo PRIVATE LIBASSERT_LOWERCASE)
    if(LIBASSERT_USE_MAGIC_ENUM)
     target_compile_definitions(demo PRIVATE LIBASSERT_USE_MAGIC_ENUM)
    endif()
    target_compile_features(
      demo
      PUBLIC cxx_std_20
    )
    target_compile_definitions(
      demo
      PUBLIC LIBASSERT_SAFE_COMPARISONS
    )

    if(LIBASSERT_USE_MAGIC_ENUM)
     add_executable(integration tests/integration/integration.cpp tests/integration/a.cpp tests/integration/x/a.cpp)
     # Temporary workaround for Visual Studio 2022 bug with __builtin_LINE() and __builtin_FILE()
     # https://developercommunity.visualstudio.com/t/__builtin_LINE-function-is-reporting-w/10439054?space=62&q=__builtin_function
     # TODO: Workaround in the header for end users?
     target_compile_features(integration PUBLIC cxx_std_20)
     target_link_libraries(integration PRIVATE libassert-lib)
     target_compile_definitions(
       integration
       PRIVATE
       LIBASSERT_USE_MAGIC_ENUM
       LIBASSERT_LOWERCASE
       LIBASSERT_SAFE_COMPARISONS
     )
     add_test(
       NAME integration
       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests
       COMMAND
       python3 run-tests.py $<TARGET_FILE:integration> ${CMAKE_BUILD_TYPE} ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_STANDARD}
     )
     set(integration_target integration)
    else()
     set(integration_target)
    endif()

    set(
      dsym_targets
      demo
      ${integration_target}
    )

    set(
      all_targets
      demo
      ${integration_target}
    )

    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY "https://github.com/google/googletest.git"
      GIT_TAG        f8d7d77c06936315286eb55f8de22cd23c188571 # v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    include(FetchContent)
    FetchContent_Declare(
      Catch2
      GIT_SHALLOW    TRUE
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        4e8d92bf02f7d1c8006a0e7a5ecabd8e62d98502 # v3.6.0
    )
    FetchContent_MakeAvailable(Catch2)

    include(FetchContent)
    FetchContent_Declare(
      fmt
      GIT_SHALLOW    TRUE
      GIT_REPOSITORY https://github.com/fmtlib/fmt.git
      GIT_TAG        e69e5f977d458f2650bb346dadf2ad30c5320281 # v10.2.1
    )
    FetchContent_MakeAvailable(fmt)

    # For catch 2 only
    # list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/contrib)
    # For catch 3 only
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)

    set(
      unit_test_sources
      tests/unit/constexpr_contexts.cpp
      tests/unit/disambiguation.cpp
      tests/unit/lexer.cpp
      tests/unit/literals.cpp
      tests/unit/test_public_utilities.cpp
      tests/unit/test_type_prettier.cpp
      tests/unit/type_handling.cpp
      tests/unit/stringify.cpp
      tests/unit/fmt-test.cpp
      tests/unit/assertion_tests.cpp
    )
    foreach(test_file ${unit_test_sources})
      get_filename_component(test_name ${test_file} NAME_WE)
      list(APPEND all_targets ${test_name})
      add_executable(${test_name} ${test_file})
      target_link_libraries(${test_name} PRIVATE libassert-lib)
      target_include_directories(${test_name} PRIVATE src)
      target_compile_definitions(${test_name} PRIVATE LIBASSERT_BUILD_TESTING)
      target_compile_features(${test_name} PUBLIC cxx_std_17)
      if(LIBASSERT_USE_CI_WRAPPER)
        add_test(
          NAME ${test_name}
          COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tests/ci-wrapper.py $<TARGET_FILE:${test_name}>
        )
      else()
        add_test(NAME ${test_name} COMMAND ${test_name})
      endif()
      list(APPEND dsym_targets ${test_name})
    endforeach()

    target_link_libraries(lexer PRIVATE GTest::gtest_main)
    target_link_libraries(fmt-test PRIVATE GTest::gtest_main fmt::fmt)
    target_link_libraries(assertion_tests PRIVATE GTest::gtest_main)
    target_compile_options(assertion_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/permissive- /Zc:preprocessor>)
    target_compile_definitions(fmt-test PRIVATE LIBASSERT_USE_FMT)
    target_compile_options(lexer PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>)
    target_compile_options(fmt-test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>)
    target_compile_options(stringify PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>)

    set(
      binary_sources
      tests/binaries/basic_test.cpp
      tests/binaries/basic_demo.cpp
      tests/binaries/gtest-demo.cpp
      tests/binaries/catch2-demo.cpp
      tests/binaries/tokens_and_highlighting.cpp
    )
    foreach(test_file ${binary_sources})
      get_filename_component(test_name ${test_file} NAME_WE)
      list(APPEND all_targets ${test_name})
      add_executable(${test_name} ${test_file})
      target_link_libraries(${test_name} PRIVATE libassert-lib)
      target_include_directories(${test_name} PRIVATE src)
      target_compile_definitions(${test_name} PRIVATE LIBASSERT_BUILD_TESTING)
      list(APPEND dsym_targets ${test_name})
    endforeach()

    target_link_libraries(gtest-demo PRIVATE GTest::gtest_main)
    target_compile_options(gtest-demo PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>)
    target_link_libraries(catch2-demo PRIVATE Catch2::Catch2WithMain)
    target_compile_options(catch2-demo PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>)
    target_compile_definitions(basic_demo PRIVATE LIBASSERT_BREAK_ON_FAIL)

    if(APPLE)
      foreach(target ${dsym_targets})
        add_custom_command(
          TARGET ${target}
          POST_BUILD
          COMMAND dsymutil $<TARGET_FILE:${target}>
        )
      endforeach()
    endif()

    foreach(target ${all_targets})
      target_compile_options(
        ${target_name}
        PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror=return-type -Wshadow -Wundef -Werror>
        $<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast -Wnonnull-compare>
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive->
      )
    endforeach()
endif()
