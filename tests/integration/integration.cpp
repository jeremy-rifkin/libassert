#define _CRT_SECURE_NO_WARNINGS // for fopen

#include "assert.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <streambuf>
#include <string_view>
#include <string>

using namespace std::literals;

void test_path_differentiation();

void custom_fail(std::string message, assert_detail::assert_type, assert_detail::ASSERTION) {
	std::cerr<<message<<std::endl;
	//std::cerr<<assert_detail::strip_colors(message)<<std::endl<<std::endl;
}

void rec(int n) {
	if(n == 0) assert(false);
	else rec(n - 1);
}

void recursive_a(int), recursive_b(int);

void recursive_a(int n) {
	if(n == 0) assert(false);
	else recursive_b(n - 1);
}

void recursive_b(int n) {
	if(n == 0) assert(false);
	else recursive_a(n - 1);
}

template<typename T>
class test_class {
public:
	template<typename S> void something([[maybe_unused]] std::pair<S, T> x) {
		something_else();
	}

	void something_else() {
		// value printing: strings
		// value printing: pointers
		// value printing: number formats
		// value printing: ostream overloads
		// value printing: no ostream overload

		// optional messages
		assert(false, 2);
		assert(false, "foo");
		assert(false, "foo"s);
		assert(false, "foo"sv);
		assert(false, (char*)"foo");
		assert(false, "foo", 2);
		assert(false, "foo"s, 2);
		assert(false, "foo"sv, 2);
		assert(false, (char*)"foo", 2);
		// extra diagnostics
		// errno
		errno = 2;
		assert(false, errno);
		errno = 2;
		assert(false, "foo", errno);

		// safe comparisons
		assert(18446744073709551606ULL == -10);
		assert(-1 > 1U);

		// expression decomposition
		
		// value forwarding: copy / moves
		// value forwarding: lifetimes
		// value forwarding: lvalue references
		// value forwarding: rvalues
		
		// stack traces
		// basic stack trace
		// simple recursion
		rec(10);
		// other recursion
		recursive_a(10);

		// path tries
		wubble();
	}
};

int main() {
	test_class<int> t;
	t.something(std::pair {1., 1});
}
