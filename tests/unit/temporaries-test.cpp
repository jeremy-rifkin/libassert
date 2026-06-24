#include <libassert/assert-gtest.hpp>

namespace {
int count = 0;

struct Counter {
	Counter() {
		++count;
	}

	~Counter() {
		--count;
	}
};
} // namespace

TEST(LibassertTemp, TempWorks) {
	ASSERT(count == 0);
	ASSERT((Counter {}, count) == 1);

	int inner_count = 0;
	try {
		LIBASSERT_ASSERT(
			// intentionally assert false
			(Counter {}, false),

			// to evaluate extra arg
			([&]() {
				// record the value of count
				LIBASSERT_ASSERT((Counter {}, inner_count = count, false));
				return 0;
			})()
		);
	} catch (...) {}

	// check the recorded value
	ASSERT(inner_count == 2);
	ASSERT(count == 0);
}
