#include <assert/assert.hpp>

template<int X> void foo() {}

constexpr int bar(int x) {
    ASSERT(x % 2 == 0);
    return x / 2;
}

int main() {
    foo<bar(2)>();
}
