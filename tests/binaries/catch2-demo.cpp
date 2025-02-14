#include "catch2/catch_test_macros.hpp"
#include <libassert/assert-catch2.hpp>

TEST_CASE("1 + 1 is 2") {
    ASSERT(1 + 1 == 3);
}

void foo(int x) {
    LIBASSERT_ASSERT(x >= 20, "foobar");
}

TEST_CASE("REQUIRE_ASSERT FAIL") {
    REQUIRE_ASSERT(foo(20));
}

TEST_CASE("REQUIRE_ASSERT PASS") {
    REQUIRE_ASSERT(foo(5));
}
