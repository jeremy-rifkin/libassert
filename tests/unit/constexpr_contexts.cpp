#include <libassert/assert.hpp>

template<int X> void foo() {}

constexpr int bar(int x) {
    DEBUG_ASSERT(x % 2 == 0);
    return x / 2;
}

int main() {
    foo<bar(2)>();
}
