#undef ASSERT_LOWERCASE
#include <cassert>
#include <assert/assert.hpp>

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace libassert::detail;

void custom_fail(libassert::assert_type, libassert::ASSERTION, const libassert::assertion_printer& printer) {
    std::cerr<<printer(libassert::utility::terminal_width(2))<<std::endl<<std::endl;
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
    bool operator==(int y) const {
        return x == y;
    }
};

int main() {
    // test rvalue
    {
        decltype(auto) a = ASSERT(only_move_constructable(2) == 2);
        static_assert(std::is_same<decltype(a), only_move_constructable>::value);
        assert(!is_lvalue(ASSERT(only_move_constructable(2) == 2)));
        assert(ASSERT(only_move_constructable(2) == 2).x == 2);
        //assert(assert(only_move_constructable(2) == 2).x++ == 2); // not allowed
    }

    // test lvalue
    {
        only_move_constructable x(2);
        decltype(auto) b = ASSERT(x == 2);
        static_assert(std::is_same<decltype(b), only_move_constructable&>::value);
        assert(is_lvalue(ASSERT(x == 2)));
        ASSERT(x == 2).x++;
        VERIFY(x.x == 3);
    }

    // test values are forwarded properly
    // serves as a regression test against issue with std::forwarding with the wrong type
    {
        // there was an issue with the decomposer trying to forward s << 2 as an S rather than a B
        struct S;
        struct B {
            bool operator==(int) const {
                return true;
            }
        };
        struct S {
            B operator<<(int) const {
                return B{};
            }
        };
        S s;
        decltype(auto) v = ASSERT(s << 2 == false);
        static_assert(std::is_same<decltype(v), B>::value);
    }
    {
        struct S;
        struct B {
            bool operator==(int) const {
                return true;
            }
        };
        B b;
        struct S {
            B& _b;
            B& operator<<(int) const {
                return _b;
            }
        };
        S s{b};
        decltype(auto) v = ASSERT(s << 2 == false);
        static_assert(std::is_same<decltype(v), B&>::value);
    }

    // above cases test lhs returns, now test the case where the full value is returned
    {
        auto v0 = ASSERT(1 | 2);
        VERIFY(v0 == 3);
        auto v1 = ASSERT(7 & 4);
        VERIFY(v1 == 4);
        auto v2 = ASSERT(1 << 16);
        VERIFY(v2 == 65536);
        auto v3 = ASSERT(32 >> 2);
        VERIFY(v3 == 8);
    }

    // test DEBUG_ASSERT returns nothing
    {
        auto f = [] {
            return DEBUG_ASSERT(false);
        };
        static_assert(std::is_same<decltype(f()), void>::value);
    }

    return 0;
}
