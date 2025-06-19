#include <cmath>
#ifdef TEST_MODULE
#include <catch2/catch_test_macros.hpp>
import libassert;
#include <libassert/assert-catch2-macros.hpp>
#else
#include <libassert/assert-catch2.hpp>
#endif

TEST_CASE("1 + 1 is 2") {
    ASSERT(1 + 1 == 2);
}

int set(int& x, int y) {
    auto copy = x;
    x = y;
    return copy;
}

TEST_CASE("set") {
    int x = 20;
    ASSERT(set(x, 42) == 20);
    CHECK(x == 42);
}
