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

struct P {
	std::string str;
	P() = default;
	P(const P&) = delete;
	P(P&&) = default;
	P& operator=(const P&) = delete;
	P& operator=(P&&) = delete;
	bool operator==(const P& p) const { return str == p.str; }
	friend std::ostream& operator<<(std::ostream& o, const P& p) {
		o<<p.str;
		return o;
	}
};

void foo();
int bar() {
	return 2;
}

int main() {
	// demo section
	assert(false, "code should never do <xyz>");
	assert(false);
    std::map<int, int> map {{1,1}};
    assert(map.count(1) == 2);
	assert(map.count(1) >= 2 * bar(), "some data not received");
	assert(.1f == .1);
	assert_eq(0, 2 == bar());
	assert(0 == (2 == bar()));
    std::string s = "test";
    assert(s == "test2");
    assert(s[0] == 'c');
    assert(BLUE "test" RESET == "test2");
	assert(0b1000000 == 0x3);
	assert(S<S<int>>(2) == S<S<int>>(4));
    {
		S<void> e, f;
		assert(e == f);
	}
	char* buffer = nullptr;
	char thing[] = "foo";
	assert_eq(buffer, thing);

	

	// tests useful during development
    assert_gteq(map.count(1 == 1), 2);
    assert_eq(map.count(1), 2, "some data not received");
    assert_gteq(map.count(2 * bar()), 2, "some data not received");
    assert(1 == 1.5);
	assert(.1 == 2);
	assert(0xf == 16);
    assert(true == false);
    assert(true ? false : true == false);
	assert(0b100 == 0x3);
	assert(0b1000000 == 0x3);
	void* foo = (void*)0xdeadbeef;
	assert_eq(foo, (int*)nullptr);
	assert(S<S<int>>(2) == S<S<int>>(4));
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
	::foo();
	assert_eq(0x12p2, 12);
	assert_eq(0x12p2, 0b10);

	assert(0 == (2  ==  bar()));
	//assert(0 == 2 == bar());
	float ff = .1f;
	assert(ff == .1);
	const float&& gg = .1f;
	assert(gg == .1);
	assert(1 < 1 < 0);
	assert(0 + 0 + 0);
	assert(false == false == false);
	assert(1 << 1 == 200);
	assert(1 << 1 << 31);
	assert(true ? false : true, "pffft");
	{
		std::string x = "aa";
		std::string y = "bb";
		assert(x == y);
	}
	{
		P x {"aa"};
		P y {"bb"};
		assert(x == y);
	}
	{
		P x {"aa"};
		assert(x == P {"bb"});
	}
	{
		const P x {"aa"};
		assert(x == P {"bb"});
	}
	assert(false);
	int v = 1, p = 2;
	assert_eq(v, p);
	int x = 2;
	assert(x -= 2);
	x = 2;
	assert(x -= x -= 1);
	x = 2;
	assert(x -= x -= x -= 1);
	//assert(assert_impl_::always_false<void> == true);

	assert(true); // this should lead to another assert(false) because we're in demo mode
}
