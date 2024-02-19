#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <assert/assert.hpp>

using namespace libassert::detail;
using namespace std::literals;

struct S {};
struct S2 {};

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
    // strings and chars
    ASSERT(generate_stringification("foobar") == R"("foobar")");
    ASSERT(generate_stringification("foobar"sv) == R"("foobar")");
    ASSERT(generate_stringification("foobar"s) == R"("foobar")");
    ASSERT(generate_stringification(char(42)) == R"('*')");
    // ASSERT(generate_stringification(R"("foobar")") == R"xx(R\\\"("foobar")")xx"); // TODO: Don't escape in raw strings
    // containers
    std::array arr{1,2,3,4,5};
    static_assert(can_stringify<std::array<int, 5>>::value);
    static_assert(stringification::stringifiable<std::array<int, 5>>);
    ASSERT(generate_stringification(arr) == R"(std::array<int, 5>: [1, 2, 3, 4, 5])");
    std::map<int, int> map{{1,2},{3,4}};
    #if defined(_WIN32) && !LIBASSERT_IS_GCC
    ASSERT(generate_stringification(map) == R"(std::map<int, int, std::less<int>>: [[1, 2], [3, 4]])");
    #else
    ASSERT(generate_stringification(map) == R"(std::map<int, int>: [[1, 2], [3, 4]])");
    #endif
    std::tuple<int, float, std::string, std::array<int, 5>> tuple = {1, 1.25, "good", arr};
    // ASSERT(generate_stringification(tuple) == R"([1, 1.25, \"good\", [1, 2, 3, 4, 5]])"); // TODO fix
    std::optional<int> opt;
    ASSERT(generate_stringification(opt) == R"(std::optional<int>: nullopt)");
    opt = 63;
    ASSERT(generate_stringification(opt) == R"(std::optional<int>: 63)");
    std::vector<int> vec2 {2, 42, {}, 60};
    ASSERT(generate_stringification(vec2) == R"(std::vector<int>: [2, 42, 0, 60])");
    std::vector<std::optional<int>> vec {2, 42, {}, 60};
    ASSERT(generate_stringification(vec) == R"(std::vector<std::optional<int>>: [2, 42, nullopt, 60])");
    std::vector<std::optional<std::vector<std::pair<int, float>>>> vovp {{{{2, 1.2f}}}, {}, {{{20, 6.2f}}}};
    ASSERT(generate_stringification(vovp) == R"(std::vector<std::optional<std::vector<std::pair<int, float>>>>: [[[2, 1.20000005]], nullopt, [[20, 6.19999981]]])");
    int carr[] = {1, 1, 2, 3, 5, 8};
    static_assert(can_stringify<int[5]>::value);
    static_assert(stringification::stringifiable<int[5]>);
    static_assert(stringification::stringifiable_container<int[5]>());
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
    static_assert(!can_stringify<std::vector<S>>::value);
    static_assert(!stringification::stringifiable<std::vector<S>>);
    static_assert(!stringification::stringifiable_container<std::vector<S>>());
    static_assert(!stringification::stringifiable<S>);
    static_assert(!stringification::stringifiable_container<S>());
    ASSERT(generate_stringification(svec) == R"(<instance of std::vector<S>>)");
    std::vector<std::vector<S2>> svec2(10, std::vector<S2>(10));
    static_assert(!stringification::stringifiable<std::vector<std::vector<S2>>>);
    ASSERT(generate_stringification(svec2) == R"(<instance of std::vector<std::vector<S2>>>)");
    std::vector<std::vector<int>> svec3(10, std::vector<int>(10));
    static_assert(stringification::stringifiable<std::vector<std::vector<int>>>);
    ASSERT(generate_stringification(svec3) == R"(std::vector<std::vector<int>>: [[0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]])");

    // error codes
    // customization point objects
    // libfmt
    // std::format
    // stringification tests
}
