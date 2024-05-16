#ifndef LIBASSERT_UTILITIES_HPP
#define LIBASSERT_UTILITIES_HPP

#include <type_traits>
#include <string>
#include <string_view>

#include <libassert/platform.hpp>

// =====================================================================================================================
// || Core utilities                                                                                                  ||
// =====================================================================================================================

namespace libassert {
    // Lightweight helper, eventually may use C++20 std::source_location if this library no longer
    // targets C++17. Note: __builtin_FUNCTION only returns the name, so __PRETTY_FUNCTION__ is
    // still needed.
    struct source_location {
        const char* file;
        //const char* function; // disabled for now due to static constexpr restrictions
        int line;
        constexpr source_location(
            //const char* _function /*= __builtin_FUNCTION()*/,
            const char* _file = __builtin_FILE(),
            int _line         = __builtin_LINE()
        ) : file(_file), /*function(_function),*/ line(_line) {}
    };
}

namespace libassert::detail {
    // bootstrap with primitive implementations
    LIBASSERT_EXPORT void primitive_assert_impl(
        bool condition,
        bool verify,
        const char* expression,
        const char* signature,
        source_location location,
        const char* message = nullptr
    );

    [[noreturn]] LIBASSERT_EXPORT void primitive_panic_impl (
        const char* signature,
        source_location location,
        const char* message
    );

    // always_false is just convenient to use here
    #define LIBASSERT_PHONY_USE(E) ((void)libassert::detail::always_false<decltype(E)>)

    #ifndef NDEBUG
     #define LIBASSERT_PRIMITIVE_ASSERT(c, ...) \
        libassert::detail::primitive_assert_impl(c, false, #c, LIBASSERT_PFUNC, {} LIBASSERT_VA_ARGS(__VA_ARGS__))
    #else
     #define LIBASSERT_PRIMITIVE_ASSERT(c, ...) LIBASSERT_PHONY_USE(c)
    #endif

    #define LIBASSERT_PRIMITIVE_PANIC(message) libassert::detail::primitive_panic_impl(LIBASSERT_PFUNC, {}, message)
}

// =====================================================================================================================
// || Basic formatting and type tools                                                                                 ||
// =====================================================================================================================

namespace libassert::detail {
    [[nodiscard]] LIBASSERT_EXPORT std::string bstringf(const char* format, ...);

    LIBASSERT_ATTR_COLD [[nodiscard]]
    constexpr inline std::string_view substring_bounded_by(
        std::string_view sig,
        std::string_view l,
        std::string_view r
    ) noexcept {
        auto i = sig.find(l) + l.length();
        return sig.substr(i, sig.rfind(r) - i);
    }

    template<typename T>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string_view type_name() noexcept {
        // Cases to handle:
        // gcc:   constexpr std::string_view ns::type_name() [with T = int; std::string_view = std::basic_string_view<char>]
        // clang: std::string_view ns::type_name() [T = int]
        // msvc:  class std::basic_string_view<char,struct std::char_traits<char> > __cdecl ns::type_name<int>(void)
        #if LIBASSERT_IS_CLANG
         return substring_bounded_by(LIBASSERT_PFUNC, "[T = ", "]");
        #elif LIBASSERT_IS_GCC
         return substring_bounded_by(LIBASSERT_PFUNC, "[with T = ", "; std::string_view = ");
        #elif LIBASSERT_IS_MSVC
         return substring_bounded_by(LIBASSERT_PFUNC, "type_name<", ">(void)");
        #else
         return LIBASSERT_PFUNC;
        #endif
    }

    [[nodiscard]] LIBASSERT_EXPORT std::string prettify_type(std::string type);
}

// =====================================================================================================================
// || Metaprogramming utilities                                                                                       ||
// =====================================================================================================================

namespace libassert::detail {
    struct nothing {};

    template<typename T> inline constexpr bool is_nothing = std::is_same_v<T, nothing>;

    // Hack to get around static_assert(false); being evaluated before any instantiation, even under
    // an if-constexpr branch
    // Also used for PHONY_USE
    template<typename T> inline constexpr bool always_false = false;

    template<typename T> using strip = std::remove_cv_t<std::remove_reference_t<T>>;

    // intentionally not stripping B
    template<typename A, typename B> inline constexpr bool isa = std::is_same_v<strip<A>, B>;

    // Is integral but not boolean
    template<typename T> inline constexpr bool is_integral_and_not_bool = std::is_integral_v<strip<T>> && !isa<T, bool>;

    template<typename T>
    inline constexpr bool is_arith_not_bool_char = std::is_arithmetic_v<strip<T>> && !isa<T, bool> && !isa<T, char>;

    template<typename T>
    inline constexpr bool is_c_string =
           isa<std::decay_t<strip<T>>, char*> // <- covers literals (i.e. const char(&)[N]) too
            || isa<std::decay_t<strip<T>>, const char*>;

    template<typename T>
    inline constexpr bool is_string_type =
           isa<T, std::string>
            || isa<T, std::string_view>
            || is_c_string<T>;

    // char(&)[20], const char(&)[20], const char(&)[]
    template<typename T> inline constexpr bool is_string_literal =
           std::is_lvalue_reference_v<T>
            && std::is_array_v<typename std::remove_reference_t<T>>
            && isa<typename std::remove_extent_t<typename std::remove_reference_t<T>>, char>;

    template<typename T> typename std::add_lvalue_reference_t<T> decllval() noexcept;
}

#endif
