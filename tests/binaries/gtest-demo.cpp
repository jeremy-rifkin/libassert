#include <libassert/assert-gtest.hpp>

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
