#include <map>
#include <string>
#include <utility>

#include <assert/assert.hpp>

using namespace libassert::utility;

int main() {
    // pretty_type_name tests
    auto pretty_name = pretty_type_name<std::map<std::string, int>>();
    ASSERT(pretty_name.find("basic_string") == std::string::npos);
    ASSERT(pretty_name.find("allocator") == std::string::npos);
    // stringification tests
    ASSERT(stringify(12) == "12");
    ASSERT(stringify('x') == "'x'");
    ASSERT(stringify(std::make_pair("foobar", 20)) == R"(["foobar", 20])");
}
