#include <map>
#include <string>
#include <utility>

#include <assert/assert.hpp>

using namespace libassert::utility;

int main() {
    // pretty_type_name tests
    auto pretty_name = pretty_type_name<std::map<std::string, int>>();
    DEBUG_ASSERT(pretty_name.find("basic_string") == std::string::npos);
    DEBUG_ASSERT(pretty_name.find("allocator") == std::string::npos);
    // stringification tests
    DEBUG_ASSERT(stringify(12) == "12");
    DEBUG_ASSERT(stringify('x') == "'x'");
    DEBUG_ASSERT(stringify(std::make_pair("foobar", 20)) == R"(["foobar", 20])");
}
