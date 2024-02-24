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

#include <assert/assert.hpp>

using namespace std::literals;

void test_path_differentiation();

void custom_fail(libassert::assert_type, const libassert::assertion_printer& printer) {
    std::cout<<printer(0, {})<<std::endl<<std::endl;
}

template<typename T>
struct printable {
    std::optional<T> f;
    printable(T t) : f(t) {}
    bool operator==(const printable& other) const {
        return f == other.f;
    }
};

template<typename T>
struct not_printable {
    std::optional<T> f;
    not_printable(T t) : f(t) {}
    bool operator==(const not_printable& other) const {
        return f == other.f;
    }
};

template<typename T>
std::ostream& operator<<(std::ostream& stream, const printable<T>& p) {
    return stream<<"(printable = "<<*p.f<<")";
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

struct logger_type {
    int n;
    explicit logger_type(int _n): n(_n) {
        std::cout<<"logger_type::logger_type() [n="<<n<<"]"<<std::endl;
    }
    bool operator==(const logger_type& other) const & {
        std::cout<<"logger_type::operator==(const logger_type&) const & [n="<<n<<", other="<<other.n<<"]"<<std::endl;
        return false;
    }
    bool operator==(const logger_type& other) const && {
        std::cout<<"logger_type::operator==(const logger_type&) const && [n="<<n<<", other="<<other.n<<"]"<<std::endl;
        return false;
    }
    bool operator==(int other) const & {
        std::cout<<"logger_type::operator==(int) const & [n="<<n<<", other="<<other<<"]"<<std::endl;
        return false;
    }
    bool operator==(int other) const && {
        std::cout<<"logger_type::operator==(int) const && [n="<<n<<", other="<<other<<"]"<<std::endl;
        return false;
    }
};
std::ostream& operator<<(std::ostream& stream, const logger_type& lt) {
    return stream<<"logger_type [n = "<<lt.n<<"]";
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
        // FIXME: Check all stack traces on msvc... Bug with __PRETTY_FUNCTION__ in lambdas.
        // value printing: strings
        SECTION("value printing: strings");
        #line 1100
        {
            std::string s = "test\n";
            int i = 0;
            debug_assert(s == "test");
            debug_assert(s[i] == 'c', "", s, i);
            char* buffer = nullptr;
            char thing[] = "foo";
            debug_assert(buffer == thing);
            debug_assert(buffer == +thing);
            std::string_view sv = "foo";
            debug_assert(s == sv);
        }
        // value printing: pointers
        SECTION("value printing: pointers");
        #line 1200
        {
            debug_assert((uintptr_t)-1 == 0xff);
            debug_assert((uintptr_t)-1 == (uintptr_t)0xff);
            void* foo = (void*)0xdeadbeefULL;
            debug_assert(foo == nullptr);
        }
        // value printing: number formats
        SECTION("value printing: number formats");
        #line 1300
        {
            const uint16_t flags = 0b000101010;
            const uint16_t mask = 0b110010101;
            debug_assert(mask bitand flags);
            debug_assert(0xf == 16);
        }
        // value printing: floating point
        SECTION("value printing: floating point");
        #line 1400
        {
            debug_assert(1 == 1.5);
            debug_assert(0.5 != .5); // FIXME
            debug_assert(0.1 + 0.2 == 0.3);
            ASSERT(.1 + .2 == .3);
            debug_assert(0.1f + 0.2f == 0.3f);
            float ff = .1f;
            debug_assert(ff == .1);
            debug_assert(.1f == .1);
        }
        // value printing: ostream overloads
        SECTION("value printing: ostream overloads");
        #line 1500
        {
            printable p{1.42};
            debug_assert(p == printable{2.55});
        }
        // value printing: no ostream overload
        SECTION("value printing: no ostream overload");
        #line 1600
        {
            const not_printable p{1.42};
            debug_assert(p == not_printable{2.55});
            debug_assert(p.f == not_printable{2.55}.f);
        }

        // optional messages
        SECTION("optional messages");
        #line 1700
        {
            debug_assert(false, 2);
            debug_assert(false, "foo");
            debug_assert(false, "foo"s);
            debug_assert(false, "foo"sv);
            debug_assert(false, (char*)"foo");
            debug_assert(false, "foo", 2);
            debug_assert(false, "foo"s, 2);
            debug_assert(false, "foo"sv, 2);
            debug_assert(false, (char*)"foo", 2);
            debug_assert(false, nullptr);
            debug_assert(false, (char*)nullptr);
        }
        // extra diagnostics
        SECTION("errno");
        #line 1800
        {
            errno = 2;
            debug_assert(false, errno);
            errno = 2;
            debug_assert(false, "foo", errno);
        }
        // general
        SECTION("general");
        #line 1900
        {
            debug_assert(false, "foo", false, 2 * foo(), "foobar"sv, bar(), printable{2.55});
            debug_assert([] { return false; } ());
        }
        // safe comparisons
        SECTION("safe comparisons");
        #line 2000
        {
            debug_assert(18446744073709551606ULL == -10);
            debug_assert(-1 > 1U);
        }

        // expression decomposition
        SECTION("expression decomposition");
        #line 2100
        {
            //DEBUG_ASSERT(1 = (1 bitand 2)); // <- FIXME: Not evaluated correctly
            debug_assert(1 == (1 bitand 2));
            debug_assert(1 < 1 < 0);
            debug_assert(0 + 0 + 0);
            debug_assert(false == false == false);
            debug_assert(1 << 1 == 200);
            debug_assert(1 << 1 << 31);
            int x = 2;
            debug_assert(x -= 2);
            x = 2;
            debug_assert(x -= x -= 1); // TODO: double check....
            x = 2;
            debug_assert(x -= x -= x -= 1); // TODO: double check....
            debug_assert(true ? false : true, "pffft");
            int a = 1; // regression test for #26
            debug_assert(a >> 1);
        }
        // ensure values are only computed once
        SECTION("ensure values are only computed once");
        #line 2200
        {
            debug_assert(foo() < bar());
        }
        // value forwarding: lifetimes
        // TODO
        // value forwarding: lvalue references
        SECTION("value forwarding: lvalue references");
        #line 2400
        {
            {
                logger_type lt(2);
                decltype(auto) x = get_lt_a(lt);
                x.n++;
                debug_assert(lt == 1);
                debug_assert(x == 1);
            }
            std::cout<<"--------------------------------------------"<<std::endl<<std::endl;
            {
                int x = 1;
                debug_assert(x ^= 1);
                //debug_assert(x ^= 1) |= 0xf0; // FIXME
                //debug_assert(x == 0);
            }
        }
        // value forwarding: rvalues
        SECTION("value forwarding: rvalues");
        #line 2500
        {
            {
                auto x = get_lt_b();
                debug_assert(false, x);
            }
            std::cout<<"--------------------------------------------"<<std::endl<<std::endl;
        }

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

        SECTION("Enum handling");
        #line 2900
        {
            enum foo { A, B };
            enum class bar { A, B };
            foo a = A;
            debug_assert(a != A);
            bar b = bar::A;
            debug_assert(b != bar::A);
        }

        SECTION("Literal format handling");
        #line 3000
        {
            debug_assert(0xff == 077);
            debug_assert('x' == 20);
            debug_assert('x' == 'y');
            char c = 'x';
            debug_assert(c == 20);
        }

        SECTION("Container printing");
        #line 3100
        {
            std::set<int> a = { 2, 2, 4, 6, 10 };
            std::set<int> b = { 2, 2, 5, 6, 10 };
            std::vector<double> c = { 1.2f, 2.44f, 3.15159f, 5.2f };
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
            std::array<int, 10> arr = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
            debug_assert(false, arr);
            int carr[] = { 5, 4, 3, 2, 1 };
            debug_assert(false, carr);
        }

        SECTION("Type cleaning"); // also pretty thoughroughly tested above aside from std::string_view
        #line 3200
        {
            test_pretty_function_cleaning({});
        }

        SECTION("Debug stringification customization point");
        #line 3400
        {
            debug_print_customization x = 2, y = 1;
            debug_assert(x == y, x, y);
        }

        SECTION("Complex type resolution");
        #line 3500
        {
            const volatile std::vector<std::string>* ptr = nullptr;
            test_complex_typing(ptr, nullptr, "foo", nullptr, nullptr);
        }

        SECTION("Panics");
        #line 3600
        {
            const std::vector<std::string> vec{"foo", "bar", "baz"};
            PANIC("message", vec);
        }
    }

    #line 600
    decltype(auto) get_lt_a(logger_type& l) {
        return assert_val(l == 2);
    }

    decltype(auto) get_lt_b() {
        return assert_val(logger_type(2) == 2);
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
    libassert::set_color_output(false);
    test_class<int> t;
    #line 402
    t.something(std::pair {N(), 1});
}
