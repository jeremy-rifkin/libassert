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
     if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      add_test(
        NAME integration
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests
        COMMAND
        python3 run-tests.py $<TARGET_FILE:integration> ${CMAKE_BUILD_TYPE} ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_STANDARD}
      )
     endif()
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

    function(do_add_unittest)
      cmake_parse_arguments(
          LIBASSERT_UNIT
          ""
          ""
          "TEST_NAME;SOURCES;OPTIONS;FEATURES;DEFINES;LIBS"
          ${ARGN}
      )

      add_executable(
        "${LIBASSERT_UNIT_TEST_NAME}"
        ${LIBASSERT_UNIT_SOURCES}
      )

      list(APPEND all_targets "${LIBASSERT_UNIT_TEST_NAME}")
      target_link_libraries("${LIBASSERT_UNIT_TEST_NAME}" PRIVATE libassert-lib ${LIBASSERT_UNIT_LIBS})
      target_compile_options("${LIBASSERT_UNIT_TEST_NAME}" PRIVATE ${LIBASSERT_UNIT_OPTIONS})
      target_compile_features("${LIBASSERT_UNIT_TEST_NAME}" PRIVATE ${LIBASSERT_UNIT_FEATURES})
      target_include_directories("${LIBASSERT_UNIT_TEST_NAME}" PRIVATE src)
      target_compile_definitions("${LIBASSERT_UNIT_TEST_NAME}" PRIVATE LIBASSERT_BUILD_TESTING ${LIBASSERT_UNIT_DEFINES})
      target_compile_features("${LIBASSERT_UNIT_TEST_NAME}" PUBLIC cxx_std_17)
      if(LIBASSERT_USE_CI_WRAPPER)
        add_test(
          NAME "${LIBASSERT_UNIT_TEST_NAME}"
          COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tests/ci-wrapper.py $<TARGET_FILE:"${LIBASSERT_UNIT_TEST_NAME}">
        )
      else()
        add_test(NAME "${LIBASSERT_UNIT_TEST_NAME}" COMMAND "${LIBASSERT_UNIT_TEST_NAME}")
      endif()
      list(APPEND dsym_targets "${LIBASSERT_UNIT_TEST_NAME}")
    endfunction()

    function(add_unittest)
      cmake_parse_arguments(
          LIBASSERT_UNIT
          ""
          ""
          "SOURCE;OPTIONS;FEATURES;DEFINES;LIBS"
          ${ARGN}
      )

      get_filename_component(test_name "${LIBASSERT_UNIT_SOURCE}" NAME_WE)

      do_add_unittest(
        TEST_NAME "${test_name}"
        SOURCES ${LIBASSERT_UNIT_SOURCE}
        OPTIONS ${LIBASSERT_UNIT_OPTIONS}
        FEATURES ${LIBASSERT_UNIT_FEATURES}
        DEFINES ${LIBASSERT_UNIT_DEFINES}
        LIBS ${LIBASSERT_UNIT_LIBS}
      )

      if(HAS_CXX20_MODULES)
        do_add_unittest(
          TEST_NAME "${test_name}_modules"
          SOURCES ${LIBASSERT_UNIT_SOURCE}
          OPTIONS ${LIBASSERT_UNIT_OPTIONS}
          FEATURES ${LIBASSERT_UNIT_FEATURES}
          DEFINES ${LIBASSERT_UNIT_DEFINES} TEST_MODULE
          LIBS ${LIBASSERT_UNIT_LIBS}
        )
      endif()
    endfunction()

    add_unittest(SOURCE tests/unit/constexpr_contexts.cpp)
    add_unittest(SOURCE tests/unit/disambiguation.cpp)
    add_unittest(SOURCE tests/unit/lexer.cpp OPTIONS $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor> LIBS GTest::gtest_main)
    add_unittest(SOURCE tests/unit/literals.cpp)
    add_unittest(SOURCE tests/unit/test_public_utilities.cpp)
    add_unittest(SOURCE tests/unit/test_type_prettier.cpp)
    add_unittest(SOURCE tests/unit/type_handling.cpp)
    add_unittest(
      SOURCE tests/unit/stringify.cpp
      OPTIONS $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
      LIBS GTest::gtest_main
    )
    add_unittest(
      SOURCE tests/unit/fmt-test.cpp
      OPTIONS $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
      DEFINES LIBASSERT_USE_FMT
      LIBS GTest::gtest_main fmt::fmt
    )
    add_unittest(
      SOURCE tests/unit/std_format20.cpp
      OPTIONS $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
      FEATURES cxx_std_20
      DEFINES LIBASSERT_USE_FMT
      LIBS GTest::gtest_main fmt::fmt
    )
    add_unittest(
      SOURCE tests/unit/assertion_tests.cpp
      OPTIONS $<$<CXX_COMPILER_ID:MSVC>:/permissive- /Zc:preprocessor>
      LIBS GTest::gtest_main
    )
    if(cxx_std_23 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
      add_unittest(
        SOURCE tests/unit/std_format23.cpp
        OPTIONS $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
        FEATURES cxx_std_23
        DEFINES LIBASSERT_USE_FMT
        LIBS GTest::gtest_main fmt::fmt
      )
    endif()
    add_unittest(
      SOURCE tests/unit/catch2-test.cpp
      OPTIONS $<$<CXX_COMPILER_ID:MSVC>:/permissive- /Zc:preprocessor>
      LIBS Catch2::Catch2WithMain
    )

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
      target_compile_features(${test_name} PUBLIC cxx_std_17)
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
        ${warning_options}
      )
    endforeach()
endif()
