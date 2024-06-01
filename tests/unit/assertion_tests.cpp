#include <gtest/gtest.h>
#include <libassert/assert.hpp>

#include "utils.hpp"
#include "microfmt.hpp"
#include "tokenizer.hpp"

#include <array>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

using namespace std::literals;

inline void failure_handler(const libassert::assertion_info& info) {
    // everything from .to_string except the stacktrace
    std::string output;
    output += info.tagline(libassert::color_scheme::blank);
    output += info.statement(libassert::color_scheme::blank);
    output += info.print_binary_diagnostics(0, libassert::color_scheme::blank);
    output += info.print_extra_diagnostics(0, libassert::color_scheme::blank);
    throw std::runtime_error(output);
}

inline auto pre_main = [] () {
    libassert::set_failure_handler(failure_handler);
    return 1;
} ();

struct location {
    std::string_view file;
    int line;
    std::string_view signature;
};

constexpr std::string_view file = [] () constexpr {
    constexpr std::string_view f = __FILE__;
    constexpr std::size_t pos = f.find_last_of("/\\");
    static_assert(pos != std::string_view::npos);
    return f.substr(pos + 1);
} ();

std::string_view mtrim(const std::string_view s) {
    const size_t l = s.find_first_not_of(" \n");
    if(l == std::string_view::npos) {
        return "";
    }
    const size_t r = s.find_last_not_of(" ") + 1;
    ASSERT(r != std::string_view::npos);
    return s.substr(l, r - l);
}

std::string replace(std::string str, std::string_view needle, std::string_view replacement) {
    if(auto pos = str.find(needle); pos != std::string::npos) {
        return str.replace(pos, needle.size(), replacement);
    } else {
        return str;
    }
}

std::string assertion_failure_message;

#define WRAP(...) do { \
        assertion_failure_message = ""; \
        try { \
            __VA_ARGS__; \
        } catch(std::exception& e) { \
            assertion_failure_message = e.what(); \
        } \
    } while(0)

std::string prepare(std::string_view string, location loc) {
    using namespace libassert::detail;
    auto lines = split(mtrim(string), "\n");
    for(auto& line : lines) {
        auto pos = line.find('|');
        line = line.substr(pos == std::string_view::npos ? 0 : pos + 1);
    }
    auto message = replace(
        join(lines, "\n"),
        "<LOCATION>",
        microfmt::format("{}:{}: {}", loc.file, loc.line, loc.signature)
    );
    return message;
}

std::string normalize(std::string message) {
    using namespace libassert::detail;
    // TODO: Find a better way to do this, or integrate into normal type normalization rules
    // msvc does T const
    replace_all(message, "char const", "const char");
    // clang does T *
    replace_all(message, "void *", "void*");
    replace_all(message, "char *", "char*");
    // clang does T[N], gcc does T [N]
    replace_all(message, "int [5]", "int[5]");
    // msvc includes the std::less
    replace_all(message, ", std::less<int>", "");
    replace_all(message, ", std::less<std::string>", "");
    return message;
}

#define CHECK(call, expected) \
    do { \
        /* using __builtin_LINE() as source_location to work around */ \
        /* https://developercommunity.visualstudio.com/t/__builtin_LINE-function-is-reporting-w/10439054 */ \
        /* and also there seems to be some weirdness with __LINE__ vs __builtin_LINE() on msvc */ \
        location loc = {file, __builtin_LINE(), LIBASSERT_PFUNC}; \
        WRAP(call); \
        EXPECT_EQ( \
            prepare((expected), loc), \
            normalize(assertion_failure_message) \
        ); \
    } while(0)

#define PASS(...) try { __VA_ARGS__; SUCCEED(); } catch(const std::exception& e) { FAIL() << e.what(); }

// TEST(LibassertBasic, Warmup) {
//     try {
//         DEBUG_ASSERT(1 == 2);
//     } catch(...) {}
// }

TEST(LibassertBasic, StringDiagnostics) {
    std::string s = "test\n";
    CHECK(
        DEBUG_ASSERT(s == "test"),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(s == "test");
        |    Where:
        |        s => "test\n"
        )XX"
    );
    int i = 0;
    CHECK(
        DEBUG_ASSERT(s[i] == 'c', "", s, i),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(s[i] == 'c', ...);
        |    Where:
        |        s[i] => 't'
        |    Extra diagnostics:
        |        s => "test\n"
        |        i => 0
        )XX"
    );
    char* buffer = nullptr;
    char thing[] = "foo";
    CHECK(
        DEBUG_ASSERT(buffer == thing),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(buffer == thing);
        |    Where:
        |        buffer => nullptr
        |        thing  => "foo"
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(buffer == +thing),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(buffer == +thing);
        |    Where:
        |        buffer => nullptr
        |        +thing => "foo"
        )XX"
    );
    std::string_view sv = "foo";
    CHECK(
        DEBUG_ASSERT(s == sv),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(s == sv);
        |    Where:
        |        s  => "test\n"
        |        sv => "foo"
        )XX"
    );
}

TEST(LibassertBasic, PointerDiagnostics) {
    // TODO: Move
    CHECK(
        DEBUG_ASSERT((uintptr_t)-1 == 0xff),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT((uintptr_t)-1 == 0xff);
        |    Where:
        |        (uintptr_t)-1 => 18446744073709551615 0xffffffffffffffff
        |        0xff          => 255 0xff
        )XX"
    );
    // TODO: Move
    CHECK(
        DEBUG_ASSERT((uintptr_t)-1 == (uintptr_t)0xff),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT((uintptr_t)-1 == (uintptr_t)0xff);
        |    Where:
        |        (uintptr_t)-1   => 18446744073709551615
        |        (uintptr_t)0xff => 255
        )XX"
    );
    void* foo = (void*)0xdeadbeefULL;
    CHECK(
        DEBUG_ASSERT(foo == nullptr),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(foo == nullptr);
        |    Where:
        |        foo => void*: 0xdeadbeef
        )XX"
    );
}

TEST(LibassertBasic, LiteralFormatting) {
    const uint16_t flags = 0b000101010;
    const uint16_t mask = 0b110010101;
    CHECK(
        DEBUG_ASSERT(mask bitand flags),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(mask bitand flags);
        |    Where:
        |        mask  => 405 0b0000000110010101
        |        flags => 42 0b0000000000101010
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(0xf == 16),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(0xf == 16);
        |    Where:
        |        0xf => 15 0xf
        |        16  => 16 0x10
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(0xff == 077),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(0xff == 077);
        |    Where:
        |        0xff => 255 0xff 0377
        |        077  => 63 0x3f 077
        )XX"
    );
    CHECK(
        DEBUG_ASSERT('x' == 20),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT('x' == 20);
        |    Where:
        |        'x' => 'x' 120
        |        20  => '\x14' 20
        )XX"
    );
    CHECK(
        DEBUG_ASSERT('x' == 'y'),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT('x' == 'y');
        )XX"
    );
    char c = 'x';
    CHECK(
        DEBUG_ASSERT(c == 20),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(c == 20);
        |    Where:
        |        c  => 'x' 120
        |        20 => '\x14' 20
        )XX"
    );
}

TEST(LibassertBasic, FloatingPoint) {
    CHECK(
        DEBUG_ASSERT(1 == 1.5),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(1 == 1.5);
        )XX"
    );
    // FIXME
    CHECK(
        DEBUG_ASSERT(0.5 != .5),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(0.5 != .5);
        |    Where:
        |        .5 => 0.5
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(0.1 + 0.2 == 0.3),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(0.1 + 0.2 == 0.3);
        |    Where:
        |        0.1 + 0.2 => 0.30000000000000004
        |        0.3       => 0.29999999999999999
        )XX"
    );
    CHECK(
        ASSERT(.1 + .2 == .3),
        R"XX(
        |Assertion failed at <LOCATION>:
        |    ASSERT(.1 + .2 == .3);
        |    Where:
        |        .1 + .2 => 0.30000000000000004
        |        .3      => 0.29999999999999999
        )XX"
    );
    float ff = .1f;
    CHECK(
        DEBUG_ASSERT(ff == .1),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(ff == .1);
        |    Where:
        |        ff => 0.100000001
        |        .1 => 0.10000000000000001
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(.1f == .1),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(.1f == .1);
        |    Where:
        |        .1f => 0.100000001
        |        .1  => 0.10000000000000001
        )XX"
    );
    PASS(DEBUG_ASSERT(0.1f + 0.2f == 0.3f));
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
std::ostream& operator<<(std::ostream& stream, const printable<T>& p) {
    return stream<<"(printable = "<<*p.f<<")";
}

TEST(LibassertBasic, OstreamOverloads) {
    printable p{1.42};
    CHECK(
        DEBUG_ASSERT(p == printable{2.55}),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(p == printable{2.55});
        |    Where:
        |        p               => (printable = 1.42)
        |        printable{2.55} => (printable = 2.55)
        )XX"
    );
}

template<typename T>
struct not_printable {
    std::optional<T> f;
    not_printable(T t) : f(t) {}
    bool operator==(const not_printable& other) const {
        return f == other.f;
    }
};

TEST(LibassertBasic, NotPrintable) {
    const not_printable p{1.42};
    CHECK(
        DEBUG_ASSERT(p == not_printable{2.55}),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(p == not_printable{2.55});
        |    Where:
        |        p                   => <instance of not_printable<double>>
        |        not_printable{2.55} => <instance of not_printable<double>>
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(p.f == not_printable{2.55}.f),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(p.f == not_printable{2.55}.f);
        |    Where:
        |        p.f                   => std::optional<double>: 1.4199999999999999
        |        not_printable{2.55}.f => std::optional<double>: 2.5499999999999998
        )XX"
    );
}

TEST(LibassertBasic, OptionalMessages) {
    CHECK(
        DEBUG_ASSERT(false, 2),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        2 => 2
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(false, "foo"),
        R"XX(
        |Debug Assertion failed at <LOCATION>: foo
        |    DEBUG_ASSERT(false, ...);
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(false, "foo"s),
        R"XX(
        |Debug Assertion failed at <LOCATION>: foo
        |    DEBUG_ASSERT(false, ...);
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(false, "foo"sv),
        R"XX(
        |Debug Assertion failed at <LOCATION>: foo
        |    DEBUG_ASSERT(false, ...);
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(false, (char*)"foo"),
        R"XX(
        |Debug Assertion failed at <LOCATION>: foo
        |    DEBUG_ASSERT(false, ...);
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(false, "foo", 2),
        R"XX(
        |Debug Assertion failed at <LOCATION>: foo
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        2 => 2
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(false, "foo"s, 2),
        R"XX(
        |Debug Assertion failed at <LOCATION>: foo
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        2 => 2
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(false, "foo"sv, 2),
        R"XX(
        |Debug Assertion failed at <LOCATION>: foo
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        2 => 2
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(false, (char*)"foo", 2),
        R"XX(
        |Debug Assertion failed at <LOCATION>: foo
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        2 => 2
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(false, nullptr),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        nullptr => nullptr
        )XX"
    );
    // TODO: This behavior should probably change
    CHECK(
        DEBUG_ASSERT(false, (char*)nullptr),
        R"XX(
        |Debug Assertion failed at <LOCATION>: (nullptr)
        |    DEBUG_ASSERT(false, ...);
        )XX"
    );
}

TEST(LibassertBasic, Errno) {
    errno = 2;
    CHECK(
        DEBUG_ASSERT(false, errno),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        errno =>  2 "No such file or directory"
        )XX"
    );
    errno = 2;
    CHECK(
        DEBUG_ASSERT(false, "foo", errno),
        R"XX(
        |Debug Assertion failed at <LOCATION>: foo
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        errno =>  2 "No such file or directory"
        )XX"
    );
}

int foo() {
    return 2;
}
int bar() {
    return -2;
}

TEST(LibassertBasic, General) {
    CHECK(
        DEBUG_ASSERT(false, "foo", false, 2 * foo(), "foobar"sv, bar(), printable{2.55}),
        R"XX(
        |Debug Assertion failed at <LOCATION>: foo
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        false           => false
        |        2 * foo()       => 4
        |        "foobar"sv      => "foobar"
        |        bar()           => -2
        |        printable{2.55} => (printable = 2.55)
        )XX"
    );
    CHECK(
        DEBUG_ASSERT([] { return false; } ()),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT([] { return false; } ());
        )XX"
    );
}

TEST(LibassertBasic, SignedUnsignedComparisonWithoutSafeCompareMode) {
    PASS(DEBUG_ASSERT(18446744073709551606ULL == -10));
    PASS(DEBUG_ASSERT(-1 > 1U));
}

TEST(LibassertBasic, ExpressionDecomposition) {
    CHECK(
        DEBUG_ASSERT(1 == (1 bitand 2)),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(1 == (1 bitand 2));
        |    Where:
        |        (1 bitand 2) => 0
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(1 < 1 < 0),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(1 < 1 < 0);
        |    Where:
        |        1 < 1 => false
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(0 + 0 + 0),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(0 + 0 + 0);
        |    Where:
        |        0 + 0 + 0 => 0
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(false == false == false),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(false == false == false);
        |    Where:
        |        false == false => true
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(1 << 1 == 200),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(1 << 1 == 200);
        |    Where:
        |        1 << 1 => 2
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(1 << 1 << 31),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(1 << 1 << 31);
        |    Where:
        |        1 << 1 => 2
        )XX"
    );
    int x = 2;
    CHECK(
        DEBUG_ASSERT(x -= 2),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(x -= 2);
        |    Where:
        |        x => 0
        )XX"
    );
    x = 2;
    CHECK(
        DEBUG_ASSERT(x -= x -= 1),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(x -= x -= 1);
        |    Where:
        |        x      => 0
        |        x -= 1 => 0
        )XX"
    );
    x = 2;
    CHECK(
        DEBUG_ASSERT(x -= x -= x -= 1),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(x -= x -= x -= 1);
        |    Where:
        |        x           => 0
        |        x -= x -= 1 => 0
        )XX"
    );
    CHECK(
        DEBUG_ASSERT(true ? false : true, "pffft"),
        R"XX(
        |Debug Assertion failed at <LOCATION>: pffft
        |    DEBUG_ASSERT(true ? false : true, ...);
        )XX"
    );
    // regression test for #26
    int a = 1;
    CHECK(
        DEBUG_ASSERT(a >> 1),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(a >> 1);
        |    Where:
        |        a => 1
        )XX"
    );
}

TEST(LibassertBasic, ValueComputation) {
    // Ensure value names are only computed once
    auto foo = [] {
        static int x = 2;
        return x++;
    };
    auto bar = [] {
        static int x = -2;
        return x--;
    };
    auto baz = [] {
        static int x = 10;
        return x++;
    };
    CHECK(
        DEBUG_ASSERT(foo() < bar(), baz()),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(foo() < bar(), ...);
        |    Where:
        |        foo() => 2
        |        bar() => -2
        |    Extra diagnostics:
        |        baz() => 10
        )XX"
    );
    EXPECT_EQ(foo(), 3);
    EXPECT_EQ(bar(), -3);
    EXPECT_EQ(baz(), 11);
}

TEST(LibassertBasic, LvalueForwarding) {
    int x = 1;
    CHECK(
        DEBUG_ASSERT(x ^= 1),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(x ^= 1);
        |    Where:
        |        x => 0 0b00000000000000000000000000000000
        |        1 => 1 0b00000000000000000000000000000001
        )XX"
    );
    EXPECT_EQ(x, 0);
}

#ifdef LIBASSERT_USE_MAGIC_ENUM
enum foo_e { A, B };
enum class bar_e { A, B };
TEST(LibassertBasic, EnumHandling) {
    foo_e a = A;
    CHECK(
        DEBUG_ASSERT(a != A),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(a != A);
        |    Where:
        |        a => A
        )XX"
    );
    bar_e b = bar_e::A;
    CHECK(
        DEBUG_ASSERT(b != bar_e::A),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(b != bar_e::A);
        |    Where:
        |        b        => A
        |        bar_e::A => A
        )XX"
    );
}
#else
// TODO: Also test this outside of gcc 8
enum foo_e { A, B };
enum class bar_e { A, B };
TEST(LibassertBasic, EnumHandling) {
    foo_e a = A;
    CHECK(
        DEBUG_ASSERT(a != A),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(a != A);
        |    Where:
        |        a => enum foo_e: 0
        |        A => enum foo_e: 0
        )XX"
    );
    bar_e b = bar_e::A;
    CHECK(
        DEBUG_ASSERT(b != bar_e::A),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(b != bar_e::A);
        |    Where:
        |        b        => enum bar_e: 0
        |        bar_e::A => enum bar_e: 0
        )XX"
    );
}
#endif

TEST(LibassertBasic, Containers) {
    std::set<int> a = { 2, 2, 4, 6, 10 };
    std::set<int> b = { 2, 2, 5, 6, 10 };
    std::vector<double> c = { 1.2f, 2.44f, 3.15159f, 5.2f };
    CHECK(
        DEBUG_ASSERT(a == b, c),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(a == b, ...);
        |    Where:
        |        a => std::set<int>: [2, 4, 6, 10]
        |        b => std::set<int>: [2, 5, 6, 10]
        |    Extra diagnostics:
        |        c => std::vector<double>: [1.2000000476837158, 2.440000057220459, 3.15159010887146, 5.1999998092651367]
        )XX"
    );
    std::map<std::string, int> m0 = {
        {"foo", 2},
        {"bar", -2}
    };
    CHECK(
        DEBUG_ASSERT(false, m0),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        m0 => std::map<std::string, int>: [["bar", -2], ["foo", 2]]
        )XX"
    );
    std::map<std::string, std::vector<int>> m1 = {
        {"foo", {1, -2, 3, -4}},
        {"bar", {-100, 200, 400, -800}}
    };
    CHECK(
        DEBUG_ASSERT(false, m1),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        m1 => std::map<std::string, std::vector<int>>: [["bar", [-100, 200, 400, -800]], ["foo", [1, -2, 3, -4]]]
        )XX"
    );
    auto t = std::make_tuple(1, 0.1 + 0.2, "foobars");
    CHECK(
        DEBUG_ASSERT(false, t),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        t => std::tuple<int, double, const char*>: [1, 0.30000000000000004, "foobars"]
        )XX"
    );
    std::array<int, 10> arr = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    CHECK(
        DEBUG_ASSERT(false, arr),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        arr => std::array<int, 10>: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
        )XX"
    );
    int carr[] = { 5, 4, 3, 2, 1 };
    CHECK(
        DEBUG_ASSERT(false, carr),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(false, ...);
        |    Extra diagnostics:
        |        carr => int[5]: [5, 4, 3, 2, 1]
        )XX"
    );
}

// TEST(LibassertBasic, TypeCleaning) {}

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

TEST(LibassertBasic, StringificationCustomizationPoint) {
    debug_print_customization x = 2, y = 1;
    CHECK(
        DEBUG_ASSERT(x == y, x, y),
        R"XX(
        |Debug Assertion failed at <LOCATION>:
        |    DEBUG_ASSERT(x == y, ...);
        |    Where:
        |        x => (debug_print_customization = 2)
        |        y => (debug_print_customization = 1)
        |    Extra diagnostics:
        |        x => (debug_print_customization = 2)
        |        y => (debug_print_customization = 1)
        )XX"
    );
}

TEST(LibassertBasic, Panic) {
    const std::vector<std::string> vec{"foo", "bar", "baz"};
    CHECK(
        PANIC("message", vec),
        R"XX(
        |Panic at <LOCATION>: message
        |    PANIC(...);
        |    Extra diagnostics:
        |        vec => std::vector<std::string>: ["foo", "bar", "baz"]
        )XX"
    );
    float x = 40;
    CHECK(
        PANIC(x),
        R"XX(
        |Panic at <LOCATION>:
        |    PANIC(...);
        |    Extra diagnostics:
        |        x => 40.0
        )XX"
    );
}

// TODO:
// basic assertion failures
// extra diagnostics
// other kinds of assertions...
// assumptions...
// lowercase vs uppercase
// fmt
// optimizations vs no optimizations
// traces
// path name disambiguation
// color codes?
// message logic
// safe comparisons
// magic enum
// todo: unary non-bool
// value forwarding: lifetimes
// value forwarding: lvalue references
// value forwarding: rvalue references
// recursion / recursion folding
// Complex type resolution
// non-conformant msvc preprocessor
// source location unit test
