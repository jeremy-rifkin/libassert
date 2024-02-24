#define _CRT_SECURE_NO_WARNINGS // for fopen

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <assert/assert.hpp>

#define ESC "\033["
#define RED ESC "1;31m"
#define GREEN ESC "1;32m"
#define BLUE ESC "1;34m"
#define CYAN ESC "1;36m"
#define PURPL ESC "1;35m"
#define DARK ESC "1;30m"
#define RESET ESC "0m"

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

void qux();
void wubble();

void custom_fail(libassert::assert_type, const libassert::assertion_info& assertion) {
    std::cerr<<assertion.to_string(libassert::terminal_width(STDERR_FILENO), libassert::color_scheme::ansi_rgb)<<std::endl<<std::endl;
}

static std::string indent(const std::string_view str, size_t depth, char c = ' ', bool ignore_first = false) {
    size_t i = 0, j;
    std::string output;
    while((j = str.find('\n', i)) != std::string::npos) {
        if(i != 0 || !ignore_first) {
            output.insert(output.end(), depth, c);
        }
        output.insert(output.end(), str.begin() + i, str.begin() + j + 1);
        i = j + 1;
    }
    if(i != 0 || !ignore_first) {
        output.insert(output.end(), depth, c);
    }
    output.insert(output.end(), str.begin() + i, str.end());
    return output;
}

template<class T> struct S {
    T x;
    S() = default;
    ~S() = default;
    S(T&& _x) : x(std::forward<T>(_x)) {}
    // moveable, not copyable
    S(const S&) = delete;
    S(S&&) noexcept = default;
    S& operator=(const S&) = delete;
    S& operator=(S&&) noexcept = default;
    bool operator==(const S& s) const { return x == s.x; }
    friend std::ostream& operator<<(std::ostream& o, const S& s) {
        o<<"I'm S<"<<libassert::detail::type_name<T>()<<"> and I contain:"<<std::endl;
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
    ~P() = delete;
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
    ~M() {
        puts("M::~M(); called");
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
    if(n == 0) debug_assert(false);
    else rec(n - 1);
}

void recursive_a(int), recursive_b(int);

void recursive_a(int n) {
    if(n == 0) debug_assert(false);
    else recursive_b(n - 1);
}

void recursive_b(int n) {
    if(n == 0) debug_assert(false);
    else recursive_a(n - 1);
}

auto min_items() {
    return 10;
}

void zoog(std::map<std::string, int> map) {
    #if __cplusplus >= 202002L
     debug_assert(map.contains("foo"), "expected key not found", map);
    #else
     debug_assert(map.count("foo") != 1, "expected key not found", map);
    #endif
    DEBUG_ASSERT(map.at("bar") >= 0, "unexpected value for foo in the map", map);
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

// disable unsafe use of bool warning msvc
#ifdef _MSC_VER
 #pragma warning(disable: 4806)
#endif

class foo {
public:
    template<typename> void bar([[maybe_unused]] std::pair<int, int> x) {
        baz();
    }

    void baz() {
        puts("");
        // General demos
        {
            zoog({
                //{ "foo", 2 },
                { "bar", -2 },
                { "baz", 20 }
            });
            std::vector<int> vec = {2, 3, 5, 7, 11, 13};
            debug_assert(vec.size() > min_items(), "vector doesn't have enough items", vec);
        }
        const char* path = "/home/foobar/baz";
        {
            int fd = open(path, O_RDONLY);
            debug_assert(fd >= 0, "Internal error with foobars", errno, path);
            LIBASSERT_PHONY_USE(fd);
        }
        {
            debug_assert(open(path, O_RDONLY) >= 0, "Internal error with foobars", errno, path);
        }
        {
            FILE* f = ASSERT_VAL(fopen(path, "r") != nullptr, "Internal error with foobars", errno, path);
            LIBASSERT_PHONY_USE(f);
        }
        debug_assert(false, "Error while doing XYZ");
        debug_assert(false);
        DEBUG_ASSERT((puts("DEBUG_ASSERT called") && false));

        {
            std::map<int, int> map {{1,1}};
            debug_assert(map.count(1) == 2);
            debug_assert(map.count(1) >= 2 * garple(), "Error while doing XYZ");
        }
        debug_assert(0, 2 == garple());
        {
            std::optional<float> parameter;
            #ifndef _MSC_VER
             if(auto i = *ASSERT_VAL(parameter)) {
                 static_assert(std::is_same<decltype(i), float>::value);
             }
             float f = *assert_val(get_param());
             (void)f;
            #else
             ASSERT(parameter);
             debug_assert(get_param());
            #endif
            auto x = [&] () -> decltype(auto) { return ASSERT_VAL(parameter); };
            static_assert(std::is_same<decltype(x()), std::optional<float>&>::value);
        }

        qux();

        {
            M() < 2;
            puts("----");
            debug_assert(M() < 2);
            puts("----");
            M m;
            puts("----");
            debug_assert(m < 2);
            puts("----");
        }


        debug_assert(true ? false : true == false);
        debug_assert(true ? false : true, "pffft");

        wubble();

        rec(10);

        recursive_a(10);

        debug_assert(18446744073709551606ULL == -10);

        debug_assert(get_mask() == 0b00001101);
        debug_assert(0xf == 16);

        {
            std::string s = "test\n";
            int i = 0;
            debug_assert(s == "test");
            debug_assert(s[i] == 'c', "", s, i);
        }
        {
            debug_assert(S<S<int>>(2) == S<S<int>>(4));
            S<void> e, f;
            debug_assert(e == f);
        }

        long long x = -9'223'372'036'854'775'807;
        debug_assert(x & 0x4);

        debug_assert(!x and true == 2);
        debug_assert((puts("A"), false) && (puts("B"), false));

        {
            #if defined(__GNUC__) || defined(__GNUG__) // gcc/clang
             #pragma GCC diagnostic ignored "-Wshadow"
            #endif
            std::string s = "h1eLlo";
            debug_assert(std::find_if(s.begin(), s.end(), [](char c) {
                debug_assert(not isdigit(c), c);
                return c >= 'A' and c <= 'Z';
            }) == s.end());
        }

        {
            std::set<int> a = { 2, 2, 4, 6, 10 };
            std::set<int> b = { 2, 2, 5, 6, 10 };
            std::vector<double> c = { 1.2, 2.44, 3.15159, 5.2 };
            debug_assert(a == b, c);
            std::map<std::string, int> m0 = {
                {"foo", 2},
                {"bar", -2}
            };
            debug_assert(false, m0);
            std::map<std::string, std::vector<int>> m1 = {
                {"foo", {1, -2, 3, -4}},
                {"bar", {-100, 200, 400, -800}}
            };
            debug_assert(false, m1);
            auto t = std::make_tuple(1, 0.1 + 0.2, "foobars");
            debug_assert(false, t);
            std::array<int, 10> arr = {1,2,3,4,5,6,7,8,9,10};
            debug_assert(false, arr);
        }

        // Numeric
        // Tests useful during development
        /*debug_assert(.1f == .1);
        debug_assert(1.0 == 1.0 + std::numeric_limits<double>::epsilon());
        ASSERT_EQ(0x12p2, 12);
        ASSERT_EQ(0x12p2, 0b10);
        debug_assert(0b1000000 == 0x3);
        debug_assert(.1 == 2);
        debug_assert(true == false);
        debug_assert(true ? false : true == false);
        debug_assert(0b100 == 0x3);

        debug_assert(0 == (2 == garple()));
        ASSERT_GTEQ(map.count(1 == 1), 2);
        ASSERT_EQ(map.count(1), 2, "Error while doing XYZ");
        ASSERT_GTEQ(map.count(2 * garple()), 2, "Error while doing XYZ");
        debug_assert(S<S<int>>(2) == S<S<int>>(4));
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


        debug_assert(0 == (2  ==  garple()));
        //debug_assert(0 == 2 == garple());

        debug_assert(true ? false : true, "pffft");
        {
            std::string x = "aa";
            std::string y = "bb";
            debug_assert(x == y);
        }
        {
            P x {"aa"};
            P y {"bb"};
            debug_assert(x == y);
        }
        {
            P x {"aa"};
            debug_assert(x == P {"bb"});
        }
        {
            const P x {"aa"};
            debug_assert(x == P {"bb"});
        }
        debug_assert((42 & 3U) == 1UL);

        debug_assert([](int a, int b) {
            return a + b;
        } (10, 32) not_eq 42);
        debug_assert([](){return 42;}() not_eq 42);
        debug_assert([&]<typename T>(T a, T b){return a+b;}(10, 32) not_eq 42);
        ASSERT_NEQ([](int a, int b) {
            return a + b;
        } (10, 32), 42);
        debug_assert('\n' == '\t');
        debug_assert(<:](){return 42;%>() not_eq 42);

        debug_assert(&a == nullptr);

        {
            std::string s = "h1ello";
            debug_assert(std::find_if(s.begin(), s.end(), [](char c) {
                if(c == '1') debug_assert(c != '1');
                //debug_assert(!isdigit(c), c);
                return c == 'e';
            }) == s.end());
        }

        debug_assert(0.1 == 0.2);
        debug_assert(.1 == 0.2);
        debug_assert(.1f == 0.2);

        debug_assert(true); // this should lead to another debug_assert(false) because we're in demo mode*/
    }
};

namespace libassert::detail {
    LIBASSERT_EXPORT void enable_virtual_terminal_processing_if_needed();
}

int main() {
    libassert::detail::enable_virtual_terminal_processing_if_needed();
    libassert::set_failure_handler(custom_fail);
    foo f;
    f.bar<int>({});
}
