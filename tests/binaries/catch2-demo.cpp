#include <libassert/assert-catch2.hpp>

#include <cstdint>

uint32_t factorial( uint32_t number ) {
    return number <= 1 ? number : factorial(number-1) * number;
}

TEST_CASE( "Factorials are computed", "[factorial]" ) {
    REQUIRE( factorial( 1) == 1 );
    REQUIRE( factorial( 2) == 2 );
    // LIBASSERT_ASSERT(22 == 40, "foobar");
    std::vector<int> vec{1,2,3,4,5,6,7,8};
    int max_size = 5;
    ASSERT(vec.size() < max_size, "foobar");
    REQUIRE( factorial( 3) == 60 );
    REQUIRE( factorial(10) == 3'628'800 );
}
