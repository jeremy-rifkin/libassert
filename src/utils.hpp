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

#include <libassert/assert.hpp>

#if !defined(LIBASSERT_BUILD_TESTING) || defined(LIBASSERT_STATIC_DEFINE)
 #define LIBASSERT_EXPORT_TESTING
#else
 #ifndef LIBASSERT_EXPORT_TESTING
  #ifdef libassert_lib_EXPORTS
   /* We are building this library */
   #define LIBASSERT_EXPORT_TESTING LIBASSERT_EXPORT_ATTR
  #else
   /* We are using this library */
   #define LIBASSERT_EXPORT_TESTING LIBASSERT_IMPORT_ATTR
  #endif
 #endif
#endif

namespace libassert::detail {
    // Still present in release mode, nonfatal
    #define internal_verify(c, ...) primitive_assert_impl(c, true, #c, LIBASSERT_PFUNC, {} LIBASSERT_VA_ARGS(__VA_ARGS__))

    /*
     * string utilities
     */

    LIBASSERT_ATTR_COLD
    std::vector<std::string_view> split(std::string_view s, std::string_view delims);

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

    template<typename T>
    LIBASSERT_ATTR_COLD
    std::vector<T> concat(std::vector<T> a, std::vector<T> b) {
        a.insert(
            a.end(),
            std::make_move_iterator(b.begin()),
            std::make_move_iterator(b.end())
        );
        return a;
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

    /*
     * Other
     */

    // Container utility
    template<typename N> class needle {
        // TODO: Re-evaluate
        const N& needle_value;
    public:
        explicit needle(const N& n) : needle_value(n) {}
        template<typename K, typename V, typename... Rest>
        constexpr V lookup(const K& option, const V& result, const Rest&... rest) {
            if(needle_value == option) { return result; }
            if constexpr(sizeof...(Rest) > 0) { return lookup(rest...); }
            else { LIBASSERT_PRIMITIVE_ASSERT(false); LIBASSERT_UNREACHABLE_CALL; }
        }
        template<typename... Args>
        constexpr bool is_in(const Args&... option) {
            return ((needle_value == option) || ... || false);
        }
    };

    // copied from cppref
    template<typename T, std::size_t N, std::size_t... I>
    constexpr std::array<std::remove_cv_t<T>, N> to_array_impl(T(&&a)[N], std::index_sequence<I...>) {
        return {{std::move(a[I])...}};
    }

    template<typename T, std::size_t N>
    constexpr std::array<std::remove_cv_t<T>, N> to_array(T(&&a)[N]) {
        return to_array_impl(std::move(a), std::make_index_sequence<N>{});
    }

    template<typename A, typename B>
    constexpr void constexpr_swap(A& a, B& b) {
        B tmp = std::move(b);
        b = std::move(a);
        a = std::move(tmp);
    }

    // cmp(a, b) should return whether a comes before b
    template<typename T, std::size_t N, typename C>
    constexpr void constexpr_sort(std::array<T, N>& arr, const C& cmp) {
        // insertion sort is fine for small arrays
        static_assert(N <= 65);
        for(std::size_t i = 1; i < arr.size(); i++) {
            for(std::size_t j = 0; j < i; j++) {
                if(cmp(arr[j], arr[i])) {
                    constexpr_swap(arr[i], arr[j]);
                }
            }
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

    template<typename T>
    LIBASSERT_ATTR_COLD
    static constexpr T n_digits(T value) {
        return value < 10 ? 1 : 1 + n_digits(value / 10);
    }

    static_assert(n_digits(0) == 1);
    static_assert(n_digits(1) == 1);
    static_assert(n_digits(9) == 1);
    static_assert(n_digits(10) == 2);
    static_assert(n_digits(11) == 2);
    static_assert(n_digits(1024) == 4);

    inline bool operator==(const color_scheme& a, const color_scheme& b) {
        return a.string == b.string
            && a.escape == b.escape
            && a.keyword == b.keyword
            && a.named_literal == b.named_literal
            && a.number == b.number
            && a.punctuation == b.punctuation
            && a.operator_token == b.operator_token
            && a.call_identifier == b.call_identifier
            && a.scope_resolution_identifier == b.scope_resolution_identifier
            && a.identifier == b.identifier
            && a.accent == b.accent
            && a.unknown == b.unknown
            && a.reset == b.reset;
    }
}

#endif
