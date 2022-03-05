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

void qux();
void wubble();

#define ESC "\033["
#define RED ESC "1;31m"
#define GREEN ESC "1;32m"
#define BLUE ESC "1;34m"
#define CYAN ESC "1;36m"
#define PURPL ESC "1;35m"
#define DARK ESC "1;30m"
#define RESET ESC "0m"

void custom_fail(std::string message, assert_detail::assert_type, assert_detail::ASSERTION) {
	std::cerr<<message<<std::endl<<std::endl;
}

static std::string indent(const std::string_view str, size_t depth, char c = ' ', bool ignore_first = false) {
	size_t i = 0, j;
	std::string output;
	while((j = str.find('\n', i)) != std::string::npos) {
		if(i != 0 || !ignore_first) output.insert(output.end(), depth, c);
		output.insert(output.end(), str.begin() + i, str.begin() + j + 1);
		i = j + 1;
	}
	if(i != 0 || !ignore_first) output.insert(output.end(), depth, c);
	output.insert(output.end(), str.begin() + i, str.end());
	return output;
}

template<class T> struct S {
	T x;
	S() = default;
	S(T&& x) : x(std::forward<T>(x)) {}
	// moveable, not copyable
	S(const S&) = delete;
	S(S&&) = default;
	S& operator=(const S&) = delete;
	S& operator=(S&&) = default;
	bool operator==(const S& s) const { return x == s.x; }
	friend std::ostream& operator<<(std::ostream& o, const S& s) {
		o<<"I'm S<"<<assert_detail::type_name<T>()<<"> and I contain:"<<std::endl;
		std::ostringstream oss;
		oss<<s.x;
		o<<indent(std::move(oss).str(), 4);
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

struct M {
	M() = default;
	compl M() {
		puts("M::compl M(); called");
	}
	M(const M&) = delete;
	M(M&&) = default; // only move-constructable
	M& operator=(const M&) = delete;
	M& operator=(M&&) = delete;
	bool operator<(int) const & {
		puts("M::operator<(int)& called");
		return false;
	}
	bool operator<(int) const && {
		puts("M::operator<(int)&& called");
		return false;
	}
};

int garple() {
	return 2;
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

auto min_items() {
	return 10;
}

void zoog(std::vector<int>& vec) {
	assert(vec.size() > min_items(), "vector doesn't have enough items");
	assert(vec.size() > 7);
}

#define O_RDONLY 0
int open(const char*, int) {
	return -1;
}

std::optional<float> get_param() {
	return {};
}

int get_mask() {
	return 0b00101101;
}

class foo {
public:
	template<typename> void bar([[maybe_unused]] std::pair<int, int> x) {
		baz();
	}

	void baz() {
		puts("");
		// General demos
		{
			std::vector<int> vec = {1, 2, 3, 4, 5, 6, 7};
			zoog(vec);
		}
		const char* path = "/home/foobar/baz";
		{
			int fd = open(path, O_RDONLY);
			assert(fd >= 0, "Internal error with foobars", errno, path);
			ASSERT_DETAIL_PHONY_USE(fd);
		}
		{
			assert(open(path, O_RDONLY) >= 0, "Internal error with foobars", errno, path);
		}
		{
			FILE* f = VERIFY(fopen(path, "r") != nullptr, "Internal error with foobars", errno, path);
			ASSERT_DETAIL_PHONY_USE(f);
		}
		assert(false, "Error while doing XYZ");
		assert(false);
		CHECK((puts("CHECK called") && false));

		{
			std::map<int, int> map {{1,1}};
			assert(map.count(1) == 2);
			assert(map.count(1) >= 2 * garple(), "Error while doing XYZ");
		}
		assert(0, 2 == garple());
		{
			std::optional<float> parameter;
			#ifndef _MSC_VER
			 if(auto i = *VERIFY(parameter)) {
			 	static_assert(std::is_same<decltype(i), float>::value);
			 }
			 float f = *assert(get_param());
			#else
			 VERIFY(parameter);
			 assert(get_param());
			#endif
			auto x = [&] () -> decltype(auto) { return VERIFY(parameter); };
			static_assert(std::is_same<decltype(x()), std::optional<float>&>::value);
		}
		
		qux();

		{
			M() < 2;
			puts("----");
			assert(M() < 2);
			puts("----");
			M m;
			puts("----");
			assert(m < 2);
			puts("----");
		}


		assert(true ? false : true == false);
		assert(true ? false : true, "pffft");

		wubble();

		rec(10);

		recursive_a(10);

		assert(18446744073709551606ULL == -10);

		assert(get_mask() == 0b00001101);
		assert(0xf == 16);

		{
			std::string s = "test\n";
			int i = 0;
			assert(s == "test");
			assert(s[i] == 'c', "", s, i);
		}
		{
			assert(S<S<int>>(2) == S<S<int>>(4));
			S<void> e, f;
			assert(e == f);
		}

		long long x = -9'223'372'036'854'775'807;
		assert(x & 0x4);

		assert(!x and true == 2);
		assert((puts("A"), false) && (puts("B"), false));

		{
			std::string s = "h1eLlo";
			assert(std::find_if(s.begin(), s.end(), [](char c) {
				assert(not isdigit(c), c);
				return c >= 'A' and c <= 'Z';
			}) == s.end());
		}

		// Numeric
		/*
		// Tests useful during development
		/*assert(.1f == .1);
		assert(1.0 == 1.0 + std::numeric_limits<double>::epsilon());
		ASSERT_EQ(0x12p2, 12);
		ASSERT_EQ(0x12p2, 0b10);
		assert(0b1000000 == 0x3);
		assert(.1 == 2);
		assert(true == false);
		assert(true ? false : true == false);
		assert(0b100 == 0x3);

		assert(0 == (2 == garple()));
		ASSERT_GTEQ(map.count(1 == 1), 2);
		ASSERT_EQ(map.count(1), 2, "Error while doing XYZ");
		ASSERT_GTEQ(map.count(2 * garple()), 2, "Error while doing XYZ");
		assert(S<S<int>>(2) == S<S<int>>(4));
		S<S<int>> a(1), b(2);
		ASSERT_EQ(a, b);
		const S<S<int>> c(4), d(8);
		ASSERT_EQ(c, d);
		S<void> g, h;
		ASSERT_EQ(g, h);
		ASSERT_EQ(1, 2);
		ASSERT_EQ(&a, nullptr);
		ASSERT_EQ((uintptr_t)&a, 0ULL & 0ULL);
		ASSERT_AND(&a, nullptr);
		ASSERT_AND(nullptr && nullptr, nullptr);
		ASSERT_AND(&a, nullptr && nullptr);
		ASSERT_AND((bool)nullptr && (bool)nullptr, (bool)nullptr);
		ASSERT_AND((uintptr_t)&a, (bool)nullptr && (bool)nullptr); // FIXME: parentheses
		ASSERT_EQ(foo, (int*)nullptr);


		assert(0 == (2  ==  garple()));
		//assert(0 == 2 == garple());
		
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
		assert((42 & 3U) == 1UL);
		
		assert([](int a, int b) {
			return a + b;
		} (10, 32) not_eq 42);
		assert([](){return 42;}() not_eq 42);
		assert([&]<typename T>(T a, T b){return a+b;}(10, 32) not_eq 42);
		ASSERT_NEQ([](int a, int b) {
			return a + b;
		} (10, 32), 42);
		assert('\n' == '\t');
		assert(<:](){return 42;%>() not_eq 42);

		assert(&a == nullptr);
		
		{
			std::string s = "h1ello";
			assert(std::find_if(s.begin(), s.end(), [](char c) {
				if(c == '1') assert(c != '1');
				//assert(!isdigit(c), c);
				return c == 'e';
			}) == s.end());
		}

		assert(0.1 == 0.2);
		assert(.1 == 0.2);
		assert(.1f == 0.2);

		assert(true); // this should lead to another assert(false) because we're in demo mode*/
	}
};

namespace assert_detail {
	void enable_virtual_terminal_processing_if_needed();
}

int main() {
	assert_detail::enable_virtual_terminal_processing_if_needed();
	foo f;
	f.bar<int>({});
}
