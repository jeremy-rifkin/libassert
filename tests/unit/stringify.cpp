#include <array>
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

int main() {
    // primitive types
    ASSERT(stringification::stringify(false) == R"(false)");
    ASSERT(stringification::stringify(42) == R"(42)");
    ASSERT(stringification::stringify(2.25) == R"(2.25)");
    // TODO: Other formats
    // pointers
    ASSERT(stringification::stringify(nullptr) == R"(nullptr)");
    int x;
    int* ptr = &x;
    auto s = stringification::stringify(ptr);
    ASSERT(s.find("int*: 0x") == 0, "", s);
    auto uptr = std::make_unique<int>(62);
    ASSERT(stringification::stringify(uptr) == R"(std::unique_ptr<int>: 62)");
    ASSERT(stringification::stringify(std::unique_ptr<int>()) == R"(std::unique_ptr<int>: nullptr)");
    // strings and chars
    ASSERT(stringification::stringify("foobar") == R"("foobar")");
    ASSERT(stringification::stringify("foobar"sv) == R"("foobar")");
    ASSERT(stringification::stringify("foobar"s) == R"("foobar")");
    ASSERT(stringification::stringify(char(42)) == R"(42)");
    // ASSERT(stringification::stringify(R"("foobar")") == R"xx(R\\\"("foobar")")xx"); // TODO: Don't escape in raw strings
    // containers
    std::array arr{1,2,3,4,5};
    ASSERT(stringification::stringify(arr) == R"([1, 2, 3, 4, 5])");
    std::map<int, int> map{{1,2},{3,4}};
    ASSERT(stringification::stringify(map) == R"([[1, 2], [3, 4]])");
    std::tuple<int, float, std::string, std::array<int, 5>> tuple = {1, 1.25, "good", arr};
    // ASSERT(stringification::stringify(tuple) == R"([1, 1.25, \"good\", [1, 2, 3, 4, 5]])"); // TODO fix
    std::optional<int> opt;
    ASSERT(stringification::stringify(opt) == R"(std::optional<int>: nullopt)");
    opt = 63;
    ASSERT(stringification::stringify(opt) == R"(std::optional<int>: 63)");
    // error codes
    // customization point objects
    // libfmt
    // std::format
    // stringification tests
}
