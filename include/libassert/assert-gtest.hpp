#ifndef ASSERT_GTEST_HPP
#define ASSERT_GTEST_HPP

#include <gtest/gtest.h>

#define LIBASSERT_PREFIX_ASSERTIONS
#include <libassert/assert.hpp>

#include "tokenizer.hpp"

#if defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL != 0
 #error "Libassert integration does not work with MSVC's non-conformant preprocessor. /Zc:preprocessor must be used."
#endif
#define ASSERT(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); ASSERT_TRUE(true); } catch(std::exception& e) { ASSERT_TRUE(false) << e.what(); } } while(false)
#define EXPECT(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); ASSERT_TRUE(true); } catch(std::exception& e) { EXPECT_TRUE(false) << e.what(); } } while(false)

inline void libassert_failure_handler(const libassert::assertion_info& info) {
    std::string message = "";
    throw std::runtime_error(info.header());
}

inline auto libassert_pre_main = [] () {
    libassert::set_failure_handler(libassert_failure_handler);
    return 1;
} ();

#endif
