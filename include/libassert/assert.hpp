#ifndef LIBASSERT_HPP
#define LIBASSERT_HPP

// Copyright (c) 2021-2024 Jeremy Rifkin under the MIT license
// https://github.com/jeremy-rifkin/libassert

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <sstream>
#include <string_view>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#ifdef __cpp_lib_expected
 #include <expected>
#endif

#if defined(_MSVC_LANG) && _MSVC_LANG < 201703L
 #error "libassert requires C++17 or newer"
#elif !defined(_MSVC_LANG) && __cplusplus < 201703L
 #pragma error "libassert requires C++17 or newer"
#endif

#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
 #include <compare>
#endif

#ifdef LIBASSERT_USE_MAGIC_ENUM
 // relative include so that multiple library versions don't clash
 // e.g. if both libA and libB have different versions of libassert as a public
 // dependency, then any library that consumes both will have both sets of include
 // paths. this isn't an issue for #include <assert.hpp> but becomes an issue
 // for includes within the library (libA might include from libB)
 #if __has_include(<magic_enum/magic_enum.hpp>)
    #include <magic_enum/magic_enum.hpp>
 #else
    #include <magic_enum.hpp>
 #endif
#endif

#ifdef LIBASSERT_USE_FMT
 #include <fmt/format.h>
#endif

#include <cpptrace/cpptrace.hpp>

// Ever seen an assertion library with a 1500+ line header? :)
// Block comments are used to create some visual separation and try to break the library into more manageable parts.
// I've tried as much as I can to keep logically connected parts together but there is some bootstrapping necessary.

// =====================================================================================================================
// || Preprocessor stuff                                                                                              ||
// =====================================================================================================================

#ifdef _WIN32
#define LIBASSERT_EXPORT_ATTR __declspec(dllexport)
#define LIBASSERT_IMPORT_ATTR __declspec(dllimport)
#else
#define LIBASSERT_EXPORT_ATTR __attribute__((visibility("default")))
#define LIBASSERT_IMPORT_ATTR __attribute__((visibility("default")))
#endif

#ifdef LIBASSERT_STATIC_DEFINE
 #define LIBASSERT_EXPORT
 #define LIBASSERT_NO_EXPORT
#else
 #ifndef LIBASSERT_EXPORT
  #ifdef libassert_lib_EXPORTS
   /* We are building this library */
   #define LIBASSERT_EXPORT LIBASSERT_EXPORT_ATTR
  #else
   /* We are using this library */
   #define LIBASSERT_EXPORT LIBASSERT_IMPORT_ATTR
  #endif
 #endif
#endif

#define LIBASSERT_IS_CLANG 0
#define LIBASSERT_IS_GCC 0
#define LIBASSERT_IS_MSVC 0

#if defined(__clang__)
 #undef LIBASSERT_IS_CLANG
 #define LIBASSERT_IS_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
 #undef LIBASSERT_IS_GCC
 #define LIBASSERT_IS_GCC 1
#elif defined(_MSC_VER)
 #undef LIBASSERT_IS_MSVC
 #define LIBASSERT_IS_MSVC 1
 #include <iso646.h> // alternative operator tokens are standard but msvc requires the include or /permissive- or /Za
#else
 #error "Libassert: Unsupported compiler"
#endif

#if LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC
 #define LIBASSERT_PFUNC __extension__ __PRETTY_FUNCTION__
 #define LIBASSERT_ATTR_COLD     [[gnu::cold]]
 #define LIBASSERT_ATTR_NOINLINE [[gnu::noinline]]
 #define LIBASSERT_UNREACHABLE_CALL __builtin_unreachable()
#else
 #define LIBASSERT_PFUNC __FUNCSIG__
 #define LIBASSERT_ATTR_COLD
 #define LIBASSERT_ATTR_NOINLINE __declspec(noinline)
 #define LIBASSERT_UNREACHABLE_CALL __assume(false)
#endif

#if LIBASSERT_IS_MSVC
 #define LIBASSERT_STRONG_EXPECT(expr, value) (expr)
#elif defined(__clang__) && __clang_major__ >= 11 || __GNUC__ >= 9
 #define LIBASSERT_STRONG_EXPECT(expr, value) __builtin_expect_with_probability((expr), (value), 1)
#else
 #define LIBASSERT_STRONG_EXPECT(expr, value) __builtin_expect((expr), (value))
#endif

// deal with gcc shenanigans
// at one point their std::string's move assignment was not noexcept even in c++17
// https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_dual_abi.html
#if defined(_GLIBCXX_USE_CXX11_ABI)
 // NOLINTNEXTLINE(misc-include-cleaner)
 #define LIBASSERT_GCC_ISNT_STUPID _GLIBCXX_USE_CXX11_ABI
#else
 // assume others target new abi by default - homework
 #define LIBASSERT_GCC_ISNT_STUPID 1
#endif

// always_false is just convenient to use here
#define LIBASSERT_PHONY_USE(E) ((void)libassert::detail::always_false<decltype(E)>)

#if LIBASSERT_IS_MSVC
 #pragma warning(push)
 // warning C4251: using non-dll-exported type in dll-exported type, firing on std::vector<frame_ptr> and others for
 // some reason
 // 4275 is the same thing but for base classes
 #pragma warning(disable: 4251; disable: 4275)
#endif

#if LIBASSERT_IS_GCC || __cplusplus >= 2020002L
 // __VA_OPT__ needed for GCC, https://gcc.gnu.org/bugzilla/show_bug.cgi?id=44317
 #define LIBASSERT_VA_ARGS(...) __VA_OPT__(,) __VA_ARGS__
#else
 // clang properly eats the comma with ##__VA_ARGS__
 #define LIBASSERT_VA_ARGS(...) , ##__VA_ARGS__
#endif

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

    [[nodiscard]] LIBASSERT_EXPORT std::string_view type_name_core(std::string_view signature) noexcept;

    template<typename T>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string_view type_name() noexcept {
        return type_name_core(LIBASSERT_PFUNC);
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

// =====================================================================================================================
// || Stringification micro-library                                                                                   ||
// || Note: There is some stateful stuff behind the scenes related to literal format configuration                    ||
// =====================================================================================================================

namespace libassert {
    // customization point
    template<typename T> struct stringifier /*{
        std::convertible_to<std::string> stringify(const T&);
    }*/;
}

namespace libassert::detail {
    // What can be stringified
    // Base types:
    //  - anything string-like
    //  - arithmetic types
    //  - pointers
    //  - smart pointers
    //  - nullptr_t
    //  - std::error_code/std::error_condition
    //  - orderings
    //  - enum values
    //  User-provided stringifications:
    //   - std::ostream<< overloads
    //   - std format TODO
    //   - fmt TODO
    //   - libassert::stringifier
    // Containers:
    //  - std::optional
    //  - std::variant TODO
    //  - std::expected TODO
    //  - tuples and tuple-likes
    //  - anything container-like (std::vector, std::array, std::unordered_map, C arrays, .....)
    // Priorities:
    //  - libassert::stringifier
    //  - default formatters
    //  - fmt
    //  - std fmt TODO
    //  - osteam format
    //  - instance of catch all
    // Other logistics:
    //  - Containers are only stringified if their value_type is stringifiable
    // TODO Weak pointers?

    template<typename Test, template<typename...> class Ref>
    struct is_specialization : std::false_type {};

    template<template<typename...> class Ref, typename... Args>
    struct is_specialization<Ref<Args...>, Ref>: std::true_type {};

    template<typename T>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string do_stringify(const T& v);

    namespace stringification {
        //
        // General traits
        //
        template<typename T, typename = void> class is_tuple_like : public std::false_type {};
        template<typename T>
        class is_tuple_like<
            T,
            std::void_t<
                typename std::tuple_size<T>::type, // TODO: decltype(std::tuple_size_v<T>) ?
                decltype(std::get<0>(std::declval<T>()))
            >
        > : public std::true_type {};

        namespace adl {
            using std::begin, std::end; // ADL
            template<typename T, typename = void> class is_container : public std::false_type {};
            template<typename T>
            class is_container<
                T,
                std::void_t<
                    decltype(begin(decllval<T>())),
                    decltype(end(decllval<T>()))
                >
            > : public std::true_type {};
            template<typename T, typename = void> class is_begin_deref : public std::false_type {};
            template<typename T>
            class is_begin_deref<
                T,
                std::void_t<
                    decltype(*begin(decllval<T>()))
                >
            > : public std::true_type {};
        }

        template<typename T, typename = void> class is_deref : public std::false_type {};
        template<typename T>
        class is_deref<
            T,
            std::void_t<
                decltype(*decllval<T>())
            >
        > : public std::true_type {};

        template<typename T, typename = void> class has_ostream_overload : public std::false_type {};
        template<typename T>
        class has_ostream_overload<
            T,
            std::void_t<decltype(std::declval<std::ostream>() << std::declval<T>())>
        > : public std::true_type {};

        template<typename T, typename = void> class has_stringifier : public std::false_type {};
        template<typename T>
        class has_stringifier<
            T,
            std::void_t<decltype(stringifier<strip<T>>{}.stringify(std::declval<T>()))>
        > : public std::true_type {};

        //
        // Catch all
        //

        template<typename T>
        [[nodiscard]] std::string stringify_unknown() {
            return bstringf("<instance of %s>", prettify_type(std::string(type_name<T>())).c_str());
        }

        //
        // Basic types
        //
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::string_view);
        // without nullptr_t overload msvc (without /permissive-) will call stringify(bool) and mingw
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::nullptr_t);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(char);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(bool);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(short);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(int);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(long);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(long long);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(unsigned short);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(unsigned int);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(unsigned long);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(unsigned long long);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(float);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(double);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(long double);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::error_code ec);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::error_condition ec);
        #if __cplusplus >= 202002L
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::strong_ordering);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::weak_ordering);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::partial_ordering);
        #endif
        [[nodiscard]] LIBASSERT_EXPORT
        std::string stringify_pointer_value(const void*);

        template<typename T>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify_smart_ptr(const T& t) {
            if(t) {
                return do_stringify(*t);
            } else {
                return "nullptr";
            }
        }

        template<typename T>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify_by_ostream(const T& t) {
            // clang-tidy bug here
            // NOLINTNEXTLINE(misc-const-correctness)
            std::ostringstream oss;
            oss<<t;
            return std::move(oss).str();
        }

        #ifdef LIBASSERT_USE_MAGIC_ENUM
        template<typename T, typename std::enable_if_t<std::is_enum_v<strip<T>>, int> = 0>
        LIBASSERT_ATTR_COLD [[nodiscard]] std::string stringify_enum(const T& t) {
            std::string_view name = magic_enum::enum_name(t);
            if(!name.empty()) {
                return std::string(name);
            } else {
                return bstringf(
                    "enum %s: %s",
                    prettify_type(std::string(type_name<T>())).c_str(),
                    stringify(static_cast<typename std::underlying_type<T>::type>(t)).c_str()
                );
            }
        }
        #else
        template<typename T, typename std::enable_if_t<std::is_enum_v<strip<T>>, int> = 0>
        LIBASSERT_ATTR_COLD [[nodiscard]] std::string stringify_enum(const T& t) {
            return bstringf(
                "enum %s: %s",
                prettify_type(std::string(type_name<T>())).c_str(),
                stringify(static_cast<typename std::underlying_type_t<T>>(t)).c_str()
            );
        }
        #endif

        //
        // Compositions of other types
        //
        // #ifdef __cpp_lib_expected
        // template<typename E>
        // [[nodiscard]] std::string stringify(const std::unexpected<E>& x) {
        //     return "unexpected " + stringify(x.error());
        // }

        // template<typename T, typename E>
        // [[nodiscard]] std::string stringify(const std::expected<T, E>& x) {
        //     if(x.has_value()) {
        //         if constexpr(std::is_void_v<T>) {
        //             return "expected void";
        //         } else {
        //             return "expected " + stringify(*x);
        //         }
        //     } else {
        //         return "unexpected " + stringify(x.error());
        //     }
        // }
        // #endif

        template<typename T>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify(const std::optional<T>& t) {
            if(t) {
                return do_stringify(t.value());
            } else {
                return "nullopt";
            }
        }

        inline constexpr std::size_t max_container_print_items = 1000;

        template<typename T>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify_container(const T& container) {
            using std::begin, std::end; // ADL
            std::string str = "[";
            const auto begin_it = begin(container);
            std::size_t count = 0;
            for(auto it = begin_it; it != end(container); it++) {
                if(it != begin_it) {
                    str += ", ";
                }
                str += do_stringify(*it);
                if(++count == max_container_print_items) {
                    str += ", ...";
                    break;
                }
            }
            str += "]";
            return str;
        }

        // I'm going to assume at least one index because is_tuple_like requires index 0 to exist
        template<typename T, size_t... I>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify_tuple_like_impl(const T& t, std::index_sequence<I...>) {
            return "[" + (do_stringify(std::get<0>(t)) + ... + (", " + do_stringify(std::get<I + 1>(t)))) + "]";
        }

        template<typename T>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify_tuple_like(const T& t) {
            return stringify_tuple_like_impl(t, std::make_index_sequence<std::tuple_size<T>::value - 1>{});
        }
    }

    template<typename T, typename = void>
    class has_value_type : public std::false_type {};
    template<typename T>
    class has_value_type<
        T,
        std::void_t<typename T::value_type>
    > : public std::true_type {};

    template<typename T> inline constexpr bool is_smart_pointer =
        is_specialization<T, std::unique_ptr>::value
        || is_specialization<T, std::shared_ptr>::value; // TODO: Handle weak_ptr too?

    template<typename T, typename = void>
    class can_basic_stringify : public std::false_type {};
    template<typename T>
    class can_basic_stringify<
        T,
        std::void_t<decltype(stringification::stringify(std::declval<T>()))>
    > : public std::true_type {};

    template<typename T> constexpr bool stringifiable_container();

    template<typename T> inline constexpr bool stringifiable =
        stringification::has_stringifier<T>::value
        || std::is_convertible_v<T, std::string_view>
        || (std::is_pointer_v<T> || std::is_function_v<T>)
        || std::is_enum_v<T>
        || (stringification::is_tuple_like<T>::value && stringifiable_container<T>())
        || (stringification::adl::is_container<T>::value && stringifiable_container<T>())
        || can_basic_stringify<T>::value
        || stringifiable_container<T>();

    template<typename T, size_t... I> constexpr bool tuple_has_stringifiable_args_core(std::index_sequence<I...>) {
        return (
            stringifiable<decltype(std::get<0>(std::declval<T>()))>
            || ...
            || stringifiable<decltype(std::get<I>(std::declval<T>()))>
        );
    }

    template<typename T> inline constexpr bool tuple_has_stringifiable_args =
        tuple_has_stringifiable_args_core<T>(std::make_index_sequence<std::tuple_size<T>::value - 1>{});

    template<typename T> constexpr bool stringifiable_container() {
        // TODO: Guard against std::expected....?
        if constexpr(has_value_type<T>::value) {
            if constexpr(std::is_same_v<typename T::value_type, T>) {
                return false;
            } else {
                return stringifiable<typename T::value_type>;
            }
        } else if constexpr(std::is_array_v<typename std::remove_reference_t<T>>) { // C arrays
            return stringifiable<decltype(std::declval<T>()[0])>;
        } else if constexpr(stringification::is_tuple_like<T>::value) {
            return tuple_has_stringifiable_args<T>;
        } else {
            return false;
        }
    }

    template<typename T>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string do_stringify(const T& v) {
        if constexpr(stringification::has_stringifier<T>::value) {
            return stringifier<strip<T>>{}.stringify(v);
        } else if constexpr(std::is_convertible_v<T, std::string_view>) {
            if constexpr(std::is_pointer_v<T> || std::is_same_v<T, std::nullptr_t>) {
                if(v == nullptr) {
                    return "nullptr";
                }
            }
            return stringification::stringify(std::string_view(v));
        } else if constexpr(std::is_pointer_v<T> || std::is_function_v<T>) {
            return stringification::stringify_pointer_value(reinterpret_cast<const void*>(v));
        } else if constexpr(is_smart_pointer<T>) {
            #ifndef LIBASSERT_NO_STRINGIFY_SMART_POINTER_OBJECTS
             if(stringifiable<typename T::element_type>) {
            #else
             if(false) {
            #endif
                return stringification::stringify_smart_ptr(v);
            } else {
                return stringification::stringify_pointer_value(v.get());
            }
        } else if constexpr(std::is_enum_v<T>) {
            return stringification::stringify_enum(v);
        } else if constexpr(stringification::is_tuple_like<T>::value) {
            if constexpr(stringifiable_container<T>()) {
                return stringification::stringify_tuple_like(v);
            } else {
                return stringification::stringify_unknown<T>();
            }
        } else if constexpr(stringification::adl::is_container<T>::value) {
            if constexpr(stringifiable_container<T>()) {
                return stringification::stringify_container(v);
            } else {
                return stringification::stringify_unknown<T>();
            }
        } else if constexpr(can_basic_stringify<T>::value) {
            return stringification::stringify(v);
        } else if constexpr(stringification::has_ostream_overload<T>::value) {
            return stringification::stringify_by_ostream(v);
        }
        #ifdef LIBASSERT_USE_FMT
        else if constexpr(fmt::is_formattable<T>::value) {
            return fmt::format("{}", v);
        }
        #endif
        else {
            return stringification::stringify_unknown<T>();
        }
        // TODO fmt / std fmt
    }

    // Top-level stringify utility
    template<typename T>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string generate_stringification(const T& v) {
        if constexpr(
            stringification::adl::is_container<T>::value
            && !is_string_type<T>
            && stringifiable_container<T>()
        ) {
            return prettify_type(std::string(type_name<T>())) + ": " + do_stringify(v);
        } else if constexpr(stringification::is_tuple_like<T>::value && stringifiable_container<T>()) {
            return prettify_type(std::string(type_name<T>())) + ": " + do_stringify(v);
        } else if constexpr((std::is_pointer_v<T> && !is_string_type<T>) || is_smart_pointer<T>) {
            return prettify_type(std::string(type_name<T>())) + ": " + do_stringify(v);
        } else if constexpr(is_specialization<T, std::optional>::value) {
            return prettify_type(std::string(type_name<T>())) + ": " + do_stringify(v);
        } else {
            return do_stringify(v);
        }
    }
}

// =====================================================================================================================
// || Expression decomposition micro-library                                                                          ||
// =====================================================================================================================

namespace libassert::detail {
    // Lots of boilerplate
    // Using int comparison functions here to support proper signed comparisons. Need to make sure
    // assert(map.count(1) == 2) doesn't produce a warning. It wouldn't under normal circumstances
    // but it would in this library due to the parameters being forwarded down a long chain.
    // And we want to provide as much robustness as possible anyways.
    // Copied and pasted from https://en.cppreference.com/w/cpp/utility/intcmp
    // Not using std:: versions because library is targeting C++17
    template<typename T, typename U>
    [[nodiscard]] constexpr bool cmp_equal(T t, U u) {
        using UT = std::make_unsigned_t<T>;
        using UU = std::make_unsigned_t<U>;
        if constexpr(std::is_signed_v<T> == std::is_signed_v<U>) {
            return t == u;
        } else if constexpr(std::is_signed_v<T>) {
            return t >= 0 && UT(t) == u;
        } else {
            return u >= 0 && t == UU(u);
        }
    }

    template<typename T, typename U>
    [[nodiscard]] constexpr bool cmp_not_equal(T t, U u) {
        return !cmp_equal(t, u);
    }

    template<typename T, typename U>
    [[nodiscard]] constexpr bool cmp_less(T t, U u) {
        using UT = std::make_unsigned_t<T>;
        using UU = std::make_unsigned_t<U>;
        if constexpr(std::is_signed_v<T> == std::is_signed_v<U>) {
            return t < u;
        } else if constexpr(std::is_signed_v<T>) {
            return t < 0  || UT(t) < u;
        } else {
            return u >= 0 && t < UU(u);
        }
    }

    template<typename T, typename U>
    [[nodiscard]] constexpr bool cmp_greater(T t, U u) {
        return cmp_less(u, t);
    }

    template<typename T, typename U>
    [[nodiscard]] constexpr bool cmp_less_equal(T t, U u) {
        return !cmp_less(u, t);
    }

    template<typename T, typename U>
    [[nodiscard]] constexpr bool cmp_greater_equal(T t, U u) {
        return !cmp_less(t, u);
    }

    // Lots of boilerplate
    // std:: implementations don't allow two separate types for lhs/rhs
    // Note: is this macro potentially bad when it comes to debugging(?)
    namespace ops {
        #define LIBASSERT_GEN_OP_BOILERPLATE(name, op) struct name { \
            static constexpr std::string_view op_string = #op; \
            template<typename A, typename B> \
            LIBASSERT_ATTR_COLD [[nodiscard]] \
            constexpr decltype(auto) operator()(A&& lhs, B&& rhs) const { /* no need to forward ints */ \
                return std::forward<A>(lhs) op std::forward<B>(rhs); \
            } \
        }
        #define LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(name, op, cmp) struct name { \
            static constexpr std::string_view op_string = #op; \
            template<typename A, typename B> \
            LIBASSERT_ATTR_COLD [[nodiscard]] \
            constexpr decltype(auto) operator()(A&& lhs, B&& rhs) const { /* no need to forward ints */ \
                if constexpr(is_integral_and_not_bool<A> && is_integral_and_not_bool<B>) return cmp(lhs, rhs); \
                else return std::forward<A>(lhs) op std::forward<B>(rhs); \
            } \
        }
        LIBASSERT_GEN_OP_BOILERPLATE(shl, <<);
        LIBASSERT_GEN_OP_BOILERPLATE(shr, >>);
        #if __cplusplus >= 202002L
         LIBASSERT_GEN_OP_BOILERPLATE(spaceship, <=>);
        #endif
        #ifdef LIBASSERT_SAFE_COMPARISONS
        // TODO: Make a SAFE() wrapper...
        LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(eq,   ==, cmp_equal);
        LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(neq,  !=, cmp_not_equal);
        LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(gt,    >, cmp_greater);
        LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(lt,    <, cmp_less);
        LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(gteq, >=, cmp_greater_equal);
        LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(lteq, <=, cmp_less_equal);
        #else
        LIBASSERT_GEN_OP_BOILERPLATE(eq,   ==);
        LIBASSERT_GEN_OP_BOILERPLATE(neq,  !=);
        LIBASSERT_GEN_OP_BOILERPLATE(gt,    >);
        LIBASSERT_GEN_OP_BOILERPLATE(lt,    <);
        LIBASSERT_GEN_OP_BOILERPLATE(gteq, >=);
        LIBASSERT_GEN_OP_BOILERPLATE(lteq, <=);
        #endif
        LIBASSERT_GEN_OP_BOILERPLATE(band,   &);
        LIBASSERT_GEN_OP_BOILERPLATE(bxor,   ^);
        LIBASSERT_GEN_OP_BOILERPLATE(bor,    |);
        #ifdef LIBASSERT_DECOMPOSE_BINARY_LOGICAL
         struct land {
             static constexpr std::string_view op_string = "&&";
             template<typename A, typename B>
             LIBASSERT_ATTR_COLD [[nodiscard]]
             constexpr decltype(auto) operator()(A&& lhs, B&& rhs) const {
                 // Go out of the way to support old-style ASSERT(foo && "Message")
                 #if LIBASSERT_IS_GCC
                  if constexpr(is_string_literal<B>) {
                      #pragma GCC diagnostic push
                      #pragma GCC diagnostic ignored "-Wnonnull-compare"
                      return std::forward<A>(lhs) && std::forward<B>(rhs);
                      #pragma GCC diagnostic pop
                  } else {
                      return std::forward<A>(lhs) && std::forward<B>(rhs);
                  }
                 #else
                  return std::forward<A>(lhs) && std::forward<B>(rhs);
                 #endif
             }
         };
         LIBASSERT_GEN_OP_BOILERPLATE(lor,    ||);
        #endif
        LIBASSERT_GEN_OP_BOILERPLATE(assign, =);
        LIBASSERT_GEN_OP_BOILERPLATE(add_assign,  +=);
        LIBASSERT_GEN_OP_BOILERPLATE(sub_assign,  -=);
        LIBASSERT_GEN_OP_BOILERPLATE(mul_assign,  *=);
        LIBASSERT_GEN_OP_BOILERPLATE(div_assign,  /=);
        LIBASSERT_GEN_OP_BOILERPLATE(mod_assign,  %=);
        LIBASSERT_GEN_OP_BOILERPLATE(shl_assign,  <<=);
        LIBASSERT_GEN_OP_BOILERPLATE(shr_assign,  >>=);
        LIBASSERT_GEN_OP_BOILERPLATE(band_assign, &=);
        LIBASSERT_GEN_OP_BOILERPLATE(bxor_assign, ^=);
        LIBASSERT_GEN_OP_BOILERPLATE(bor_assign,  |=);
        #undef LIBASSERT_GEN_OP_BOILERPLATE
        #undef LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL
    }

    // I learned this automatic expression decomposition trick from lest:
    // https://github.com/martinmoene/lest/blob/master/include/lest/lest.hpp#L829-L853
    //
    // I have improved upon the trick by supporting more operators and generally improving
    // functionality.
    //
    // Some cases to test and consider:
    //
    // Expression   Parsed as
    // Basic:
    // false        << false
    // a == b       (<< a) == b
    //
    // Equal precedence following:
    // a << b       (<< a) << b
    // a << b << c  ((<< a) << b) << c
    // a << b + c   (<< a) << (b + c)
    // a << b < c   ((<< a) << b) < c  // edge case
    //
    // Higher precedence following:
    // a + b        << (a + b)
    // a + b + c    << ((a + b) + c)
    // a + b * c    << (a + (b * c))
    // a + b < c    (<< (a + b)) < c
    //
    // Lower precedence following:
    // a < b        (<< a) < b
    // a < b < c    ((<< a) < b) < c
    // a < b + c    (<< a) < (b + c)
    // a < b == c   ((<< a) < b) == c // edge case

    template<typename A = nothing, typename B = nothing, typename C = nothing>
    struct expression_decomposer {
        A a;
        B b;
        explicit constexpr expression_decomposer() = default;
        ~expression_decomposer() = default;
        // not copyable
        constexpr expression_decomposer(const expression_decomposer&) = delete;
        constexpr expression_decomposer& operator=(const expression_decomposer&) = delete;
        // allow move construction
        constexpr expression_decomposer(expression_decomposer&&)
        #if !LIBASSERT_IS_GCC || __GNUC__ >= 10 // gcc 9 has some issue with the move constructor being noexcept
         noexcept
        #endif
         = default;
        constexpr expression_decomposer& operator=(expression_decomposer&&) = delete;
        // value constructors
        template<typename U, typename std::enable_if_t<!isa<U, expression_decomposer>, int> = 0>
        // NOLINTNEXTLINE(bugprone-forwarding-reference-overload) // TODO
        explicit constexpr expression_decomposer(U&& _a) : a(std::forward<U>(_a)) {}
        template<typename U, typename V>
        explicit constexpr expression_decomposer(U&& _a, V&& _b) : a(std::forward<U>(_a)), b(std::forward<V>(_b)) {}
        /* Ownership logic:
         *  One of two things can happen to this class
         *   - Either it is composed with another operation
         *         + The value of the subexpression represented by this is computed (either get_value()
         *        or operator bool), either A& or C()(a, b)
         *      + Or, just the lhs is moved B is nothing
         *   - Or this class represents the whole expression
         *      + The value is computed (either A& or C()(a, b))
         *      + a and b are referenced freely
         *      + Either the value is taken or a is moved out
         * Ensuring the value is only computed once is left to the assert handler
         */
        [[nodiscard]]
        constexpr decltype(auto) get_value() {
            if constexpr(is_nothing<C>) {
                static_assert(is_nothing<B> && !is_nothing<A>);
                return (a);
            } else {
                return C()(a, b);
            }
        }
        [[nodiscard]]
        constexpr operator bool() { // for ternary support
            return (bool)get_value();
        }
        // return true if the lhs should be returned, false if full computed value should be
        [[nodiscard]]
        constexpr bool ret_lhs() {
            static_assert(!is_nothing<A>);
            static_assert(is_nothing<B> == is_nothing<C>);
            if constexpr(is_nothing<C>) {
                // if there is no top-level binary operation, A is the only thing that can be returned
                return true;
            } else {
                // return LHS for the following types;
                return C::op_string == "=="
                    || C::op_string == "!="
                    || C::op_string == "<"
                    || C::op_string == ">"
                    || C::op_string == "<="
                    || C::op_string == ">="
                    || C::op_string == "&&"
                    || C::op_string == "||";
                // don't return LHS for << >> & ^ | and all assignments
            }
        }
        [[nodiscard]]
        constexpr A take_lhs() { // should only be called if ret_lhs() == true
            if constexpr(std::is_lvalue_reference_v<A>) {
                return ((((a))));
            } else {
                return std::move(a);
            }
        }
        // Need overloads for operators with precedence <= bitshift
        // TODO: spaceship operator?
        // Note: Could decompose more than just comparison and boolean operators, but it would take
        // a lot of work and I don't think it's beneficial for this library.
        template<typename O> [[nodiscard]] constexpr auto operator<<(O&& operand) && {
            using Q = std::conditional_t<std::is_rvalue_reference_v<O>, std::remove_reference_t<O>, O>;
            if constexpr(is_nothing<A>) {
                static_assert(is_nothing<B> && is_nothing<C>);
                return expression_decomposer<Q, nothing, nothing>(std::forward<O>(operand));
            } else if constexpr(is_nothing<B>) {
                static_assert(is_nothing<C>);
                return expression_decomposer<A, Q, ops::shl>(std::forward<A>(a), std::forward<O>(operand));
            } else {
                static_assert(!is_nothing<C>);
                return expression_decomposer<decltype(get_value()), O, ops::shl>(
                    std::forward<A>(get_value()),
                    std::forward<O>(operand)
                );
            }
        }
        #define LIBASSERT_GEN_OP_BOILERPLATE(functor, op) \
        template<typename O> [[nodiscard]] constexpr auto operator op(O&& operand) && { \
            static_assert(!is_nothing<A>); \
            using Q = std::conditional_t<std::is_rvalue_reference_v<O>, std::remove_reference_t<O>, O>; \
            if constexpr(is_nothing<B>) { \
                static_assert(is_nothing<C>); \
                return expression_decomposer<A, Q, functor>(std::forward<A>(a), std::forward<O>(operand)); \
            } else { \
                static_assert(!is_nothing<C>); \
                using V = decltype(get_value()); \
                return expression_decomposer<V, Q, functor>(std::forward<V>(get_value()), std::forward<O>(operand)); \
            } \
        }
        LIBASSERT_GEN_OP_BOILERPLATE(ops::shr, >>)
        #if __cplusplus >= 202002L
         LIBASSERT_GEN_OP_BOILERPLATE(ops::spaceship, <=>)
        #endif
        LIBASSERT_GEN_OP_BOILERPLATE(ops::eq, ==)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::neq, !=)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::gt, >)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::lt, <)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::gteq, >=)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::lteq, <=)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::band, &)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::bxor, ^)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::bor, |)
        #ifdef LIBASSERT_DECOMPOSE_BINARY_LOGICAL
         LIBASSERT_GEN_OP_BOILERPLATE(ops::land, &&)
         LIBASSERT_GEN_OP_BOILERPLATE(ops::lor, ||)
        #endif
        LIBASSERT_GEN_OP_BOILERPLATE(ops::assign, =) // NOLINT(cppcoreguidelines-c-copy-assignment-signature, misc-unconventional-assign-operator)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::add_assign, +=)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::sub_assign, -=)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::mul_assign, *=)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::div_assign, /=)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::mod_assign, %=)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::shl_assign, <<=)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::shr_assign, >>=)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::band_assign, &=)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::bxor_assign, ^=)
        LIBASSERT_GEN_OP_BOILERPLATE(ops::bor_assign, |=)
        #undef LIBASSERT_GEN_OP_BOILERPLATE
    };

    // for ternary support
    template<typename U>
    expression_decomposer(U&&) -> expression_decomposer<
        std::conditional_t<std::is_rvalue_reference_v<U>, std::remove_reference_t<U>, U>
    >;
}

// =====================================================================================================================
// || Libassert public interface                                                                                      ||
// =====================================================================================================================

namespace libassert {
    // returns the width of the terminal represented by fd, will be 0 on error
    [[nodiscard]] LIBASSERT_EXPORT int terminal_width(int fd);

    // Enable virtual terminal processing on windows terminals
    LIBASSERT_ATTR_COLD LIBASSERT_EXPORT void enable_virtual_terminal_processing_if_needed();

    inline constexpr int stdin_fileno = 0;
    inline constexpr int stdout_fileno = 1;
    inline constexpr int stderr_fileno = 2;

    LIBASSERT_ATTR_COLD LIBASSERT_EXPORT bool isatty(int fd);

    // returns the type name of T
    template<typename T>
    [[nodiscard]] std::string_view type_name() noexcept {
        return detail::type_name<T>();
    }

    // returns the prettified type name for T
    template<typename T> // TODO: Use this above....
    [[nodiscard]] std::string pretty_type_name() noexcept {
        return detail::prettify_type(std::string(detail::type_name<T>()));
    }

    // returns a debug stringification of t
    template<typename T>
    [[nodiscard]] std::string stringify(const T& t) {
        return detail::generate_stringification(t);
    }

    // NOTE: string view underlying data should have static storage duration, or otherwise live as long as the scheme
    // is in use
    struct color_scheme {
        std::string_view string;
        std::string_view escape;
        std::string_view keyword;
        std::string_view named_literal;
        std::string_view number;
        std::string_view punctuation;
        std::string_view operator_token;
        std::string_view call_identifier;
        std::string_view scope_resolution_identifier;
        std::string_view identifier;
        std::string_view accent;
        std::string_view unknown;
        std::string_view reset;
        LIBASSERT_EXPORT const static color_scheme ansi_basic;
        LIBASSERT_EXPORT const static color_scheme ansi_rgb;
        LIBASSERT_EXPORT const static color_scheme blank;
    };

    LIBASSERT_EXPORT void set_color_scheme(const color_scheme&);
    LIBASSERT_EXPORT const color_scheme& get_color_scheme();

    // set separator used for diagnostics, by default it is "=>"
    // note: not thread-safe
    LIBASSERT_EXPORT void set_separator(std::string_view separator);

    // generates a stack trace, formats to the given width
    [[nodiscard]] LIBASSERT_ATTR_NOINLINE LIBASSERT_EXPORT
    std::string stacktrace(int width = 0, const color_scheme& scheme = get_color_scheme(), std::size_t skip = 0);

    enum class literal_format : unsigned {
        // integers and floats are decimal by default, chars are of course chars, and everything else only has one
        // format that makes sense
        default_format = 0,
        integer_hex = 1,
        integer_octal = 2,
        integer_binary = 4,
        integer_character = 8, // format integers as characters and characters as integers
        float_hex = 16,
    };

    [[nodiscard]] constexpr literal_format operator|(literal_format a, literal_format b) {
        return static_cast<literal_format>(
            static_cast<std::underlying_type<literal_format>::type>(a) |
            static_cast<std::underlying_type<literal_format>::type>(b)
        );
    }

    enum class literal_format_mode {
        infer, // infer literal formats based on the assertion condition
        no_variations, // don't do any literal format variations, just default
        fixed_variations // use a fixed set of formats always; note the default format will always be used
    };

    // NOTE: Should not be called during handling of an assertion in the current thread
    LIBASSERT_EXPORT void set_literal_format_mode(literal_format_mode);

    // NOTE: Should not be called during handling of an assertion in the current thread
    // set a fixed literal format configuration, automatically changes the literal_format_mode; note that the default
    // format will always be used along with others
    LIBASSERT_EXPORT void set_fixed_literal_format(literal_format);

    enum class path_mode {
        // full path is used
        full,
        // only enough folders needed to disambiguate are provided
        disambiguated, // TODO: Maybe just a bad idea...?
        // only the file name is used
        basename,
    };
    LIBASSERT_EXPORT void set_path_mode(path_mode mode);

    enum class assert_type {
        debug_assertion,
        assertion,
        assumption,
        panic,
        unreachable
    };

    struct assertion_info;

    LIBASSERT_EXPORT void set_failure_handler(void (*handler)(const assertion_info&));

    struct LIBASSERT_EXPORT binary_diagnostics_descriptor {
        std::string left_expression;
        std::string right_expression;
        std::string left_stringification;
        std::string right_stringification;
        bool multiple_formats;
        binary_diagnostics_descriptor(); // = default; in the .cpp
        binary_diagnostics_descriptor(
            std::string_view left_expression,
            std::string_view right_expression,
            std::string&& left_stringification,
            std::string&& right_stringification,
            bool multiple_formats
        );
        ~binary_diagnostics_descriptor(); // = default; in the .cpp
        binary_diagnostics_descriptor(const binary_diagnostics_descriptor&) = delete;
        binary_diagnostics_descriptor(binary_diagnostics_descriptor&&) noexcept; // = default; in the .cpp
        binary_diagnostics_descriptor& operator=(const binary_diagnostics_descriptor&) = delete;
        binary_diagnostics_descriptor&
        operator=(binary_diagnostics_descriptor&&) noexcept(LIBASSERT_GCC_ISNT_STUPID); // = default; in the .cpp
    };

    namespace detail {
        struct sv_span {
            const std::string_view* data;
            std::size_t size;
        };

        // collection of assertion data that can be put in static storage and all passed by a single pointer
        struct LIBASSERT_EXPORT assert_static_parameters {
            std::string_view macro_name;
            assert_type type;
            std::string_view expr_str;
            source_location location;
            sv_span args_strings;
        };
    }

    struct extra_diagnostic {
        std::string_view expression;
        std::string stringification;
    };

    namespace detail {
        class path_handler {
        public:
            virtual ~path_handler() = default;
            virtual std::string_view resolve_path(std::string_view) = 0;
            virtual bool has_add_path() const;
            virtual void add_path(std::string_view);
            virtual void finalize();
        };
    }

    struct LIBASSERT_EXPORT assertion_info {
        std::string_view macro_name;
        assert_type type;
        std::string_view expression_string;
        std::string_view file_name;
        std::uint32_t line;
        std::string_view function;
        std::optional<std::string> message;
        std::optional<binary_diagnostics_descriptor> binary_diagnostics;
        std::vector<extra_diagnostic> extra_diagnostics;
        size_t n_args;
    private:
        mutable std::variant<cpptrace::raw_trace, cpptrace::stacktrace> trace; // lazy, resolved when needed
        mutable std::unique_ptr<detail::path_handler> path_handler;
        detail::path_handler* get_path_handler() const; // will get and setup the path handler
    public:
        assertion_info() = delete;
        assertion_info(
            const detail::assert_static_parameters* static_params,
            cpptrace::raw_trace&& raw_trace,
            size_t n_args
        );
        ~assertion_info();
        assertion_info(const assertion_info&) = delete;
        assertion_info(assertion_info&&);
        assertion_info& operator=(const assertion_info&) = delete;
        assertion_info& operator=(assertion_info&&);

        std::string_view action() const;

        const cpptrace::raw_trace& get_raw_trace() const;
        const cpptrace::stacktrace& get_stacktrace() const;

        [[nodiscard]] std::string header(int width = 0, const color_scheme& scheme = get_color_scheme()) const;
        [[nodiscard]] std::string tagline(const color_scheme& scheme = get_color_scheme()) const;
        [[nodiscard]] std::string location() const;
        [[nodiscard]] std::string statement(const color_scheme& scheme = get_color_scheme()) const;
        [[nodiscard]] std::string print_binary_diagnostics(int width = 0, const color_scheme& scheme = get_color_scheme()) const;
        [[nodiscard]] std::string print_extra_diagnostics(int width = 0, const color_scheme& scheme = get_color_scheme()) const;
        [[nodiscard]] std::string print_stacktrace(int width = 0, const color_scheme& scheme = get_color_scheme()) const;

        [[nodiscard]] std::string to_string(int width = 0, const color_scheme& scheme = get_color_scheme()) const;
    };
}

// =====================================================================================================================
// || Library core                                                                                                    ||
// =====================================================================================================================

namespace libassert::detail {
    /*
     * C++ syntax analysis and literal formatting
     */

    // get current literal_format configuration for the thread
    [[nodiscard]] LIBASSERT_EXPORT literal_format get_thread_current_literal_format();

    // sets the current literal_format configuration for the thread
    LIBASSERT_EXPORT void set_thread_current_literal_format(literal_format format);

    LIBASSERT_EXPORT literal_format set_literal_format(
        std::string_view left_expression,
        std::string_view right_expression,
        std::string_view op,
        bool integer_character
    );
    LIBASSERT_EXPORT void restore_literal_format(literal_format);
    // does the current literal format config have multiple formats
    LIBASSERT_EXPORT bool has_multiple_formats();

    [[nodiscard]] LIBASSERT_EXPORT std::pair<std::string, std::string> decompose_expression(
        std::string_view expression,
        std::string_view target_op
    );

    /*
     * System wrappers
     */

    [[nodiscard]] LIBASSERT_EXPORT std::string strerror_wrapper(int err); // stupid C stuff, stupid microsoft stuff

    /*
     * assert diagnostics generation
     */

    template<typename A, typename B>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    binary_diagnostics_descriptor generate_binary_diagnostic(
        const A& left,
        const B& right,
        std::string_view left_str,
        std::string_view right_str,
        std::string_view op
    ) {
        constexpr bool either_is_character = isa<A, char> || isa<B, char>;
        constexpr bool either_is_arithmetic = is_arith_not_bool_char<A> || is_arith_not_bool_char<B>;
        literal_format previous_format = set_literal_format(
            left_str,
            right_str,
            op,
            either_is_character && either_is_arithmetic
        );
        binary_diagnostics_descriptor descriptor(
            left_str,
            right_str,
            generate_stringification(left),
            generate_stringification(right),
            has_multiple_formats()
        );
        restore_literal_format(previous_format);
        return descriptor;
    }

    #define LIBASSERT_X(x) #x
    #define LIBASSERT_Y(x) LIBASSERT_X(x)
    constexpr const std::string_view errno_expansion = LIBASSERT_Y(errno);
    #undef LIBASSERT_Y
    #undef LIBASSERT_X

    struct pretty_function_name_wrapper {
        const char* pretty_function;
    };

    inline void process_arg( // TODO: Don't inline
        assertion_info& info,
        size_t,
        sv_span,
        const pretty_function_name_wrapper& t
    ) {
        info.function = t.pretty_function;
    }

    template<typename T>
    LIBASSERT_ATTR_COLD
    // TODO
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void process_arg(assertion_info& info, size_t i, sv_span args_strings, const T& t) {
        if constexpr(isa<T, strip<decltype(errno)>>) {
            if(args_strings.data[i] == errno_expansion) {
                info.extra_diagnostics.push_back({ "errno", bstringf("%2d \"%s\"", t, strerror_wrapper(t).c_str()) });
                return;
            }
        } else if constexpr(is_string_type<T>) {
            if(i == 0) {
                if constexpr(std::is_pointer_v<T>) {
                    if(t == nullptr) {
                        info.message = "(nullptr)";
                        return;
                    }
                }
                info.message = t;
                return;
            }
        }
        info.extra_diagnostics.push_back({ args_strings.data[i], generate_stringification(t) });
    }

    template<typename... Args>
    LIBASSERT_ATTR_COLD
    void process_args(assertion_info& info, sv_span args_strings, Args&... args) {
        size_t i = 0;
        (process_arg(info, i++, args_strings, args), ...);
        (void)args_strings;
    }
}

/*
 * Actual top-level assertion processing
 */

namespace libassert::detail {
    LIBASSERT_EXPORT void fail(const assertion_info& info);

    template<typename A, typename B, typename C, typename... Args>
    LIBASSERT_ATTR_COLD LIBASSERT_ATTR_NOINLINE
    // TODO: Re-evaluate forwarding here.
    void process_assert_fail(
        expression_decomposer<A, B, C>& decomposer,
        const assert_static_parameters* params,
        // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
        Args&&... args
    ) {
        const size_t sizeof_extra_diagnostics = sizeof...(args) - 1; // - 1 for pretty function signature
        LIBASSERT_PRIMITIVE_ASSERT(sizeof...(args) <= params->args_strings.size);
        assertion_info info(
            params,
            cpptrace::generate_raw_trace(),
            sizeof_extra_diagnostics
        );
        // process_args fills in the message, extra_diagnostics, and pretty_function
        process_args(info, params->args_strings, args...);
        // generate binary diagnostics
        if constexpr(is_nothing<C>) {
            static_assert(is_nothing<B> && !is_nothing<A>);
            if constexpr(isa<A, bool>) {
                (void)decomposer; // suppress warning in msvc
            } else {
                info.binary_diagnostics = generate_binary_diagnostic(
                    decomposer.a,
                    true,
                    params->expr_str,
                    "true",
                    "=="
                );
            }
        } else {
            auto [left_expression, right_expression] = decompose_expression(params->expr_str, C::op_string);
            info.binary_diagnostics = generate_binary_diagnostic(
                decomposer.a,
                decomposer.b,
                left_expression,
                right_expression,
                C::op_string
            );
        }
        // send off
        fail(info);
    }

    template<typename... Args>
    LIBASSERT_ATTR_COLD [[noreturn]] LIBASSERT_ATTR_NOINLINE
    // TODO: Re-evaluate forwarding here.
    void process_panic(
        const assert_static_parameters* params,
        // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
        Args&&... args
    ) {
        const size_t sizeof_extra_diagnostics = sizeof...(args) - 1; // - 1 for pretty function signature
        LIBASSERT_PRIMITIVE_ASSERT(sizeof...(args) <= params->args_strings.size);
        assertion_info info(
            params,
            cpptrace::generate_raw_trace(),
            sizeof_extra_diagnostics
        );
        // process_args fills in the message, extra_diagnostics, and pretty_function
        process_args(info, params->args_strings, args...);
        // send off
        fail(info);
        LIBASSERT_PRIMITIVE_PANIC("PANIC/UNREACHABLE failure handler returned");
    }

    // TODO: Re-evaluate benefit of this at all in non-cold path code
    template<typename A, typename B, typename C, typename... Args>
    LIBASSERT_ATTR_COLD LIBASSERT_ATTR_NOINLINE [[nodiscard]]
    expression_decomposer<A, B, C> process_assert_fail_m(
        expression_decomposer<A, B, C> decomposer,
        const assert_static_parameters* params,
        Args&&... args
    ) {
        process_assert_fail(decomposer, params, std::forward<Args>(args)...);
        return decomposer;
    }

    template<typename A, typename B, typename C, typename... Args>
    LIBASSERT_ATTR_COLD LIBASSERT_ATTR_NOINLINE
    void process_assert_fail_n(
        expression_decomposer<A, B, C> decomposer,
        const assert_static_parameters* params,
        Args&&... args
    ) {
        process_assert_fail(decomposer, params, std::forward<Args>(args)...);
    }

    template<typename T>
    struct assert_value_wrapper {
        T value;
    };

    template<
        bool R, bool ret_lhs, bool value_is_lval_ref,
        typename T, typename A, typename B, typename C
    >
    constexpr auto get_expression_return_value(T& value, expression_decomposer<A, B, C>& decomposer) {
        if constexpr(R) {
            if constexpr(ret_lhs) {
                if constexpr(std::is_lvalue_reference_v<A>) {
                    return assert_value_wrapper<A>{decomposer.take_lhs()};
                } else {
                    return assert_value_wrapper<A>{std::move(decomposer.take_lhs())};
                }
            } else {
                if constexpr(value_is_lval_ref) {
                    return assert_value_wrapper<T&>{value};
                } else {
                    return assert_value_wrapper<T>{std::move(value)};
                }
            }
        }
    }
}

#if LIBASSERT_IS_MSVC
 #pragma warning(pop)
#endif

#if LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC || (defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL == 0)
 // Macro mapping utility by William Swanson https://github.com/swansontec/map-macro/blob/master/map.h
 #define LIBASSERT_EVAL0(...) __VA_ARGS__
 #define LIBASSERT_EVAL1(...) LIBASSERT_EVAL0(LIBASSERT_EVAL0(LIBASSERT_EVAL0(__VA_ARGS__)))
 #define LIBASSERT_EVAL2(...) LIBASSERT_EVAL1(LIBASSERT_EVAL1(LIBASSERT_EVAL1(__VA_ARGS__)))
 #define LIBASSERT_EVAL3(...) LIBASSERT_EVAL2(LIBASSERT_EVAL2(LIBASSERT_EVAL2(__VA_ARGS__)))
 #define LIBASSERT_EVAL4(...) LIBASSERT_EVAL3(LIBASSERT_EVAL3(LIBASSERT_EVAL3(__VA_ARGS__)))
 #define LIBASSERT_EVAL(...)  LIBASSERT_EVAL4(LIBASSERT_EVAL4(LIBASSERT_EVAL4(__VA_ARGS__)))
 #define LIBASSERT_MAP_END(...)
 #define LIBASSERT_MAP_OUT
 #define LIBASSERT_MAP_COMMA ,
 #define LIBASSERT_MAP_GET_END2() 0, LIBASSERT_MAP_END
 #define LIBASSERT_MAP_GET_END1(...) LIBASSERT_MAP_GET_END2
 #define LIBASSERT_MAP_GET_END(...) LIBASSERT_MAP_GET_END1
 #define LIBASSERT_MAP_NEXT0(test, next, ...) next LIBASSERT_MAP_OUT
 #define LIBASSERT_MAP_NEXT1(test, next) LIBASSERT_MAP_NEXT0(test, next, 0)
 #define LIBASSERT_MAP_NEXT(test, next)  LIBASSERT_MAP_NEXT1(LIBASSERT_MAP_GET_END test, next)
 #define LIBASSERT_MAP0(f, x, peek, ...) f(x) LIBASSERT_MAP_NEXT(peek, LIBASSERT_MAP1)(f, peek, __VA_ARGS__)
 #define LIBASSERT_MAP1(f, x, peek, ...) f(x) LIBASSERT_MAP_NEXT(peek, LIBASSERT_MAP0)(f, peek, __VA_ARGS__)
 #define LIBASSERT_MAP_LIST_NEXT1(test, next) LIBASSERT_MAP_NEXT0(test, LIBASSERT_MAP_COMMA next, 0)
 #define LIBASSERT_MAP_LIST_NEXT(test, next)  LIBASSERT_MAP_LIST_NEXT1(LIBASSERT_MAP_GET_END test, next)
 #define LIBASSERT_MAP_LIST0(f, x, peek, ...) \
                                   f(x) LIBASSERT_MAP_LIST_NEXT(peek, LIBASSERT_MAP_LIST1)(f, peek, __VA_ARGS__)
 #define LIBASSERT_MAP_LIST1(f, x, peek, ...) \
                                   f(x) LIBASSERT_MAP_LIST_NEXT(peek, LIBASSERT_MAP_LIST0)(f, peek, __VA_ARGS__)
 #define LIBASSERT_MAP(f, ...) LIBASSERT_EVAL(LIBASSERT_MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))
#else
 // https://stackoverflow.com/a/29474124/15675011
 #define LIBASSERT_PLUS_TEXT_(x,y) x ## y
 #define LIBASSERT_PLUS_TEXT(x, y) LIBASSERT_PLUS_TEXT_(x, y)
 #define LIBASSERT_ARG_1(_1, ...) _1
 #define LIBASSERT_ARG_2(_1, _2, ...) _2
 #define LIBASSERT_ARG_3(_1, _2, _3, ...) _3
 #define LIBASSERT_ARG_40( _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, \
                 _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, \
                 _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, \
                 _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, \
                 ...) _39
 #define LIBASSERT_OTHER_1(_1, ...) __VA_ARGS__
 #define LIBASSERT_OTHER_3(_1, _2, _3, ...) __VA_ARGS__
 #define LIBASSERT_EVAL0(...) __VA_ARGS__
 #define LIBASSERT_EVAL1(...) LIBASSERT_EVAL0(LIBASSERT_EVAL0(LIBASSERT_EVAL0(__VA_ARGS__)))
 #define LIBASSERT_EVAL2(...) LIBASSERT_EVAL1(LIBASSERT_EVAL1(LIBASSERT_EVAL1(__VA_ARGS__)))
 #define LIBASSERT_EVAL3(...) LIBASSERT_EVAL2(LIBASSERT_EVAL2(LIBASSERT_EVAL2(__VA_ARGS__)))
 #define LIBASSERT_EVAL4(...) LIBASSERT_EVAL3(LIBASSERT_EVAL3(LIBASSERT_EVAL3(__VA_ARGS__)))
 #define LIBASSERT_EVAL(...) LIBASSERT_EVAL4(LIBASSERT_EVAL4(LIBASSERT_EVAL4(__VA_ARGS__)))
 #define LIBASSERT_EXPAND(x) x
 #define LIBASSERT_MAP_SWITCH(...)\
     LIBASSERT_EXPAND(LIBASSERT_ARG_40(__VA_ARGS__, 2, 2, 2, 2, 2, 2, 2, 2, 2,\
             2, 2, 2, 2, 2, 2, 2, 2, 2, 2,\
             2, 2, 2, 2, 2, 2, 2, 2, 2,\
             2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0))
 #define LIBASSERT_MAP_A(...) LIBASSERT_PLUS_TEXT(LIBASSERT_MAP_NEXT_, \
                                            LIBASSERT_MAP_SWITCH(0, __VA_ARGS__)) (LIBASSERT_MAP_B, __VA_ARGS__)
 #define LIBASSERT_MAP_B(...) LIBASSERT_PLUS_TEXT(LIBASSERT_MAP_NEXT_, \
                                            LIBASSERT_MAP_SWITCH(0, __VA_ARGS__)) (LIBASSERT_MAP_A, __VA_ARGS__)
 #define LIBASSERT_MAP_CALL(fn, Value) LIBASSERT_EXPAND(fn(Value))
 #define LIBASSERT_MAP_OUT
 #define LIBASSERT_MAP_NEXT_2(...)\
     LIBASSERT_MAP_CALL(LIBASSERT_EXPAND(LIBASSERT_ARG_2(__VA_ARGS__)), \
     LIBASSERT_EXPAND(LIBASSERT_ARG_3(__VA_ARGS__))) \
     LIBASSERT_EXPAND(LIBASSERT_ARG_1(__VA_ARGS__)) \
     LIBASSERT_MAP_OUT \
     (LIBASSERT_EXPAND(LIBASSERT_ARG_2(__VA_ARGS__)), LIBASSERT_EXPAND(LIBASSERT_OTHER_3(__VA_ARGS__)))
 #define LIBASSERT_MAP_NEXT_0(...)
 #define LIBASSERT_MAP(...)    LIBASSERT_EVAL(LIBASSERT_MAP_A(__VA_ARGS__))
#endif

#define LIBASSERT_STRINGIFY(x) #x,
#define LIBASSERT_COMMA ,

// Church boolean
#define LIBASSERT_IF(b) LIBASSERT_IF_##b
#define LIBASSERT_IF_true(t,...) t
#define LIBASSERT_IF_false(t,f,...) f

#if LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC
 #if LIBASSERT_IS_GCC
  #define LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_GCC \
     _Pragma("GCC diagnostic ignored \"-Wparentheses\"") \
     _Pragma("GCC diagnostic ignored \"-Wuseless-cast\"") // #49
  #define LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_CLANG
  #define LIBASSERT_WARNING_PRAGMA_PUSH_GCC _Pragma("GCC diagnostic push")
  #define LIBASSERT_WARNING_PRAGMA_POP_GCC _Pragma("GCC diagnostic pop")
  #define LIBASSERT_WARNING_PRAGMA_PUSH_CLANG
  #define LIBASSERT_WARNING_PRAGMA_POP_CLANG
 #else
  #define LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_CLANG \
     _Pragma("GCC diagnostic ignored \"-Wparentheses\"") \
     _Pragma("GCC diagnostic ignored \"-Woverloaded-shift-op-parentheses\"")
  #define LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_GCC
  #define LIBASSERT_WARNING_PRAGMA_PUSH_GCC
  #define LIBASSERT_WARNING_PRAGMA_POP_GCC
  #define LIBASSERT_WARNING_PRAGMA_PUSH_CLANG _Pragma("GCC diagnostic push")
  #define LIBASSERT_WARNING_PRAGMA_POP_CLANG _Pragma("GCC diagnostic pop")
 #endif
#else
 #define LIBASSERT_WARNING_PRAGMA_PUSH_CLANG
 #define LIBASSERT_WARNING_PRAGMA_POP_CLANG
 #define LIBASSERT_WARNING_PRAGMA_PUSH_GCC
 #define LIBASSERT_WARNING_PRAGMA_POP_GCC
 #define LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_GCC
 #define LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_CLANG
#endif

namespace libassert {
    inline void ERROR_ASSERTION_FAILURE_IN_CONSTEXPR_CONTEXT() {
        // This non-constexpr method is called from an assertion in a constexpr context if a failure occurs. It is
        // intentionally a no-op.
    }
}

// __PRETTY_FUNCTION__ used because __builtin_FUNCTION() used in source_location (like __FUNCTION__) is just the method
// name, not signature
// The arg strings at the very least must be static constexpr. Unfortunately static constexpr variables are not allowed
// in constexpr functions pre-C++23.
// TODO: Try to do a hybrid in C++20 with std::is_constant_evaluated?
#if defined(__cpp_constexpr) && __cpp_constexpr >= 202211L
// Can just use static constexpr everywhere
#define LIBASSERT_STATIC_DATA(name, type, expr_str, ...) \
    /* extra string here because of extra comma from map, also serves as terminator */ \
    /* LIBASSERT_STRINGIFY LIBASSERT_VA_ARGS because msvc */ \
    /* Trailing return type here to work around a gcc <= 9.2 bug */ \
    /* Oddly only affecting builds under -DNDEBUG https://godbolt.org/z/5Treozc4q */ \
    using libassert_params_t = libassert::detail::assert_static_parameters; \
    /* NOLINTNEXTLINE(*-avoid-c-arrays) */ \
    static constexpr std::string_view libassert_arg_strings[] = { \
        LIBASSERT_MAP(LIBASSERT_STRINGIFY LIBASSERT_VA_ARGS(__VA_ARGS__)) "" \
    }; \
    static constexpr libassert_params_t _libassert_params = { \
        name LIBASSERT_COMMA \
        type LIBASSERT_COMMA \
        expr_str LIBASSERT_COMMA \
        {} LIBASSERT_COMMA \
        {libassert_arg_strings, sizeof(libassert_arg_strings) / sizeof(std::string_view)} LIBASSERT_COMMA \
    }; \
    const libassert_params_t* libassert_params = &_libassert_params;
#else
#define LIBASSERT_STATIC_DATA(name, type, expr_str, ...) \
    using libassert_params_t = libassert::detail::assert_static_parameters; \
    /* NOLINTNEXTLINE(*-avoid-c-arrays) */ \
    const libassert_params_t* libassert_params = []() -> const libassert_params_t* { \
        static constexpr std::string_view libassert_arg_strings[] = { \
            LIBASSERT_MAP(LIBASSERT_STRINGIFY LIBASSERT_VA_ARGS(__VA_ARGS__)) "" \
        }; \
        static constexpr libassert_params_t _libassert_params = { \
            name LIBASSERT_COMMA \
            type LIBASSERT_COMMA \
            expr_str LIBASSERT_COMMA \
            {} LIBASSERT_COMMA \
            {libassert_arg_strings, sizeof(libassert_arg_strings) / sizeof(std::string_view)} LIBASSERT_COMMA \
        }; \
        return &_libassert_params; \
    }();
#endif

// Note about statement expressions: These are needed for two reasons. The first is putting the arg string array and
// source location structure in .rodata rather than on the stack, the second is a _Pragma for warnings which isn't
// allowed in the middle of an expression by GCC. The semantics are similar to a function return:
// Given M m; in parent scope, ({ m; }) is an rvalue M&& rather than an lvalue
// ({ M m; m; }) doesn't move, it copies
// ({ M{}; }) does move
// Of relevance to this: in foo(__extension__ ({ M{1} + M{1}; })); the lifetimes of the M{1} objects end during the
// statement expression but the lifetime of the returned object is extend to the end of the full foo() expression.
// A wrapper struct is used here to return an lvalue reference from a gcc statement expression.
// Note: There is a current issue with tarnaries: auto x = assert(b ? y : y); must copy y. This can be fixed with
// lambdas but that's potentially very expensive compile-time wise. Need to investigate further.
// Note: libassert::detail::expression_decomposer(libassert::detail::expression_decomposer{} << expr) done for ternary
#if LIBASSERT_IS_MSVC
 #define LIBASSERT_INVOKE_VAL_PRETTY_FUNCTION_ARG ,libassert::detail::pretty_function_name_wrapper{libassert_msvc_pfunc}
#else
 #define LIBASSERT_INVOKE_VAL_PRETTY_FUNCTION_ARG ,libassert::detail::pretty_function_name_wrapper{LIBASSERT_PFUNC}
#endif
#define LIBASSERT_PRETTY_FUNCTION_ARG ,libassert::detail::pretty_function_name_wrapper{LIBASSERT_PFUNC}
#if LIBASSERT_IS_CLANG // -Wall in clang
 #define LIBASSERT_IGNORE_UNUSED_VALUE _Pragma("GCC diagnostic ignored \"-Wunused-value\"")
#else
 #define LIBASSERT_IGNORE_UNUSED_VALUE
#endif

#define LIBASSERT_INVOKE(expr, name, type, failaction, ...) \
    /* must push/pop out here due to nasty clang bug https://github.com/llvm/llvm-project/issues/63897 */ \
    /* must do awful stuff to workaround differences in where gcc and clang allow these directives to go */ \
    do { \
        LIBASSERT_WARNING_PRAGMA_PUSH_CLANG \
        LIBASSERT_IGNORE_UNUSED_VALUE \
        LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_CLANG \
        LIBASSERT_WARNING_PRAGMA_PUSH_GCC \
        LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_GCC \
        auto libassert_decomposer = libassert::detail::expression_decomposer( \
            libassert::detail::expression_decomposer{} << expr \
        ); \
        LIBASSERT_WARNING_PRAGMA_POP_GCC \
        if(LIBASSERT_STRONG_EXPECT(!static_cast<bool>(libassert_decomposer.get_value()), 0)) { \
            libassert::ERROR_ASSERTION_FAILURE_IN_CONSTEXPR_CONTEXT(); \
            failaction \
            LIBASSERT_STATIC_DATA(name, libassert::assert_type::type, #expr, __VA_ARGS__) \
            if constexpr(sizeof libassert_decomposer > 32) { \
                libassert::detail::process_assert_fail( \
                    libassert_decomposer, \
                    libassert_params \
                    LIBASSERT_VA_ARGS(__VA_ARGS__) LIBASSERT_PRETTY_FUNCTION_ARG \
                ); \
            } else { \
                /* std::move it to assert_fail_m, will be moved back to r */ \
                libassert::detail::process_assert_fail_n( \
                    std::move(libassert_decomposer), \
                    libassert_params \
                    LIBASSERT_VA_ARGS(__VA_ARGS__) LIBASSERT_PRETTY_FUNCTION_ARG \
                ); \
            } \
        } \
        LIBASSERT_WARNING_PRAGMA_POP_CLANG \
    } while(false) \

#define LIBASSERT_INVOKE_PANIC(name, type, ...) \
    do { \
        libassert::ERROR_ASSERTION_FAILURE_IN_CONSTEXPR_CONTEXT(); \
        LIBASSERT_STATIC_DATA(name, libassert::assert_type::type, "", __VA_ARGS__) \
        libassert::detail::process_panic( \
            libassert_params \
            LIBASSERT_VA_ARGS(__VA_ARGS__) LIBASSERT_PRETTY_FUNCTION_ARG \
        ); \
    } while(false) \

// Workaround for gcc bug 105734 / libassert bug #24
#define LIBASSERT_DESTROY_DECOMPOSER libassert_decomposer.~expression_decomposer() /* NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move) */
#if LIBASSERT_IS_GCC
 #if __GNUC__ == 12 && __GNUC_MINOR__ == 1
  namespace libassert::detail {
      template<typename T> constexpr void destroy(T& t) {
          t.~T();
      }
  }
  #undef LIBASSERT_DESTROY_DECOMPOSER
  #define LIBASSERT_DESTROY_DECOMPOSER libassert::detail::destroy(libassert_decomposer)
 #endif
#endif
#if LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC
 // Extra set of parentheses here because clang treats __extension__ as a low-precedence unary operator which interferes
 // with decltype(auto) in an expression like decltype(auto) x = __extension__ ({...}).y;
 #define LIBASSERT_STMTEXPR(B, R) (__extension__ ({ B R }))
 #define LIBASSERT_STATIC_CAST_TO_BOOL(x) static_cast<bool>(x)
#else
 #define LIBASSERT_STMTEXPR(B, R) [&](const char* libassert_msvc_pfunc) { B return R }(LIBASSERT_PFUNC)
 // Workaround for msvc bug
 #define LIBASSERT_STATIC_CAST_TO_BOOL(x) libassert::detail::static_cast_to_bool(x)
 namespace libassert::detail {
     template<typename T> constexpr bool static_cast_to_bool(T&& t) {
         return static_cast<bool>(t);
     }
 }
#endif
#define LIBASSERT_INVOKE_VAL(expr, doreturn, check_expression, name, type, failaction, ...) \
    /* must push/pop out here due to nasty clang bug https://github.com/llvm/llvm-project/issues/63897 */ \
    /* must do awful stuff to workaround differences in where gcc and clang allow these directives to go */ \
    LIBASSERT_WARNING_PRAGMA_PUSH_CLANG \
    LIBASSERT_IGNORE_UNUSED_VALUE \
    LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_CLANG \
    LIBASSERT_STMTEXPR( \
        LIBASSERT_WARNING_PRAGMA_PUSH_GCC \
        LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_GCC \
        auto libassert_decomposer = libassert::detail::expression_decomposer( \
            libassert::detail::expression_decomposer{} << expr \
        ); \
        LIBASSERT_WARNING_PRAGMA_POP_GCC \
        decltype(auto) libassert_value = libassert_decomposer.get_value(); \
        constexpr bool libassert_ret_lhs = libassert_decomposer.ret_lhs(); \
        if constexpr(check_expression) { \
            /* For *some* godforsaken reason static_cast<bool> causes an ICE in MSVC here. Something very specific */ \
            /* about casting a decltype(auto) value inside a lambda. Workaround is to put it in a wrapper. */ \
            /* https://godbolt.org/z/Kq8Wb6q5j https://godbolt.org/z/nMnqnsMYx */ \
            if(LIBASSERT_STRONG_EXPECT(!LIBASSERT_STATIC_CAST_TO_BOOL(libassert_value), 0)) { \
                libassert::ERROR_ASSERTION_FAILURE_IN_CONSTEXPR_CONTEXT(); \
                failaction \
                LIBASSERT_STATIC_DATA(name, libassert::assert_type::type, #expr, __VA_ARGS__) \
                if constexpr(sizeof libassert_decomposer > 32) { \
                    libassert::detail::process_assert_fail( \
                        libassert_decomposer, \
                        libassert_params \
                        LIBASSERT_VA_ARGS(__VA_ARGS__) LIBASSERT_INVOKE_VAL_PRETTY_FUNCTION_ARG \
                    ); \
                } else { \
                    /* std::move it to assert_fail_m, will be moved back to r */ \
                    auto libassert_r = libassert::detail::process_assert_fail_m( \
                        std::move(libassert_decomposer), \
                        libassert_params \
                        LIBASSERT_VA_ARGS(__VA_ARGS__) LIBASSERT_INVOKE_VAL_PRETTY_FUNCTION_ARG \
                    ); \
                    /* can't move-assign back to decomposer if it holds reference members */ \
                    LIBASSERT_DESTROY_DECOMPOSER; \
                    new (&libassert_decomposer) libassert::detail::expression_decomposer(std::move(libassert_r)); \
                } \
            } \
        }, \
        /* Note: std::launder needed in 17 in case of placement new / move shenanigans above */ \
        /* https://timsong-cpp.github.io/cppwp/n4659/basic.life#8.3 */ \
        /* Note: Somewhat relying on this call being inlined so inefficiency is eliminated */ \
        libassert::detail::get_expression_return_value< \
            doreturn LIBASSERT_COMMA \
            libassert_ret_lhs LIBASSERT_COMMA \
            std::is_lvalue_reference_v<decltype(libassert_value)> \
        >(libassert_value, *std::launder(&libassert_decomposer)); \
    ) LIBASSERT_IF(doreturn)(.value,) \
    LIBASSERT_WARNING_PRAGMA_POP_CLANG

#ifdef NDEBUG
 #define LIBASSERT_ASSUME_ACTION LIBASSERT_UNREACHABLE_CALL;
#else
 #define LIBASSERT_ASSUME_ACTION
#endif

// assertion macros

// Debug assert
#ifndef NDEBUG
 #define LIBASSERT_DEBUG_ASSERT(expr, ...) LIBASSERT_INVOKE(expr, "DEBUG_ASSERT", debug_assertion, , __VA_ARGS__)
#else
 #define LIBASSERT_DEBUG_ASSERT(expr, ...) (void)0
#endif

// Assert
#define LIBASSERT_ASSERT(expr, ...) LIBASSERT_INVOKE(expr, "ASSERT", assertion, , __VA_ARGS__)
// lowercase version intentionally done outside of the include guard here

// Assume
#define LIBASSERT_ASSUME(expr, ...) LIBASSERT_INVOKE(expr, "ASSUME", assumption, LIBASSERT_ASSUME_ACTION, __VA_ARGS__)

// Panic
#define LIBASSERT_PANIC(...) LIBASSERT_INVOKE_PANIC("PANIC", panic, __VA_ARGS__)

// Unreachable
#ifndef NDEBUG
 #define LIBASSERT_UNREACHABLE(...) LIBASSERT_INVOKE_PANIC("UNREACHABLE", unreachable, __VA_ARGS__)
#else
 #define LIBASSERT_UNREACHABLE(...) LIBASSERT_UNREACHABLE_CALL
#endif

// value variants

#ifndef NDEBUG
 #define LIBASSERT_DEBUG_ASSERT_VAL(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, true, "DEBUG_ASSERT_VAL", debug_assertion, , __VA_ARGS__)
#else
 #define LIBASSERT_DEBUG_ASSERT_VAL(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, false, "DEBUG_ASSERT_VAL", debug_assertion, , __VA_ARGS__)
#endif

#define LIBASSERT_ASSUME_VAL(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, true, "ASSUME_VAL", assumption, LIBASSERT_ASSUME_ACTION, __VA_ARGS__)

#define LIBASSERT_ASSERT_VAL(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, true, "ASSERT_VAL", assertion, , __VA_ARGS__)

// non-prefixed versions

#ifndef LIBASSERT_PREFIX_ASSERTIONS
 #if LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC || (defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL == 0)
  #define DEBUG_ASSERT(...) LIBASSERT_DEBUG_ASSERT(__VA_ARGS__)
  #define ASSERT(...) LIBASSERT_ASSERT(__VA_ARGS__)
  #define ASSUME(...) LIBASSERT_ASSUME(__VA_ARGS__)
  #define PANIC(...) LIBASSERT_PANIC(__VA_ARGS__)
  #define UNREACHABLE(...) LIBASSERT_UNREACHABLE(__VA_ARGS__)
  #define DEBUG_ASSERT_VAL(...) LIBASSERT_DEBUG_ASSERT_VAL(__VA_ARGS__)
  #define ASSUME_VAL(...) LIBASSERT_ASSUME_VAL(__VA_ARGS__)
  #define ASSERT_VAL(...) LIBASSERT_ASSERT_VAL(__VA_ARGS__)
 #else
  // because of course msvc
  #define DEBUG_ASSERT LIBASSERT_DEBUG_ASSERT
  #define ASSERT LIBASSERT_ASSERT
  #define ASSUME LIBASSERT_ASSUME
  #define PANIC LIBASSERT_PANIC
  #define UNREACHABLE LIBASSERT_UNREACHABLE
  #define DEBUG_ASSERT_VAL LIBASSERT_DEBUG_ASSERT_VAL
  #define ASSUME_VAL LIBASSERT_ASSUME_VAL
  #define ASSERT_VAL LIBASSERT_ASSERT_VAL
 #endif
#endif

// Lowercase variants

#ifdef LIBASSERT_LOWERCASE
 #ifndef NDEBUG
  #define debug_assert(expr, ...) LIBASSERT_INVOKE(expr, "debug_assert", debug_assertion, , __VA_ARGS__)
 #else
  #define debug_assert(expr, ...) (void)0
 #endif
#endif

#ifdef LIBASSERT_LOWERCASE
 #ifndef NDEBUG
  #define debug_assert_val(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, true, "debug_assert_val", debug_assertion, , __VA_ARGS__)
 #else
  #define debug_assert_val(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, false, "debug_assert_val", debug_assertion, , __VA_ARGS__)
 #endif
#endif

#ifdef LIBASSERT_LOWERCASE
 #define assert_val(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, true, "assert_val", assertion, , __VA_ARGS__)
#endif

#endif

// Intentionally done outside the include guard. Libc++ leaks `assert` (among other things), so the include for
// assert.hpp should go after other includes when using -DLIBASSERT_LOWERCASE.
#ifdef LIBASSERT_LOWERCASE
 #ifdef assert
  #undef assert
 #endif
 #ifndef NDEBUG
  #define assert(expr, ...) LIBASSERT_INVOKE(expr, "assert", assertion, , __VA_ARGS__)
 #else
  #define assert(expr, ...) LIBASSERT_INVOKE(expr, "assert", assertion, , __VA_ARGS__)
 #endif
#endif
