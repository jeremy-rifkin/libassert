#ifndef ASSERT_HPP
#define ASSERT_HPP

// Copyright (c) 2021-2024 Jeremy Rifkin under the MIT license
// https://github.com/jeremy-rifkin/libassert

#include <cerrno>
#include <cstddef>
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
#  define LIBASSERT_EXPORT
#  define LIBASSERT_NO_EXPORT
#else
#  ifndef LIBASSERT_EXPORT
#    ifdef libassert_lib_EXPORTS
        /* We are building this library */
#      define LIBASSERT_EXPORT LIBASSERT_EXPORT_ATTR
#    else
        /* We are using this library */
#      define LIBASSERT_EXPORT LIBASSERT_IMPORT_ATTR
#    endif
#  endif
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
 #error "Unsupported compiler"
#endif

#if LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC
 #define LIBASSERT_PFUNC __extension__ __PRETTY_FUNCTION__
 #define LIBASSERT_ATTR_COLD     [[gnu::cold]]
 #define LIBASSERT_ATTR_NOINLINE [[gnu::noinline]]
 #define LIBASSERT_UNREACHABLE __builtin_unreachable()
#else
 #define LIBASSERT_PFUNC __FUNCSIG__
 #define LIBASSERT_ATTR_COLD
 #define LIBASSERT_ATTR_NOINLINE __declspec(noinline)
 #define LIBASSERT_UNREACHABLE __assume(false)
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

// =====================================================================================================================
// || Core utilities                                                                                                  ||
// =====================================================================================================================

namespace libassert::detail {
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

    // bootstrap with primitive implementations
    LIBASSERT_EXPORT void primitive_assert_impl(
        bool condition,
        bool verify,
        const char* expression,
        const char* signature,
        source_location location,
        const char* message = nullptr
    );

    #ifndef NDEBUG
     #define LIBASSERT_PRIMITIVE_ASSERT(c, ...) \
        libassert::detail::primitive_assert_impl(c, false, #c, LIBASSERT_PFUNC, {}, ##__VA_ARGS__)
    #else
     #define LIBASSERT_PRIMITIVE_ASSERT(c, ...) LIBASSERT_PHONY_USE(c)
    #endif
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

        template<typename T>
        constexpr inline bool is_container_like = is_deref<T>::value
                                                    || adl::is_begin_deref<T>::value
                                                    || is_tuple_like<T>::value;

        template<typename T, typename = void> class has_stream_overload : public std::false_type {};
        template<typename T>
        class has_stream_overload<
            T,
            std::void_t<decltype(std::declval<std::ostringstream>() << std::declval<T>())>
        > : public std::true_type {};

        template<typename T, typename = void> class has_stringifier : public std::false_type {};
        template<typename T>
        class has_stringifier<
            T,
            std::void_t<decltype(stringifier<strip<T>>{}.stringify(std::declval<T>()))>
        > : public std::true_type {};

        //
        // Basic types
        //
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(const std::string&);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(const std::string_view&);
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

        template<
            typename T,
            typename std::enable_if_t<
                has_stream_overload<T>::value
                    && !has_stringifier<T>::value
                    && !is_string_type<T>
                    && !std::is_enum_v<strip<T>>
                    && !std::is_pointer_v<strip<typename std::decay_t<T>>>,
                int
            > = 0
        >
        LIBASSERT_ATTR_COLD [[nodiscard]] std::string stringify(const T& t) {
            // clang-tidy bug here
            // NOLINTNEXTLINE(misc-const-correctness)
            std::ostringstream oss;
            oss<<t;
            return std::move(oss).str();
        }

        template<
            typename T,
            typename std::enable_if_t<
                has_stringifier<T>::value,
                int
            > = 0
        >
        LIBASSERT_ATTR_COLD [[nodiscard]] std::string stringify(const T& t) {
            return stringifier<strip<T>>{}.stringify(t);
        }

        #ifdef LIBASSERT_USE_MAGIC_ENUM
        template<typename T, typename std::enable_if_t<std::is_enum_v<strip<T>>, int> = 0>
        LIBASSERT_ATTR_COLD [[nodiscard]] std::string stringify(const T& t) {
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
        LIBASSERT_ATTR_COLD [[nodiscard]] std::string stringify(const T& t) {
            return bstringf(
                "enum %s: %s",
                prettify_type(std::string(type_name<T>())).c_str(),
                stringify(static_cast<typename std::underlying_type_t<T>>(t)).c_str()
            );
        }
        #endif

        // pointers
        template<
            typename T,
            typename std::enable_if_t<
                (
                    std::is_pointer_v<strip<typename std::decay_t<T>>> && (
                        isa<typename std::remove_pointer_t<typename std::decay_t<T>>, char>
                            || !adl::is_container<T>::value
                    )
                ) || std::is_function_v<strip<T>>,
                int
            > = 0
        >
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify(const T& t) {
            if constexpr(isa<typename std::remove_pointer_t<typename std::decay_t<T>>, char>) { // strings
                const void* v = t; // circumvent -Wnonnull-compare
                if(v != nullptr) {
                    return stringify(std::string_view(t)); // not printing type for now, TODO: reconsider?
                }
            }
            return stringify_pointer_value(reinterpret_cast<const void*>(t));
        }

        //
        // Compositions of other types
        //
        #ifdef __cpp_lib_expected
        template<typename E>
        [[nodiscard]] std::string stringify(const std::unexpected<E>& x) {
            return "unexpected " + stringify(x.error());
        }

        template<typename T, typename E>
        [[nodiscard]] std::string stringify(const std::expected<T, E>& x) {
            if(x.has_value()) {
                if constexpr(std::is_void_v<T>) {
                    return "expected void";
                } else {
                    return "expected " + stringify(*x);
                }
            } else {
                return "unexpected " + stringify(x.error());
            }
        }
        #endif

        // deferrable wrappers, these need to be implemented after stringify(container)
        template<typename T>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify_optional(const T&);

        template<typename T>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify_smart_ptr(const T&);

        template<typename T>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify(const std::optional<T>& t) {
            return stringify_optional(t);
        }

        template<typename T>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify(const std::unique_ptr<T>& t) {
            return stringify_smart_ptr(t);
        }

        template<typename T, size_t... I>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify_tuple_like(const T& t, std::index_sequence<I...>);

        template<
            typename T,
            typename std::enable_if_t<is_tuple_like<T>::value && !adl::is_container<T>::value, int> = 0
        >
        LIBASSERT_ATTR_COLD [[nodiscard]] std::string stringify(const T& t) {
            return stringify_tuple_like(t, std::make_index_sequence<std::tuple_size<T>::value - 1>{});
        }

        template<typename T, typename = void>
        class has_basic_stringify : public std::false_type {};
        template<typename T>
        class has_basic_stringify<
            T,
            std::void_t<decltype(stringify(std::declval<T>()))>
        > : public std::true_type {};

        template<typename T, typename = void>
        class has_value_type : public std::false_type {};
        template<typename T>
        class has_value_type<
            T,
            std::void_t<typename T::value_type>
        > : public std::true_type {};

        template<typename T> constexpr bool stringifiable_container();

        template<typename T> inline constexpr bool stringifiable = has_basic_stringify<T>::value || stringifiable_container<T>();

        template<typename T> constexpr bool stringifiable_container() {
            if constexpr(has_value_type<T>::value) {
                return stringifiable<typename T::value_type>;
            } else if constexpr(std::is_array_v<typename std::remove_reference_t<T>>) { // C arrays
                return stringifiable<decltype(std::declval<T>()[0])>;
            } else {
                return false;
            }
        }

        // containers
        template<
            typename T,
            typename std::enable_if_t<
                adl::is_container<T>::value && stringifiable_container<T>() && !is_c_string<T>,
                int
            > = 0
        >
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify(const T& container) {
            using std::begin, std::end; // ADL
            std::string str = "[";
            const auto begin_it = begin(container);
            for(auto it = begin_it; it != end(container); it++) {
                if(it != begin_it) {
                    str += ", ";
                }
                str += stringify(*it);
            }
            str += "]";
            return str;
        }

        // deferred impls
        // I'm going to assume at least one index because is_tuple_like requires index 0 to exist
        template<typename T, size_t... I>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify_tuple_like(const T& t, std::index_sequence<I...>) {
            return "[" + (stringify(std::get<0>(t)) + ... + (", " + stringify(std::get<I + 1>(t)))) + "]";
        }

        template<typename T>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify_optional(const T& t) {
            if(t) {
                return stringify(t.value());
            } else {
                return "nullopt";
            }
        }

        template<typename T>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify_smart_ptr(const T& t) {
            if(t) {
                return stringify(*t);
            } else {
                return "nullptr";
            }
        }
    }

    template<typename T, typename = void>
    class can_stringify : public std::false_type {};
    template<typename T>
    class can_stringify<
        T,
        std::void_t<decltype(stringification::stringify(std::declval<T>()))>
    > : public std::true_type {};

    // Top-level stringify utility
    template<typename T, typename std::enable_if_t<can_stringify<T>::value, int> = 0>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string generate_stringification(const T& v) {
        if constexpr(stringification::is_container_like<T> && !is_string_type<T>) {
            return prettify_type(std::string(type_name<T>())) + ": " + stringification::stringify(v);
        } else if constexpr(std::is_pointer_v<T> && !is_string_type<T>) {
            return prettify_type(std::string(type_name<T>())) + ": " + stringification::stringify(v);
        } else {
            return stringification::stringify(v);
        }
    }

    template<typename T, typename std::enable_if_t<!can_stringify<T>::value, int> = 0>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string generate_stringification(const T&) {
        return bstringf("<instance of %s>", prettify_type(std::string(type_name<T>())).c_str());
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
        LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(eq,   ==, cmp_equal);
        LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(neq,  !=, cmp_not_equal);
        LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(gt,    >, cmp_greater);
        LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(lt,    <, cmp_less);
        LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(gteq, >=, cmp_greater_equal);
        LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(lteq, <=, cmp_less_equal);
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
        compl expression_decomposer() = default;
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
                return (((((((((((((((((((a)))))))))))))))))));
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

    // generates a stack trace, formats to the given width
    [[nodiscard]] LIBASSERT_EXPORT std::string stacktrace(int width);

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
    // template<typename T>
    // [[nodiscard]] std::string stringify(const T& t) {
    //     return detail::generate_stringification(t);
    // }
    using detail::generate_stringification; // TODO

    // configures whether the default assertion handler prints in color or not to tty devices (default true)
    LIBASSERT_EXPORT void set_color_output(bool);

    // NOTE: string view underlying data should have static storage duration, or otherwise live as long as the scheme
    // is in use
    struct color_scheme {
        std::string_view string;
        std::string_view escape;
        std::string_view keyword;
        std::string_view named_literal;
        std::string_view number;
        std::string_view operator_token;
        std::string_view call_identifier;
        std::string_view scope_resolution_identifier;
        std::string_view identifier;
        std::string_view accent;
        std::string_view reset;
    };

    LIBASSERT_EXPORT extern color_scheme ansi_basic;
    LIBASSERT_EXPORT extern color_scheme ansi_rgb;
    LIBASSERT_EXPORT extern color_scheme blank;

    LIBASSERT_EXPORT void set_color_scheme(color_scheme);

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

    // enum class path_mode {
    //     // full path is used
    //     full,
    //     // only enough folders needed to disambiguate are provided
    //     disambiguated, // TODO: Maybe just a bad idea
    //     // only the file name is used
    //     basename,
    // };
    // ASSERT_EXPORT void set_path_mode(path_mode mode);

    enum class assert_type {
        debug_assertion,
        assertion,
        assumption,
        verification
    };

    class assertion_printer;

    LIBASSERT_EXPORT void set_failure_handler(void (*handler)(assert_type, const assertion_printer&));
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
        const char* a_str,
        const char* b_str,
        std::string_view op,
        bool integer_character
    );
    LIBASSERT_EXPORT void restore_literal_format(literal_format);
    // does the current literal format config have multiple formats
    LIBASSERT_EXPORT bool has_multiple_formats();

    [[nodiscard]] LIBASSERT_EXPORT std::pair<std::string, std::string> decompose_expression(
        const std::string& expression,
        std::string_view target_op
    );

    /*
     * System wrappers
     */

    [[nodiscard]] LIBASSERT_EXPORT std::string strerror_wrapper(int err); // stupid C stuff, stupid microsoft stuff

    /*
     * Stacktrace implementation
     */

    struct LIBASSERT_EXPORT opaque_trace {
        void* trace;
        ~opaque_trace();
        opaque_trace(void* t) : trace(t) {}
        opaque_trace(const opaque_trace&) = delete;
        opaque_trace(opaque_trace&&) = delete;
        opaque_trace& operator=(const opaque_trace&) = delete;
        opaque_trace& operator=(opaque_trace&&) = delete;
    };

    LIBASSERT_EXPORT opaque_trace get_stacktrace_opaque();

    /*
     * assert diagnostics generation
     */

    struct LIBASSERT_EXPORT binary_diagnostics_descriptor {
        std::string lstring;
        std::string rstring;
        std::string a_str;
        std::string b_str;
        bool multiple_formats;
        bool present = false;
        binary_diagnostics_descriptor(); // = default; in the .cpp
        binary_diagnostics_descriptor(
            std::string&& lstrings,
            std::string&& rstrings,
            std::string a_str,
            std::string b_str,
            bool multiple_formats
        );
        compl binary_diagnostics_descriptor(); // = default; in the .cpp
        binary_diagnostics_descriptor(const binary_diagnostics_descriptor&) = delete;
        binary_diagnostics_descriptor(binary_diagnostics_descriptor&&) noexcept; // = default; in the .cpp
        binary_diagnostics_descriptor& operator=(const binary_diagnostics_descriptor&) = delete;
        binary_diagnostics_descriptor&
        operator=(binary_diagnostics_descriptor&&) noexcept(LIBASSERT_GCC_ISNT_STUPID); // = default; in the .cpp
    };

    template<typename A, typename B>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    binary_diagnostics_descriptor generate_binary_diagnostic(
        const A& a,
        const B& b,
        const char* a_str, // TODO: Why is op a string view but these are not
        const char* b_str,
        std::string_view op
    ) {
        constexpr bool either_is_character = isa<A, char> || isa<B, char>;
        constexpr bool either_is_arithmetic = is_arith_not_bool_char<A> || is_arith_not_bool_char<B>;
        literal_format previous_format = set_literal_format(
            a_str,
            b_str,
            op,
            either_is_character && either_is_arithmetic
        );
        binary_diagnostics_descriptor descriptor(
            generate_stringification(a),
            generate_stringification(b),
            a_str,
            b_str,
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

    struct LIBASSERT_EXPORT extra_diagnostics {
        std::string message;
        std::vector<std::pair<std::string, std::string>> entries;
        const char* pretty_function = "<error>";
        extra_diagnostics();
        compl extra_diagnostics();
        extra_diagnostics(const extra_diagnostics&) = delete;
        extra_diagnostics(extra_diagnostics&&) noexcept; // = default; in the .cpp
        extra_diagnostics& operator=(const extra_diagnostics&) = delete;
        extra_diagnostics& operator=(extra_diagnostics&&) = delete;
    };

    struct pretty_function_name_wrapper {
        const char* pretty_function;
    };

    inline void process_arg(
        extra_diagnostics& entry,
        size_t,
        const char* const* const,
        const pretty_function_name_wrapper& t
    ) {
        entry.pretty_function = t.pretty_function;
    }

    template<typename T>
    LIBASSERT_ATTR_COLD
    // TODO
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void process_arg(extra_diagnostics& entry, size_t i, const char* const* const args_strings, const T& t) {
        // three cases to handle: assert message, errno, and regular diagnostics
        #if LIBASSERT_IS_MSVC
         #pragma warning(push)
         #pragma warning(disable: 4127) // MSVC thinks constexpr should be used here. It should not.
        #endif
        // TODO: Maybe just unconditionally capture errno and handle later...
        if(isa<T, strip<decltype(errno)>> && args_strings[i] == errno_expansion) {
        #if LIBASSERT_IS_MSVC
         #pragma warning(pop)
        #endif
            // this is redundant and useless but the body for errno handling needs to be in an
            // if constexpr wrapper
            if constexpr(isa<T, strip<decltype(errno)>>) {
            // errno will expand to something hideous like (*__errno_location()),
            // may as well replace it with "errno"
            entry.entries.push_back({ "errno", bstringf("%2d \"%s\"", t, strerror_wrapper(t).c_str()) });
            }
        } else {
            if constexpr(is_string_type<T>) {
                if(i == 0) {
                    if constexpr(std::is_pointer_v<T>) {
                        if(t == nullptr) {
                            entry.message = "(nullptr)";
                            return;
                        }
                    }
                    entry.message = t;
                    return;
                }
            }
            entry.entries.push_back({ args_strings[i], generate_stringification(t) });
        }
    }

    template<typename... Args>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    extra_diagnostics process_args(const char* const* const args_strings, Args&... args) {
        extra_diagnostics entry;
        size_t i = 0;
        (process_arg(entry, i++, args_strings, args), ...);
        (void)args_strings;
        return entry;
    }

    /*
     * actual assertion handling, finally
     */

    // collection of assertion data that can be put in static storage and all passed by a single pointer
    struct LIBASSERT_EXPORT assert_static_parameters {
        const char* name;
        assert_type type;
        const char* expr_str;
        source_location location;
        const char* const* args_strings;
    };
}

/*
 * Public constructs
 */

namespace libassert {
    struct LIBASSERT_EXPORT verification_failure : std::exception {
        // I must just this once
        // NOLINTNEXTLINE(cppcoreguidelines-explicit-virtual-functions,modernize-use-override)
        [[nodiscard]] virtual const char* what() const noexcept final override;
    };

    class LIBASSERT_EXPORT assertion_printer {
        const detail::assert_static_parameters* params;
        const detail::extra_diagnostics& processed_args;
        detail::binary_diagnostics_descriptor& binary_diagnostics;
        void* raw_trace;
        size_t sizeof_args;
    public:
        assertion_printer() = delete;
        assertion_printer(
            const detail::assert_static_parameters* params,
            const detail::extra_diagnostics& processed_args,
            detail::binary_diagnostics_descriptor& binary_diagnostics,
            void* raw_trace,
            size_t sizeof_args
        );
        compl assertion_printer();
        assertion_printer(const assertion_printer&) = delete;
        assertion_printer(assertion_printer&&) = delete;
        assertion_printer& operator=(const assertion_printer&) = delete;
        assertion_printer& operator=(assertion_printer&&) = delete;
        [[nodiscard]] std::string operator()(int width, color_scheme) const;
        // filename, line, function, message
        [[nodiscard]] std::tuple<const char*, int, std::string, const char*> get_assertion_info() const;
    };
}

/*
 * Actual top-level assertion processing
 */

namespace libassert::detail {
    LIBASSERT_EXPORT size_t count_args_strings(const char* const*);

    LIBASSERT_EXPORT void fail(assert_type type, const assertion_printer& printer);

    template<typename A, typename B, typename C, typename... Args>
    LIBASSERT_ATTR_COLD LIBASSERT_ATTR_NOINLINE
    // TODO: Re-evaluate forwarding here.
    void process_assert_fail(
        expression_decomposer<A, B, C>& decomposer,
        const assert_static_parameters* params,
        // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
        Args&&... args
    ) {
        const auto* args_strings = params->args_strings;
        const size_t args_strings_count = count_args_strings(args_strings);
        const size_t sizeof_extra_diagnostics = sizeof...(args) - 1; // - 1 for pretty function signature
        LIBASSERT_PRIMITIVE_ASSERT(
            (sizeof...(args) == 1 && args_strings_count == 2) || args_strings_count == sizeof_extra_diagnostics + 1
        );
        // process_args needs to be called as soon as possible in case errno needs to be read
        const auto processed_args = process_args(args_strings, args...);
        opaque_trace raw_trace = get_stacktrace_opaque();
        // generate header
        binary_diagnostics_descriptor binary_diagnostics;
        // generate binary diagnostics
        if constexpr(is_nothing<C>) {
            static_assert(is_nothing<B> && !is_nothing<A>);
            if constexpr(isa<A, bool>) {
                (void)decomposer; // suppress warning in msvc
            } else {
                binary_diagnostics = generate_binary_diagnostic(
                    decomposer.a,
                    true,
                    params->expr_str,
                    "true",
                    "=="
                );
            }
        } else {
            auto [a_str, b_str] = decompose_expression(params->expr_str, C::op_string);
            binary_diagnostics = generate_binary_diagnostic(
                decomposer.a,
                decomposer.b,
                a_str.c_str(),
                b_str.c_str(),
                C::op_string
            );
        }
        // send off
        void* trace = raw_trace.trace;
        raw_trace.trace = nullptr; // TODO: Feels ugly
        assertion_printer printer {
            params,
            processed_args,
            binary_diagnostics,
            trace,
            sizeof_extra_diagnostics
        };
        fail(params->type, printer);
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

inline void ERROR_ASSERTION_FAILURE_IN_CONSTEXPR_CONTEXT() {
    // This non-constexpr method is called from an assertion in a constexpr context if a failure occurs. It is
    // intentionally a no-op.
}

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
 // Extra set of parentheses here because clang treats __extension__ as a low-precedence unary operator which interferes
 // with decltype(auto) in an expression like decltype(auto) x = __extension__ ({...}).y;
 #define LIBASSERT_STMTEXPR(B, R) (__extension__ ({ B R }))
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
 #define LIBASSERT_STATIC_CAST_TO_BOOL(x) static_cast<bool>(x)
#else
 #define LIBASSERT_STMTEXPR(B, R) [&](const char* libassert_msvc_pfunc) { B return R }(LIBASSERT_PFUNC)
 #define LIBASSERT_WARNING_PRAGMA_PUSH_CLANG
 #define LIBASSERT_WARNING_PRAGMA_POP_CLANG
 #define LIBASSERT_WARNING_PRAGMA_PUSH_GCC
 #define LIBASSERT_WARNING_PRAGMA_POP_GCC
 #define LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_GCC
 #define LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_CLANG
 #define LIBASSERT_STATIC_CAST_TO_BOOL(x) libassert::detail::static_cast_to_bool(x)
 namespace libassert::detail {
     template<typename T> constexpr bool static_cast_to_bool(T&& t) {
         return static_cast<bool>(t);
     }
 }
#endif

#if LIBASSERT_IS_GCC
 // __VA_OPT__ needed for GCC, https://gcc.gnu.org/bugzilla/show_bug.cgi?id=44317
 #define LIBASSERT_VA_ARGS(...) __VA_OPT__(,) __VA_ARGS__
#else
 // clang properly eats the comma with ##__VA_ARGS__
 #define LIBASSERT_VA_ARGS(...) , ##__VA_ARGS__
#endif

// __PRETTY_FUNCTION__ used because __builtin_FUNCTION() used in source_location (like __FUNCTION__) is just the method
// name, not signature
#define LIBASSERT_STATIC_DATA(name, type, expr_str, ...) \
    /* extra string here because of extra comma from map, also serves as terminator */ \
    /* LIBASSERT_STRINGIFY LIBASSERT_VA_ARGS because msvc */ \
    /* Trailing return type here to work around a gcc <= 9.2 bug */ \
    /* Oddly only affecting builds under -DNDEBUG https://godbolt.org/z/5Treozc4q */ \
    using libassert_params_t = libassert::detail::assert_static_parameters; \
    /* NOLINTNEXTLINE(*-avoid-c-arrays) */ \
    const libassert_params_t* libassert_params = []() -> const libassert_params_t* { \
        static constexpr const char* const libassert_arg_strings[] = { \
            LIBASSERT_MAP(LIBASSERT_STRINGIFY LIBASSERT_VA_ARGS(__VA_ARGS__)) "" \
        }; \
        static constexpr libassert_params_t _libassert_params = { \
            name LIBASSERT_COMMA \
            type LIBASSERT_COMMA \
            expr_str LIBASSERT_COMMA \
            {} LIBASSERT_COMMA \
            libassert_arg_strings LIBASSERT_COMMA \
        }; \
        return &_libassert_params; \
    }();

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
// Workaround for gcc bug 105734 / libassert bug #24
#define LIBASSERT_DESTROY_DECOMPOSER libassert_decomposer.compl expression_decomposer() /* NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move) */
#if LIBASSERT_IS_GCC
 #if __GNUC__ == 12 && __GNUC_MINOR__ == 1
  namespace libassert::detail {
      template<typename T> constexpr void destroy(T& t) {
          t.compl T();
      }
  }
  #undef LIBASSERT_DESTROY_DECOMPOSER
  #define LIBASSERT_DESTROY_DECOMPOSER libassert::detail::destroy(libassert_decomposer)
 #endif
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
            ERROR_ASSERTION_FAILURE_IN_CONSTEXPR_CONTEXT(); \
            failaction \
            LIBASSERT_STATIC_DATA(name, libassert::assert_type::type, #expr, __VA_ARGS__) \
            if constexpr(sizeof libassert_decomposer > 32) { \
                process_assert_fail( \
                    libassert_decomposer, \
                    libassert_params \
                    LIBASSERT_VA_ARGS(__VA_ARGS__) LIBASSERT_PRETTY_FUNCTION_ARG \
                ); \
            } else { \
                /* std::move it to assert_fail_m, will be moved back to r */ \
                process_assert_fail_n( \
                    std::move(libassert_decomposer), \
                    libassert_params \
                    LIBASSERT_VA_ARGS(__VA_ARGS__) LIBASSERT_PRETTY_FUNCTION_ARG \
                ); \
            } \
        } \
        LIBASSERT_WARNING_PRAGMA_POP_CLANG \
    } while(false) \

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
                ERROR_ASSERTION_FAILURE_IN_CONSTEXPR_CONTEXT(); \
                failaction \
                LIBASSERT_STATIC_DATA(name, libassert::assert_type::type, #expr, __VA_ARGS__) \
                if constexpr(sizeof libassert_decomposer > 32) { \
                    process_assert_fail( \
                        libassert_decomposer, \
                        libassert_params \
                        LIBASSERT_VA_ARGS(__VA_ARGS__) LIBASSERT_INVOKE_VAL_PRETTY_FUNCTION_ARG \
                    ); \
                } else { \
                    /* std::move it to assert_fail_m, will be moved back to r */ \
                    auto libassert_r = process_assert_fail_m( \
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
 #define LIBASSERT_ASSUME_ACTION LIBASSERT_UNREACHABLE;
#else
 #define LIBASSERT_ASSUME_ACTION
#endif

// assertion macros

// Debug assert
#ifndef NDEBUG
 #define DEBUG_ASSERT(expr, ...) LIBASSERT_INVOKE(expr, "DEBUG_ASSERT", debug_assertion, , __VA_ARGS__)
#else
 #define DEBUG_ASSERT(expr, ...) (void)0
#endif

#ifdef LIBASSERT_LOWERCASE
 #ifndef NDEBUG
  #define debug_assert(expr, ...) LIBASSERT_INVOKE(expr, "debug_assert", debug_assertion, , __VA_ARGS__)
 #else
  #define debug_assert(expr, ...) (void)0
 #endif
#endif

// Assert
#define ASSERT(expr, ...) LIBASSERT_INVOKE(expr, "ASSERT", assertion, , __VA_ARGS__)
// lowercase version intentionally done outside of the include guard here

// Assume
#define ASSUME(expr, ...) LIBASSERT_INVOKE(expr, "ASSUME", assumption, LIBASSERT_ASSUME_ACTION, __VA_ARGS__)

// value variants

#ifndef NDEBUG
 #define DEBUG_ASSERT_VAL(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, true, "DEBUG_ASSERT_VAL", debug_assertion, , __VA_ARGS__)
#else
 #define DEBUG_ASSERT_VAL(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, false, "DEBUG_ASSERT_VAL", debug_assertion, , __VA_ARGS__)
#endif

#ifdef LIBASSERT_LOWERCASE
 #ifndef NDEBUG
  #define debug_assert_val(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, true, "debug_assert_val", debug_assertion, , __VA_ARGS__)
 #else
  #define debug_assert_val(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, false, "debug_assert_val", debug_assertion, , __VA_ARGS__)
 #endif
#endif

#define ASSUME_VAL(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, true, "ASSUME_VAL", assumption, LIBASSERT_ASSUME_ACTION, __VA_ARGS__)

#define ASSERT_VAL(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, true, "ASSERT_VAL", verification, , __VA_ARGS__)

#ifdef LIBASSERT_LOWERCASE
 #define assert_val(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, true, "assert_val", assertion, , __VA_ARGS__)
#endif

#endif

// Intentionally done outside the include guard. Libc++ leaks `assert` (among other things), so the include for
// assert.hpp should go after other includes when using -DASSERT_LOWERCASE.
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
