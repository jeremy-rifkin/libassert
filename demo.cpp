#include "assert.hpp"

#include <map>
#include <string>
#include <fstream>
#include <streambuf>
#include <iostream>
#include <string_view>
#include <iostream>

template<class T> struct S {
	T x;
	S() = default;
	S(T&& x) : x(x) {}
	bool operator==(const S& s) const { return x == s.x; }
	friend std::ostream& operator<<(std::ostream& o, const S& s) {
		o<<"I'm S<"<<assert_impl_::type_name<T>()<<"> and I contain:"<<std::endl;
		std::ostringstream oss;
		oss<<s.x;
		o<<assert_impl_::indent(std::move(oss).str(), 4);
		return o;
	}
};

template<> struct S<void> {
	bool operator==(const S&) const { return false; }
};

void foo();
int bar() {
	return 2;
}

int main() {
    assert(false, "this should be unreachable");
	assert(false);
    std::map<int, int> map {{1,1}};
    assert_eq(map.count(1), 2);
    assert_gteq(map.count(1 == 1), 2ULL);
    assert_eq(map.count(1), 2ULL, "some data not received");
    assert_gteq(map.count(2 * bar()), 2, "some data not received");
    assert_eq(1, 1.5);
	assert_eq(.1, 2);
	assert_eq(.1f, .1);
	assert_eq(0, 2 == bar());
    std::string s = "test";
    assert_eq(s, "test2");
    assert_eq(s[0], 'c');
    assert_eq(BLUE "test" RESET, "test2");
    assert_eq(0xf, 16);
    assert_eq(true, false);
    assert_eq(true ? true : false, false);
	assert_eq(0b100, 0x3);
	assert_eq(0b1000000, 0x3);
	//assert(nullptr);
	void* foo = (void*)0xdeadbeef;
	assert_eq(foo, (int*)nullptr);
	assert_eq(S<S<int>>(2), S<S<int>>(4));
	S<S<int>> a(1), b(2);
	assert_eq(a, b);
	const S<S<int>> c(4), d(8);
	assert_eq(c, d);
	S<void> e, f;
	assert_eq(e, f);
    assert_eq(1, 2);
    assert_eq(&a, nullptr);
    assert_eq((uintptr_t)&a, 0ULL & 0ULL);
    assert_and(&a, nullptr);
    assert_and(nullptr && nullptr, nullptr);
    assert_and(&a, nullptr && nullptr);
    assert_and((bool)nullptr && (bool)nullptr, (bool)nullptr);
    assert_and((uintptr_t)&a, (bool)nullptr && (bool)nullptr); // FIXME: parentheses
	char* buffer = nullptr;
	char thing[] = "foo";
	assert_eq(buffer, thing);
	::foo();
	assert_eq(0x12p2, 12);
	assert(true); // this should cause a primitive_assert fail
}

// TODO: syntax highlighting
