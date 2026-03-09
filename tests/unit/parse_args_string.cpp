#include <iostream>
#include <string_view>
#include <string>
#include <vector>

#include "analysis.hpp"

#ifdef TEST_MODULE
import libassert;
#include <libassert/assert-macros.hpp>
#else
#include <libassert/assert.hpp>
#endif

#define ESC "\033["
#define RED ESC "1;31m"
#define GREEN ESC "1;32m"
#define RESET ESC "0m"

struct test_case {
    std::string_view args_string;
    size_t n_args;
    std::vector<std::string_view> expected;
};

int main() {
    test_case tests[] = {
        // Edge cases: Empty input, n_args=0, wrong n_args counts
        {"", 0, {}},
        {"x", 2, {"", ""}},
        {"x, y", 4, {"", "", "", ""}},
        {"a + b, x, y, z", 2, {"", ""}},
        {"a + b, x, y, z", 3, {"", "", ""}},
        {"foo<1, 2>(3), bar<3, 4>(5), x", 2, {"", ""}},
        {"a<1, 2>(3), b<4, 5>(6), c<7, 8>(9), d", 2, {"", ""}},
        {"a<1, 2>(3), b<4, 5>(6), c<7, 8>(9), d", 3, {"", "", ""}},
        {"a<1, 2> + b", 2, {"a<1", "2> + b"}},
        {"a<1, 2, 3> + b", 2, {"", ""}},
        {"x, a<1, 2> + b, c", 2, {"", ""}},
        {"w, x, a<1, 2> + b, c", 3, {"", "", ""}},
        {"w, x, a<1, 2> + b, c", 2, {"", ""}},
        {"a<1, 2> == b<3, 4>", 2, {"a<1", "2> == b<3, 4>"}},
        {"std::map<int, float>::iterator{} != end", 2, {"std::map<int", "float>::iterator{} != end"}},
        {R"(a < b, c > 0, "msg")", 2, {"", ""}},
        {R"(i < n, j > 0, "loop invariant")", 2, {"", ""}},
        {"a < b, c, d > e", 2, {"", ""}},
        {"std::map<int, int> m, x", 2, {"", ""}},
        {"non-empty but zero args", 0, {}},

        // Single arg, simple expressions, trimming, templates with n=1
        {"false", 1, {"false"}},
        {"1 == 2", 1, {"1 == 2"}},
        {"  foo  ", 1, {"foo"}},
        {"a<1, 2>(3)", 1, {"a<1, 2>(3)"}},
        {R"("")", 1, {R"("")"}},
        {"std::is_same_v<int, double>", 1, {"std::is_same_v<int, double>"}},
        {"MyClass<int, float>::value == 42", 1, {"MyClass<int, float>::value == 42"}},
        {"a<1, 2> == b<3, 4>", 1, {"a<1, 2> == b<3, 4>"}},
        {"std::is_convertible_v<int, double> == true", 1, {"std::is_convertible_v<int, double> == true"}},
        {"std::map<int, float>::iterator{} != end", 1, {"std::map<int, float>::iterator{} != end"}},
        {"a<1, 2> + b", 1, {"a<1, 2> + b"}},

        // Simple splitting, no templates
        {"false, x", 2, {"false", "x"}},
        {"false, x, y", 3, {"false", "x", "y"}},
        {R"(1 == 2, "msg")", 2, {"1 == 2", R"("msg")"}},
        {"a + b, x, y, z", 4, {"a + b", "x", "y", "z"}},
        {"!x, y", 2, {"!x", "y"}},
        {"-a == b, x", 2, {"-a == b", "x"}},
        {"sizeof(int), x", 2, {"sizeof(int)", "x"}},

        // Parentheses, braces, brackets
        {"f(a, b), x", 2, {"f(a, b)", "x"}},
        {"f(a, b, c), x, y", 3, {"f(a, b, c)", "x", "y"}},
        {"{1, 2, 3}, x", 2, {"{1, 2, 3}", "x"}},
        {"a[1, 2], x", 2, {"a[1, 2]", "x"}},
        {"f(g(a, b), h(c, d)), x", 2, {"f(g(a, b), h(c, d))", "x"}},
        {"f(a<1, 2>(3)), x", 2, {"f(a<1, 2>(3))", "x"}},
        {"f(a<1, 2>(3), b<4, 5>(6)), x", 2, {"f(a<1, 2>(3), b<4, 5>(6))", "x"}},
        {"{a<1>(2), b<3>(4)}, x", 2, {"{a<1>(2), b<3>(4)}", "x"}},
        {"a[b<1>(2)], x", 2, {"a[b<1>(2)]", "x"}},
        {"[](int a, int b){ return a + b; }, x", 2, {"[](int a, int b){ return a + b; }", "x"}},

        // String literals
        {R"("hello, world", x)", 2, {R"("hello, world")", "x"}},
        {R"("a,b,c", x, y)", 3, {R"("a,b,c")", "x", "y"}},
        {R"('\x2c', x)", 2, {R"('\x2c')", "x"}},

        // Unambiguous templates, nesting, comparisons
        {"foo<1, 2>(3), x", 2, {"foo<1, 2>(3)", "x"}},
        {"foo<1, 2>(3) == 4, x, y", 3, {"foo<1, 2>(3) == 4", "x", "y"}},
        {"a<b<c, d>>(e), x", 2, {"a<b<c, d>>(e)", "x"}},
        {"foo<1, 2>(3), bar<3, 4>(5)", 2, {"foo<1, 2>(3)", "bar<3, 4>(5)"}},
        {"a<1>() == b<2>(), x", 2, {"a<1>() == b<2>()", "x"}},
        {"foo<1, 2>(3), bar<3, 4>(5), x", 3, {"foo<1, 2>(3)", "bar<3, 4>(5)", "x"}},
        {"a<1>(2), b<3>(4), c<5>(6)", 3, {"a<1>(2)", "b<3>(4)", "c<5>(6)"}},
        {"a<1, 2>(3), b<4, 5>(6), c<7, 8>(9)", 3, {"a<1, 2>(3)", "b<4, 5>(6)", "c<7, 8>(9)"}},
        {"a<1>(2), b<3>(4), c<5>(6), d<7>(8)", 4, {"a<1>(2)", "b<3>(4)", "c<5>(6)", "d<7>(8)"}},
        {"a<b<1, 2>>(3), x", 2, {"a<b<1, 2>>(3)", "x"}},
        {"a<b<c<1, 2>>>(3), x", 2, {"a<b<c<1, 2>>>(3)", "x"}},
        {"a<b<1, 2>, c<3, 4>>(5), x", 2, {"a<b<1, 2>, c<3, 4>>(5)", "x"}},
        {"a<b<1, 2>>(3), c<d<4, 5>>(6)", 2, {"a<b<1, 2>>(3)", "c<d<4, 5>>(6)"}},
        {"foo<1, 2>(3) == 4, x, y, z", 4, {"foo<1, 2>(3) == 4", "x", "y", "z"}},
        {"foo<1, 2>(3) + bar<4, 5>(6), a, b, c, d", 5, {"foo<1, 2>(3) + bar<4, 5>(6)", "a", "b", "c", "d"}},
        {"a<1>(2), b, c, d, e", 5, {"a<1>(2)", "b", "c", "d", "e"}},
        {"static_cast<int>(x), y", 2, {"static_cast<int>(x)", "y"}},
        {"static_cast<int>(x) == 1, msg, y", 3, {"static_cast<int>(x) == 1", "msg", "y"}},
        {"std::get<0>(t), std::get<1>(t), msg", 3, {"std::get<0>(t)", "std::get<1>(t)", "msg"}},
        {"std::make_pair<int, double>(1, 2.0), x", 2, {"std::make_pair<int, double>(1, 2.0)", "x"}},
        {"a<1>(2) == b<3>(4), x", 2, {"a<1>(2) == b<3>(4)", "x"}},
        {"a<1>(2) != b<3>(4), x, y", 3, {"a<1>(2) != b<3>(4)", "x", "y"}},
        {"a<1, 2>(3) >= b<4, 5>(6), x", 2, {"a<1, 2>(3) >= b<4, 5>(6)", "x"}},
        {"foo<1, 2> (3), x", 2, {"foo<1, 2> (3)", "x"}},
        {"foo<>(), x", 2, {"foo<>()", "x"}},
        {"foo<>() == 1, msg", 2, {"foo<>() == 1", "msg"}},
        {"a<b<>>(1), x", 2, {"a<b<>>(1)", "x"}},
        {"a<1>() + b<2>(), x, y", 3, {"a<1>() + b<2>()", "x", "y"}},
        {"a<1>(2), x, y, z", 4, {"a<1>(2)", "x", "y", "z"}},
        {"a<int>(x) == b<int>(y), msg", 2, {"a<int>(x) == b<int>(y)", "msg"}},
        {"a<1, 2>(3), b<4, 5>(6), c<7, 8>(9), d", 4, {"a<1, 2>(3)", "b<4, 5>(6)", "c<7, 8>(9)", "d"}},
        {"a.foo<int>().bar<double>(), x", 2, {"a.foo<int>().bar<double>()", "x"}},

        // More templates, things that would be more ambiguous without comma counts
        {"a<1, 2>, a<1, 2>", 2, {"a<1, 2>", "a<1, 2>"}},
        {"a<1, 2>, b", 2, {"a<1, 2>", "b"}},
        {"a<1, 2> + b, c", 2, {"a<1, 2> + b", "c"}},
        {"a<1, 2> + b, c", 3, {"a<1", "2> + b", "c"}},
        {"a<1, 2> + b, foo", 2, {"a<1, 2> + b", "foo"}},
        {"x, a<1, 2> + b, c", 3, {"x", "a<1, 2> + b", "c"}},
        {"w, x, a<1, 2> + b, c", 4, {"w", "x", "a<1, 2> + b", "c"}},
        {R"(std::is_same_v<int, double>, "type mismatch")", 2, {"std::is_same_v<int, double>", R"("type mismatch")"}},

        // Comparison operators that could look like templates
        {"a < b, c > d", 2, {"a < b", "c > d"}},
        {"a < b, c > 0", 2, {"a < b", "c > 0"}},
        {"size < capacity, used > freed", 2, {"size < capacity", "used > freed"}},
        {R"(a < b, c > 0, "msg")", 3, {"a < b", "c > 0", R"("msg")"}},
        {R"(i < n, j > 0, "loop invariant")", 3, {"i < n", "j > 0", R"("loop invariant")"}},
        {"a < b, c, d > e", 3, {"a < b", "c", "d > e"}},
        {"f() < limit, threshold > 0", 2, {"f() < limit", "threshold > 0"}},
        {R"(a < b && c > d, "msg")", 2, {"a < b && c > d", R"("msg")"}},

        // Trimming
        {"  false  ,  x  ", 2, {"false", "x"}},
        {"  a + b  ,  x  ,  y  ", 3, {"a + b", "x", "y"}},

        // Some practical examples combining multiple features
        {R"(fizz<2, 3>(4) == 2, "Message", x, 42)", 4, {"fizz<2, 3>(4) == 2", R"("Message")", "x", "42"}},
        {R"(vec.size() > 0, "expected non-empty", vec)", 3, {"vec.size() > 0", R"("expected non-empty")", "vec"}},
        {"std::get<0>(t) == 1, t", 2, {"std::get<0>(t) == 1", "t"}},
        {R"("" , "message", vec)", 3, {R"("")", R"("message")", "vec"}},
        {R"("" , x)", 2, {R"("")", "x"}},
        {"vec.size() == static_cast<size_t>(n), vec, n", 3, {"vec.size() == static_cast<size_t>(n)", "vec", "n"}},
        {"std::get<0>(t) == std::get<1>(t), t", 2, {"std::get<0>(t) == std::get<1>(t)", "t"}},
        {"f<1>(g<2>(x)), y", 2, {"f<1>(g<2>(x))", "y"}},
        {"obj.method<int>(x) == 42, obj, x", 3, {"obj.method<int>(x) == 42", "obj", "x"}},

        // Simple many-arg tests
        {"a, b, c, d, e, f, g, h", 8, {"a", "b", "c", "d", "e", "f", "g", "h"}},
        {"a, b, c, d, e, f, g, h, i, j, k, l", 12, {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l"}},
        {
            "a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t",
            20,
            {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t"}
        },
        {"a, b, c, d, e, f, g, h", 5, {"", "", "", "", ""}},
        {"a, b, c, d, e, f, g, h", 10, {"", "", "", "", "", "", "", "", "", ""}},
        {
            "a + b, c + d, e + f, g + h, i + j, k + l, m + n, o + p", 8,
            {"a + b", "c + d", "e + f", "g + h", "i + j", "k + l", "m + n", "o + p"}
        },
        {
            "f(a, b), g(c, d), h(e, f), i(g, h), j(i, j), k(k, l), l(m, n), m(o, p)",
            8,
            {"f(a, b)", "g(c, d)", "h(e, f)", "i(g, h)", "j(i, j)", "k(k, l)", "l(m, n)", "m(o, p)"}
        },

        // More complex many-arg tests
        {
            "a<1>(2), b<3>(4), c<5>(6), d<7>(8), e<9>(10), f<11>(12), g<13>(14), h<15>(16)",
            8,
            {"a<1>(2)", "b<3>(4)", "c<5>(6)", "d<7>(8)", "e<9>(10)", "f<11>(12)", "g<13>(14)", "h<15>(16)"}
        },
        {
            "a<1>(2), b, c<3>(4), d, e, f<5>(6), g, h<7>(8), i, j",
            10,
            {"a<1>(2)", "b", "c<3>(4)", "d", "e", "f<5>(6)", "g", "h<7>(8)", "i", "j"}},
        {
            "a<1, 2>(3), b<4, 5>(6), c<7, 8>(9), d<10, 11>(12), e, f, g, h",
            8,
            {"a<1, 2>(3)", "b<4, 5>(6)", "c<7, 8>(9)", "d<10, 11>(12)", "e", "f", "g", "h"}
        },
        {
            "a<b<1, 2>>(3), c, d, e<f<3, 4>>(5), g, h, i<j<5, 6>>(7), k",
            8,
            {"a<b<1, 2>>(3)", "c", "d", "e<f<3, 4>>(5)", "g", "h", "i<j<5, 6>>(7)", "k"}
        },
        {
            "std::get<0>(a), std::get<1>(b), std::get<2>(c), std::get<3>(d), "
            "std::get<4>(e), std::get<5>(f), std::get<6>(g), std::get<7>(h)",
            8,
            {
                "std::get<0>(a)", "std::get<1>(b)", "std::get<2>(c)", "std::get<3>(d)",
                "std::get<4>(e)", "std::get<5>(f)", "std::get<6>(g)", "std::get<7>(h)"
            }
        },

        // More ambiguous many-arg tests, more branching required
        {
            "x, y, a<1, 2> + b, c, d, e, f, g",
            8,
            {"x", "y", "a<1, 2> + b", "c", "d", "e", "f", "g"}
        },
        // Same expression but n=9 forces comparison interpretation
        {
            "x, y, a<1, 2> + b, c, d, e, f, g",
            9,
            {"x", "y", "a<1", "2> + b", "c", "d", "e", "f", "g"}
        },
        // Multiple ambiguous templates scattered among many args
        {
            "w, a<1, 2> + b, x, c<3, 4> + d, y, e<5, 6> + f, z, g",
            8,
            {"w", "a<1, 2> + b", "x", "c<3, 4> + d", "y", "e<5, 6> + f", "z", "g"}
        },
        // Same but n=9 or n=10 is really ambiguous
        {
            "w, a<1, 2> + b, x, c<3, 4> + d, y, e<5, 6> + f, z, g",
            9,
            {"", "", "", "", "", "", "", "", ""}
        },
        {
            "w, a<1, 2> + b, x, c<3, 4> + d, y, e<5, 6> + f, z, g",
            10,
            {"", "", "", "", "", "", "", "", "", ""}
        },
        // n=11 is ok though
        {
            "w, a<1, 2> + b, x, c<3, 4> + d, y, e<5, 6> + f, z, g",
            11,
            {"w", "a<1", "2> + b", "x", "c<3", "4> + d", "y", "e<5", "6> + f", "z", "g"}
        },
        {
            "a<1, 2> + b, c, d, e, f, g, h, i",
            8,
            {"a<1, 2> + b", "c", "d", "e", "f", "g", "h", "i"}
        },
        {
            "c, d, e, f, g, h, i, a<1, 2> + b",
            8,
            {"c", "d", "e", "f", "g", "h", "i", "a<1, 2> + b"}
        },
        {
            "c, d, e, a<1, 2> + b, f, g, h, i",
            8,
            {"c", "d", "e", "a<1, 2> + b", "f", "g", "h", "i"}
        },
        {
            "a<1, 2> + b, c, d, e, f, g, h<3, 4> + i, j",
            8,
            {"a<1, 2> + b", "c", "d", "e", "f", "g", "h<3, 4> + i", "j"}
        },
        {
            "a<1>(2), b, c<3, 4> + d, e, f<5>(6), g, h<7, 8> + i, j",
            8,
            {"a<1>(2)", "b", "c<3, 4> + d", "e", "f<5>(6)", "g", "h<7, 8> + i", "j"}
        },
    };

    bool ok = true;
    int passed = 0;
    int failed = 0;
    for(const auto& test_case : tests) {
        auto result = libassert::detail::split_args_string(test_case.args_string, test_case.n_args);
        bool test_passed = (result == test_case.expected);
        std::cout << "split_args_string(\"" << test_case.args_string << "\", " << test_case.n_args << "): ";
        if(test_passed) {
            std::cout << GREEN "Passed" RESET << std::endl;
            passed++;
        } else {
            std::cout << RED "Failed" RESET << std::endl;
            std::cout << "  result:   [";
            for(size_t i = 0; i < result.size(); i++) {
                if(i > 0) std::cout << ", ";
                std::cout << "\"" << result[i] << "\"";
            }
            std::cout << "]" << std::endl;
            std::cout << "  expected: [";
            for(size_t i = 0; i < test_case.expected.size(); i++) {
                if(i > 0) std::cout << ", ";
                std::cout << "\"" << test_case.expected[i] << "\"";
            }
            std::cout << "]" << std::endl;
            ok = false;
            failed++;
        }
    }
    std::cout << std::endl << passed << " passed, " << failed << " failed" << std::endl;
    return !ok;
}
