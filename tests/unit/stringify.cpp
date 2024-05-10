#include <array>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string_view>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <libassert/assert.hpp>

using namespace libassert::detail;
using namespace std::literals;

struct basic_fields {
    struct value_type {
        value_type() {}
    };
    struct const_iterator {
        bool operator==(const const_iterator&) const {
            return true;
        }
    };
    const_iterator begin() {
        return {};
    }
    const_iterator end() {
        return {};
    }
};

void regression01() {
    // regression test for #90, just making sure this can compile
    constexpr bool b = stringifiable_container<basic_fields::value_type>();
    ASSERT(!b);
    basic_fields fields;
    ASSERT(fields.begin() == fields.end());
}

struct ostream_printable {
    int x;
};

std::ostream& operator<<(std::ostream& os, ostream_printable item) {
    return os << "{" << item.x << "}";
}

struct S {};
struct S2 {};

struct A {};
struct B {};
struct C {};

int main() {
    // primitive types
    ASSERT(generate_stringification(false) == R"(false)");
    ASSERT(generate_stringification(42) == R"(42)");
    ASSERT(generate_stringification(2.25) == R"(2.25)");
    // TODO: Other formats
    // pointers
    ASSERT(generate_stringification(nullptr) == R"(nullptr)");
    int x;
    int* ptr = &x;
    auto s = generate_stringification(ptr);
    ASSERT(s.find("int*: 0x") == 0 || s.find("int *: 0x") == 0, "", s);
    auto uptr = std::make_unique<int>(62);
    ASSERT(generate_stringification(uptr) == R"(std::unique_ptr<int>: 62)");
    ASSERT(generate_stringification(std::unique_ptr<int>()) == R"(std::unique_ptr<int>: nullptr)");
    ASSERT(generate_stringification(std::make_unique<S>()).find(R"(std::unique_ptr<S>: 0x)") == 0, generate_stringification(std::make_unique<S>()));
    std::unique_ptr<std::vector<int>> uptr2(new std::vector<int>{1,2,3,4});
    ASSERT(generate_stringification(uptr2) == R"(std::unique_ptr<std::vector<int>>: [1, 2, 3, 4])");
    auto d = [](int*) {};
    std::unique_ptr<int, decltype(d)> uptr3(nullptr, d);
    ASSERT(generate_stringification(uptr3).find("std::unique_ptr<int,") == 0);
    ASSERT(generate_stringification(uptr3).find("nullptr") != std::string::npos);
    // strings and chars
    ASSERT(generate_stringification("foobar") == R"("foobar")");
    ASSERT(generate_stringification("foobar"sv) == R"("foobar")");
    ASSERT(generate_stringification("foobar"s) == R"("foobar")");
    ASSERT(generate_stringification(char(42)) == R"('*')");
    // Note: There's buggy behavior here with how MSVC stringizes the raw string literal when /Zc:preprocessor is not
    // used. https://godbolt.org/z/13acEWTGc
    ASSERT(generate_stringification(R"("foobar")") == R"xx("\"foobar\"")xx");
    // containers
    std::array arr{1,2,3,4,5};
    static_assert(stringifiable<std::array<int, 5>>);
    ASSERT(generate_stringification(arr) == R"(std::array<int, 5>: [1, 2, 3, 4, 5])");
    std::map<int, int> map{{1,2},{3,4}};
    #if defined(_WIN32) && !LIBASSERT_IS_GCC
    ASSERT(generate_stringification(map) == R"(std::map<int, int, std::less<int>>: [[1, 2], [3, 4]])");
    #else
    ASSERT(generate_stringification(map) == R"(std::map<int, int>: [[1, 2], [3, 4]])");
    #endif
    std::tuple<int, float, std::string, std::array<int, 5>> tuple = {1, 1.25f, "good", arr};
    ASSERT(generate_stringification(tuple) == R"(std::tuple<int, float, std::string, std::array<int, 5>>: [1, 1.25, "good", [1, 2, 3, 4, 5]])");
    std::optional<int> opt;
    ASSERT(generate_stringification(opt) == R"(std::optional<int>: nullopt)");
    opt = 63;
    ASSERT(generate_stringification(opt) == R"(std::optional<int>: 63)");
    ASSERT(generate_stringification(std::optional<S>(S{})) == R"(std::optional<S>: <instance of S>)");
    std::vector<int> vec2 {2, 42, {}, 60};
    ASSERT(generate_stringification(vec2) == R"(std::vector<int>: [2, 42, 0, 60])");
    std::vector<std::optional<int>> vec {2, 42, {}, 60};
    ASSERT(generate_stringification(vec) == R"(std::vector<std::optional<int>>: [2, 42, nullopt, 60])");
    std::vector<std::optional<std::vector<std::pair<int, float>>>> vovp {{{{2, 1.2f}}}, {}, {{{20, 6.2f}}}};
    ASSERT(generate_stringification(vovp) == R"(std::vector<std::optional<std::vector<std::pair<int, float>>>>: [[[2, 1.20000005]], nullopt, [[20, 6.19999981]]])");
    std::vector<int> vlong(1100);
    #define TEN "0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "
    #define HUNDRED TEN TEN TEN TEN TEN TEN TEN TEN TEN TEN
    ASSERT(
        generate_stringification(vlong) == "std::vector<int>: ["
            HUNDRED HUNDRED HUNDRED HUNDRED HUNDRED HUNDRED HUNDRED HUNDRED HUNDRED HUNDRED "..."
        "]"
    );
    int carr[] = {1, 1, 2, 3, 5, 8};
    static_assert(stringifiable<int[5]>);
    static_assert(stringifiable_container<int[5]>());
    #if LIBASSERT_IS_CLANG || LIBASSERT_IS_MSVC
    ASSERT(generate_stringification(carr) == R"(int[6]: [1, 1, 2, 3, 5, 8])");
    #else
    ASSERT(generate_stringification(carr) == R"(int [6]: [1, 1, 2, 3, 5, 8])");
    #endif
    // enum E { EE, FF };
    // ASSERT(generate_stringification(FF) == R"(int [6]: [1, 1, 2, 3, 5, 8])");

    // non-printable containers
    std::vector<S> svec(10);
    // stringification::stringify(svec);
    static_assert(!stringifiable<std::vector<S>>);
    static_assert(!stringifiable_container<std::vector<S>>());
    static_assert(!stringifiable<S>);
    static_assert(!stringifiable_container<S>());
    ASSERT(generate_stringification(svec) == R"(<instance of std::vector<S>>)");
    std::vector<std::vector<S2>> svec2(10, std::vector<S2>(10));
    //static_assert(!stringification::stringifiable<std::vector<std::vector<S2>>>);
    ASSERT(generate_stringification(svec2) == R"(<instance of std::vector<std::vector<S2>>>)");
    std::vector<std::vector<int>> svec3(10, std::vector<int>(10));
    static_assert(stringifiable<std::vector<std::vector<int>>>);
    ASSERT(generate_stringification(svec3) == R"(std::vector<std::vector<int>>: [[0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]])");

    // some literal format stuff
    ASSERT(generate_stringification(100) == "100");
    libassert::set_fixed_literal_format(libassert::literal_format::integer_hex | libassert::literal_format::integer_octal);
    libassert::detail::set_literal_format("", "", "", false);
    ASSERT(generate_stringification(100) == "100 0x64 0144");
    libassert::set_literal_format_mode(libassert::literal_format_mode::infer);

    std::tuple<A, B, C> tuple2;
    ASSERT(generate_stringification(tuple2) == R"(<instance of std::tuple<A, B, C>>)");
    std::tuple<A, B, float, C> tuple3 = {{}, {}, 1.2, {}};
    ASSERT(generate_stringification(tuple3) == R"(std::tuple<A, B, float, C>: [<instance of A>, <instance of B>, 1.20000005, <instance of C>])");

    ostream_printable op{2};
    ASSERT(generate_stringification(op) == "{2}");

    std::vector<ostream_printable> opvec{{{2}, {3}}};
    ASSERT(generate_stringification(opvec) == "std::vector<ostream_printable>: [{2}, {3}]");

    // error codes
    // customization point objects
    // libfmt
    // std::format
    // stringification tests

    regression01();
}
