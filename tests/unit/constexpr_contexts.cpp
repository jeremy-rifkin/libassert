#ifdef TEST_MODULE
import libassert;
#include <libassert/assert-macros.hpp>
#else
#include <libassert/assert.hpp>
#endif

template<int X> void foo() {}

constexpr int bar(int x) {
    DEBUG_ASSERT(x % 2 == 0);
    return x / 2;
}

int main() {
    foo<bar(2)>();
}
