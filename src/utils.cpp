#include <cstdio>
#include <cstdlib>
#include <regex>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

#include <libassert/assert.hpp>
#include <cpptrace/cpptrace.hpp>

#include "utils.hpp"
#include "microfmt.hpp"

namespace libassert::detail {
    LIBASSERT_ATTR_COLD
    void primitive_assert_impl(
        bool condition,
        bool verify,
        const char* expression,
        const char* signature,
        source_location location,
        const char* message
    ) {
        if(!condition) {
            const char* action = verify ? "Verification" : "Assertion";
            const char* name   = verify ? "verify"       : "assert";
            std::string out_message;
            if(message == nullptr) {
                out_message += microfmt::format(
                    "{} failed at {}:{}: {}\n",
                    action,
                    location.file,
                    location.line,
                    signature
                );
            } else {
                out_message += microfmt::format(
                    "{} failed at {}:{}: {}: {}\n",
                    action,
                    location.file,
                    location.line,
                    signature,
                    message
                );
            }
            out_message += microfmt::format("    primitive_{}({});\n", name, expression);
            throw cpptrace::runtime_error(std::move(out_message));
        }
    }

    [[noreturn]] LIBASSERT_ATTR_COLD LIBASSERT_EXPORT void primitive_panic_impl(
        const char* signature,
        source_location location,
        const char* message
    ) {
        std::string out_message = microfmt::format(
            "PANIC failed at {}:{}: {}: {}\n",
            location.file,
            location.line,
            signature,
            message
        );
        out_message += "    primitive_panic(...);\n";
        throw cpptrace::runtime_error(std::move(out_message));
    }

    /*
     * string utilities
     */

    LIBASSERT_ATTR_COLD
    std::vector<std::string_view> split(std::string_view s, std::string_view delims) {
        std::vector<std::string_view> vec;
        size_t old_pos = 0;
        size_t pos = 0;
        while((pos = s.find_first_of(delims, old_pos)) != std::string::npos) {
            vec.emplace_back(s.substr(old_pos, pos - old_pos));
            old_pos = pos + 1;
        }
        vec.emplace_back(s.substr(old_pos));
        return vec;
    }

    constexpr const char * const ws = " \t\n\r\f\v";

    LIBASSERT_ATTR_COLD
    std::string_view trim(const std::string_view s) {
        const size_t l = s.find_first_not_of(ws);
        if(l == std::string_view::npos) {
            return "";
        }
        const size_t r = s.find_last_not_of(ws) + 1;
        return s.substr(l, r - l);
    }

    LIBASSERT_ATTR_COLD
    void replace_all_dynamic(std::string& str, std::string_view text, std::string_view replacement) {
        std::string::size_type pos = 0;
        while((pos = str.find(text.data(), pos, text.length())) != std::string::npos) {
            str.replace(pos, text.length(), replacement.data(), replacement.length());
            // advancing by one rather than replacement.length() in case replacement leads to
            // another replacement opportunity, e.g. folding > > > to >> > then >>>
            pos++;
        }
    }

    LIBASSERT_ATTR_COLD
    void replace_all(std::string& str, const std::regex& re, std::string_view replacement) {
        std::smatch match;
        std::size_t i = 0;
        // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
        while(std::regex_search(str.cbegin() + i, str.cend(), match, re)) {
            str.replace(i + match.position(), match.length(), replacement);
            i += match.position() + replacement.length();
        }
    }

    LIBASSERT_ATTR_COLD
    void replace_all(std::string& str, std::string_view substr, std::string_view replacement) {
        std::string::size_type pos = 0;
        // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
        while((pos = str.find(substr.data(), pos, substr.length())) != std::string::npos) {
            str.replace(pos, substr.length(), replacement.data(), replacement.length());
            pos += replacement.length();
        }
    }

    LIBASSERT_ATTR_COLD
    void replace_all_template(std::string& str, const std::pair<std::regex, std::string_view>& rule) {
        const auto& [re, replacement] = rule;
        std::smatch match;
        std::size_t cursor = 0;
        // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
        while(std::regex_search(str.cbegin() + cursor, str.cend(), match, re)) {
            // find matching >
            const std::size_t match_begin = cursor + match.position();
            std::size_t end = match_begin + match.length();
            for(int c = 1; end < str.size() && c > 0; end++) {
                if(str[end] == '<') {
                    c++;
                } else if(str[end] == '>') {
                    c--;
                }
            }
            // make the replacement
            str.replace(match_begin, end - match_begin, replacement);
            cursor = match_begin + replacement.length();
        }
    };

    LIBASSERT_ATTR_COLD
    std::string indent(const std::string_view str, size_t depth, char c, bool ignore_first) {
        size_t i = 0;
        size_t j;
        std::string output;
        while((j = str.find('\n', i)) != std::string::npos) {
            if(i != 0 || !ignore_first) { output.insert(output.end(), depth, c); }
            output.insert(output.end(), str.begin() + i, str.begin() + j + 1);
            i = j + 1;
        }
        if(i != 0 || !ignore_first) { output.insert(output.end(), depth, c); }
        output.insert(output.end(), str.begin() + i, str.end());
        return output;
    }
}
