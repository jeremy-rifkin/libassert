#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <libassert/assert.hpp>

using namespace std::literals;

void test_path_differentiation();

void custom_fail(const libassert::assertion_info& assertion) {
    std::cout<<assertion.to_string(0, libassert::color_scheme::blank)<<std::endl<<std::endl;
    if(assertion.type == libassert::assert_type::panic) {
        throw std::runtime_error("foobar");
    }
}

struct debug_print_customization {
    int x;
    debug_print_customization(int t) : x(t) {}
    bool operator==(const debug_print_customization& other) const {
        return x == other.x;
    }
    friend std::ostream& operator<<(std::ostream& stream, const debug_print_customization&) {
        return stream<<"wrong print";
    }
};

template<> struct libassert::stringifier<debug_print_customization> {
    std::string stringify(const debug_print_customization& p) {
        return "(debug_print_customization = " + std::to_string(p.x) + ")";
    }
};

int foo() {
    std::cout<<"foo() called"<<std::endl;
    return 2;
}
int bar() {
    std::cout<<"bar() called"<<std::endl;
    return -2;
}

namespace complex_typing {
    struct S {
        void foo(int, std::string, void***, void* (*)(int), void(*(*)(int))(), void(*(*(*)(int))[5])());
    };
}

#line 500
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

#define SECTION(s) std::cout<<"===================== ["<<(s)<<"] ====================="<<std::endl;

// disable unsafe use of bool warning msvc
#ifdef _MSC_VER
 #pragma warning(disable: 4804)
#endif

// TODO: need to check assert, verify, and check...?
// Opt/DNDEBUG

#line 1000
template<typename T>
class test_class {
public:
    template<typename S> void something([[maybe_unused]] std::pair<S, T> x) {
        something_else();
    }

    void something_else() {
        // general stack traces are tested throughout
        // simple recursion
        SECTION("simple recursion");
        #line 2600
        rec(10); // FIXME: Check stacktrace in clang
        // other recursion
        SECTION("other recursion");
        #line 2700
        recursive_a(10); // FIXME: Check stacktrace in clang

        // path differentiation
        SECTION("Path differentiation");
        #line 2800
        test_path_differentiation();

        SECTION("Type cleaning"); // also pretty thoughroughly tested above aside from std::string_view
        #line 3200
        {
            test_pretty_function_cleaning({});
        }

        SECTION("Complex type resolution");
        #line 3500
        {
            const volatile std::vector<std::string>* ptr = nullptr;
            test_complex_typing(ptr, nullptr, "foo", nullptr, nullptr);
        }
    }

    #line 3300
    void test_pretty_function_cleaning(const std::map<std::string, std::vector<std::string_view>>& other) {
        std::map<std::string, std::vector<std::string_view>> map = {
            {"foo", {"f1", "f3", "f5"}},
            {"bar", {"b1", "b3", "b5"}}
        };
        debug_assert(map == other);
    }

    #line 3600
    void test_complex_typing(const volatile std::vector<std::string>* const &, int[], const char(&)[4],
                             decltype(&complex_typing::S::foo), int complex_typing::S::*) {
        debug_assert(false);
    }
};

struct N { };

#line 400
int main() {
    libassert::set_failure_handler(custom_fail);
    libassert::set_color_scheme(libassert::color_scheme::blank);
    test_class<int> t;
    #line 402
    t.something(std::pair {N(), 1});
}
