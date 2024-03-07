#include <gtest/gtest.h>

#define LIBASSERT_PREFIX_ASSERTIONS
#include <libassert/assert.hpp>

#include "tokenizer.hpp"

#define ASSERT(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); ASSERT_TRUE(true); } catch(std::exception& e) { ASSERT_TRUE(false) << e.what(); } } while(false)
#define EXPECT(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); ASSERT_TRUE(true); } catch(std::exception& e) { EXPECT_TRUE(false) << e.what(); } } while(false)

void failure_handler(const libassert::assertion_info& info) {
    std::string message = "";
    throw std::runtime_error(info.to_string());
}

auto pre_main = [] () {
    libassert::set_failure_handler(failure_handler);
    return 1;
} ();

TEST(LexerTests, Basic) {
    //ASSERT(1 == 2, "foobar");
    ASSERT(1 == 2);
    ASSERT(2 == 22);
}

TEST(LexerTests, Strings) {
    //ASSERT(1 == 2, "foobar");
    EXPECT(1 == 2);
    EXPECT(2 == 22);
}
