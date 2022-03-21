// Most basic of tests

#include <assert.h>
#include <optional>
#include <type_traits>

#include "assert.hpp"

std::optional<float> foo() {
	return 2.5f;
}

int main() {
	auto f = *ASSERT(foo());
	static_assert(std::is_same<decltype(f), float>::value);
	assert(f == 2.5f);
}
