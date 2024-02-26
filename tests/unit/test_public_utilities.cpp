#include <map>
#include <string>
#include <utility>

#include <libassert/assert.hpp>

using namespace libassert;

std::string replace(std::string str, std::string_view substr, std::string_view replacement) {
    std::string::size_type pos = 0;
    // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
    while((pos = str.find(substr.data(), pos, substr.length())) != std::string::npos) {
        str.replace(pos, substr.length(), replacement.data(), replacement.length());
        pos += replacement.length();
    }
    return str;
}

int main() {
    // pretty_type_name tests
    auto pretty_name = pretty_type_name<std::map<std::string, int>>();
    DEBUG_ASSERT(pretty_name.find("basic_string") == std::string::npos);
    DEBUG_ASSERT(pretty_name.find("allocator") == std::string::npos);
    // stringification tests
    DEBUG_ASSERT(generate_stringification(12) == "12");
    DEBUG_ASSERT(generate_stringification('x') == "'x'");
    DEBUG_ASSERT(
        replace(
            replace(generate_stringification(std::make_pair("foobar", 20)), "char const", "const char"),
            "char *",
            "char*"
        ) == R"(std::pair<const char*, int>: ["foobar", 20])"
    );
}
