#ifndef UTILS_HPP
#define UTILS_HPP

#include <array>
#include <cstdarg>
#include <cstdio>
#include <iterator>
#include <optional>
#include <regex>
#include <string_view>
#include <string>
#include <utility>
#include <vector>
#include <type_traits>

#include <assert/assert.hpp>

namespace libassert::detail {
    // Still present in release mode, nonfatal
    #define internal_verify(c, ...) primitive_assert_impl(c, true, #c, LIBASSERT_PFUNC, {}, ##__VA_ARGS__)

    /*
     * string utilities
     */

    template<typename... T>
    LIBASSERT_ATTR_COLD
    std::string stringf(T... args) {
        const int length = snprintf(nullptr, 0, args...);
        if(length < 0) { LIBASSERT_PRIMITIVE_ASSERT(false, "Invalid arguments to stringf"); }
        std::string str(length, 0);
        (void)snprintf(str.data(), length + 1, args...);
        return str;
    }

    LIBASSERT_ATTR_COLD
    std::vector<std::string> split(std::string_view s, std::string_view delims);

    template<typename C>
    LIBASSERT_ATTR_COLD
    std::string join(const C& container, const std::string_view delim) {
        auto iter = std::begin(container);
        auto end = std::end(container);
        std::string str;
        if(std::distance(iter, end) > 0) {
            str += *iter;
            while(++iter != end) {
                str += delim;
                str += *iter;
            }
        }
        return str;
    }

    LIBASSERT_ATTR_COLD
    std::string_view trim(std::string_view s);

    LIBASSERT_ATTR_COLD
    void replace_all_dynamic(std::string& str, std::string_view text, std::string_view replacement);

    LIBASSERT_ATTR_COLD
    void replace_all(std::string& str, const std::regex& re, std::string_view replacement);

    LIBASSERT_ATTR_COLD
    void replace_all(std::string& str, std::string_view substr, std::string_view replacement);

    LIBASSERT_ATTR_COLD
    void replace_all_template(std::string& str, const std::pair<std::regex, std::string_view>& rule);

    LIBASSERT_ATTR_COLD
    std::string indent(std::string_view str, size_t depth, char c = ' ', bool ignore_first = false);

    template<size_t N>
    LIBASSERT_ATTR_COLD
    std::optional<std::array<std::string, N>> match(const std::string& s, const std::regex& r) {
        std::smatch match;
        if(std::regex_match(s, match, r)) {
            std::array<std::string, N> arr;
            for(size_t i = 0; i < N; i++) {
                arr[i] = match[i + 1].str();
            }
            return arr;
        } else {
            return {};
        }
    }

    template<typename T, typename std::enable_if<std::is_unsigned<T>::value, int>::type = 0>
    constexpr T popcount(T value) {
        T pop = 0;
        while(value) {
            value &= value - 1;
            pop++;
        }
        return pop;
    }

    static_assert(popcount(0U) == 0);
    static_assert(popcount(1U) == 1);
    static_assert(popcount(2U) == 1);
    static_assert(popcount(3U) == 2);
    static_assert(popcount(0xf0U) == 4);
}

#endif
