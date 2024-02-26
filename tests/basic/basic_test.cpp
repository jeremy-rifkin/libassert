// Most basic of tests

#include <cassert>
#include <optional>
#include <type_traits>

#include <libassert/assert.hpp>

std::optional<float> foo() {
    return 2.5f;
}

int main() {
    auto f = *DEBUG_ASSERT_VAL(foo());
    static_assert(std::is_same<decltype(f), float>::value);
    assert(f == 2.5f);
}
