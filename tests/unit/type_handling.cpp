#include "assert.hpp"

#include <iostream>
#include <type_traits>
#include <utility>

using namespace asserts::detail;

void custom_fail(asserts::assertion_printer& printer, asserts::assert_type, asserts::ASSERTION) {
	std::cerr<<printer(asserts::utility::terminal_width(2))<<std::endl<<std::endl;
	abort();
}

// Some test cases for TMP stuff
static_assert(std::is_same<decltype(std::declval<expression_decomposer<int, nothing, nothing>>().get_value()), int&>::value);
static_assert(std::is_same<decltype(std::declval<expression_decomposer<int&, nothing, nothing>>().get_value()), int&>::value);
static_assert(std::is_same<decltype(std::declval<expression_decomposer<int, int, ops::lteq>>().get_value()), bool>::value);
static_assert(std::is_same<decltype(std::declval<expression_decomposer<int, int, ops::lteq>>().take_lhs()), int>::value);
static_assert(std::is_same<decltype(std::declval<expression_decomposer<int&, int, ops::lteq>>().take_lhs()), int&>::value);

static_assert(is_string_type<char*>);
static_assert(is_string_type<const char*>);
static_assert(is_string_type<char[5]>);
static_assert(is_string_type<const char[5]>);
static_assert(!is_string_type<char(*)[5]>);
static_assert(is_string_type<char(&)[5]>);
static_assert(is_string_type<const char (&)[27]>);
static_assert(!is_string_type<std::vector<char>>);
static_assert(!is_string_type<int>);
static_assert(is_string_type<std::string>);
static_assert(is_string_type<std::string_view>);

template<typename T> constexpr bool is_lvalue(T&&) {
  return std::is_lvalue_reference<T>::value;
}

struct only_move_constructable {
	int x;
	only_move_constructable(int _x) : x(_x) {}
	compl only_move_constructable() = default;
	only_move_constructable(const only_move_constructable&) = delete;
	only_move_constructable(only_move_constructable&&) = default;
	only_move_constructable& operator=(const only_move_constructable&) = delete;
	only_move_constructable& operator=(only_move_constructable&&) = delete;
	bool operator==(int y) {
		return x == y;
	}
};

int main() {
	{
		// test rvalue
		decltype(auto) a = assert(only_move_constructable(2) == 2);
		static_assert(std::is_same<decltype(a), only_move_constructable>::value);
		assert(!is_lvalue(assert(only_move_constructable(2) == 2)));
		assert(assert(only_move_constructable(2) == 2).x == 2);
		//assert(assert(only_move_constructable(2) == 2).x++ == 2); // not allowed
	}

	// test lvalue
	{
		only_move_constructable x(2);
		decltype(auto) b = assert(x == 2);
		static_assert(std::is_same<decltype(b), only_move_constructable&>::value);
		assert(is_lvalue(assert(x == 2)));
		assert(x == 2).x++;
		assert(x.x == 3);
	}
	
	return 0;
}
