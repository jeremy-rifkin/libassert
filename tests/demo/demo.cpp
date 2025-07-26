#define _CRT_SECURE_NO_WARNINGS // for fopen

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <iterator>
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

#include <libassert/assert.hpp>
#include <libassert/version.hpp>

#ifdef _MSC_VER
 // disable unsafe use of bool warning msvc
 #pragma warning(disable: 4806)
#endif

void qux();
void wubble();

void custom_fail(const libassert::assertion_info& assertion) {
    std::cerr<<assertion.to_string(libassert::terminal_width(libassert::stderr_fileno))<<std::endl<<std::endl;
}

int garple() {
    return 2;
}

void rec(int n) {
    if(n == 0) ASSERT(false);
    else rec(n - 1);
}

void recursive_a(int), recursive_b(int);

void recursive_a(int n) {
    if(n == 0) ASSERT(false);
    else recursive_b(n - 1);
}

void recursive_b(int n) {
    if(n == 0) ASSERT(false);
    else recursive_a(n - 1);
}

auto min_items() {
    return 10;
}

void zoog(const std::map<std::string, int>& map) {
    #if __cplusplus >= 202002L
     ASSERT(map.contains("foo"), "expected key not found", map);
    #else
     ASSERT(map.count("foo") != 1, "expected key not found", map);
    #endif
    ASSERT(map.at("bar") >= 0, "unexpected value for foo in the map", map);
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

namespace demo {
    void basic() {
        ASSERT(1 == 2);
        DEBUG_ASSERT(1 == 2);
        assert(1 == 2);
        debug_assert(1 == 2);
        ASSERT(1 == 2, "Messages");
        // PANIC();
    }

    void numbers() {
        ASSERT(18446744073709551606ULL == -10);
        ASSERT(get_mask() == 0b00001101);
        ASSERT(0xf == 16);
        long long x = -9'223'372'036'854'775'807;
        ASSERT(x & 0x4);
        ASSERT(!x and true == 2);
    }

    void strings() {
        std::string s = "test\n";
        int i = 0;
        ASSERT(s == "test");
        ASSERT(s[i] == 'c', "", s, i);
    }

    void recursion() {
        rec(10);
        recursive_a(10);
    }

    void containers() {
        std::set<int> a = { 2, 2, 4, 6, 10 };
        std::set<int> b = { 2, 2, 5, 6, 10 };
        std::vector<double> c = { 1.2, 2.44, 3.15159, 5.2 };
        ASSERT(a == b, c);
        ASSERT(a == b, ([&] {
            std::vector<int> diff;
            std::set_symmetric_difference(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(diff));
            return diff;
        } ()));
        std::map<std::string, int> m0 = {
            {"foo", 2},
            {"bar", -2}
        };
        ASSERT(false, m0);
        std::map<std::string, std::vector<int>> m1 = {
            {"foo", {1, -2, 3, -4}},
            {"bar", {-100, 200, 400, -800}}
        };
        ASSERT(false, m1);
        auto t = std::make_tuple(1, 0.1 + 0.2, "foobars");
        ASSERT(false, t);
        std::array<int, 10> arr = {1,2,3,4,5,6,7,8,9,10};
        ASSERT(false, arr);
        std::map<int, int> map {{1,1}};
        ASSERT(map.count(1) == 2);
        ASSERT(map.count(1) >= 2 * garple(), "Error while doing XYZ");
        zoog({
            //{ "foo", 2 },
            { "bar", -2 },
            { "baz", 20 }
        });
        std::vector<int> vec = {2, 3, 5, 7, 11, 13};
        ASSERT(vec.size() > min_items(), "vector doesn't have enough items", vec);
        std::optional<float> parameter;
        if(auto i = *ASSERT_VAL(parameter)) {
            static_assert(std::is_same<decltype(i), float>::value);
        }
        [[maybe_unused]] float f = *assert_val(get_param());
        auto x = [&] () -> decltype(auto) { return ASSERT_VAL(parameter); };
        static_assert(std::is_same<decltype(x()), std::optional<float>&>::value);
    }

    void cerrno() {
        const char* path = "/home/foobar/baz";
        ASSERT(open(path, O_RDONLY) >= 0, "Internal error with foobars", errno, path);
        [[maybe_unused]] int fd = open(path, O_RDONLY);
        ASSERT(fd >= 0, "Internal error with foobars", errno, path);
        [[maybe_unused]] FILE* f = ASSERT_VAL(fopen(path, "r") != nullptr, "Internal error with foobars", errno, path);
    }

    void misc() {
        wubble();
        std::string s = "h1eLlo";
        [[maybe_unused]] auto it = std::find_if(
            s.begin(),
            s.end(),
            [](char c) {
                ASSERT(not isdigit(c), c);
                return c >= 'A' and c <= 'Z';
            }
        );
        ASSERT(true ? false : true == false);
        ASSERT(true ? false : true, "pffft");
        ASSERT(0, 2 == garple());
    }

    void diff() {
        libassert::set_diff_highlighting(true);
        std::string s1 = "Lorem ipsum sit amet";
        std::string s2 = "Lorem ipsum dolor sit amet";
        ASSERT(s1 == s2);
        auto a = std::nextafter(0.3, 0.4);
        auto b = std::nextafter(a, 0.4);
        ASSERT(a == b);
        libassert::set_diff_highlighting(false);
    }
}

void foo() {
    demo::basic();
    demo::numbers();
    demo::strings();
    demo::recursion();
    demo::containers();
    demo::cerrno();
    demo::misc();
    demo::diff();
}

int main() {
    libassert::enable_virtual_terminal_processing_if_needed();
    libassert::set_failure_handler(custom_fail);
    foo();
}
