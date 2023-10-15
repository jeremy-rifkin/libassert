#ifndef LIBASSERT_HPP
#define LIBASSERT_HPP

// Copyright (c) 2021-2023 Jeremy Rifkin under the MIT license
// https://github.com/jeremy-rifkin/libassert

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
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

#ifdef ASSERT_USE_MAGIC_ENUM
 // this is a temporary hack to make testing thing in compiler explorer quicker (it disallows simple relative includes)
 #include \
 "../third_party/magic_enum.hpp"
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
 #define GCC_ISNT_STUPID _GLIBCXX_USE_CXX11_ABI
#else
 // assume others target new abi by default - homework
 #define GCC_ISNT_STUPID 1
#endif

namespace libassert {
    enum class ASSERTION {
        NONFATAL, FATAL
    };

    enum class assert_type {
        debug_assertion,
        assertion,
        assumption,
        verification
    };

    class assertion_printer;
}

#ifndef ASSERT_FAIL
 #define ASSERT_FAIL libassert_default_fail_action
#endif

void ASSERT_FAIL(libassert::assert_type type, libassert::ASSERTION fatal, const libassert::assertion_printer& printer);

// always_false is just convenient to use here
#define LIBASSERT_PHONY_USE(E) ((void)libassert::detail::always_false<decltype(E)>)

/*
 * Internal mechanisms
 *
 * Macros exposed: LIBASSERT_PRIMITIVE_ASSERT
 */

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
            const char* _file     = __builtin_FILE(),
            int _line             = __builtin_LINE()
        ) : file(_file), /*function(_function),*/ line(_line) {}
    };

    // bootstrap with primitive implementations
    void primitive_assert_impl(
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

    /*
     * String utilities
     */

    [[nodiscard]] std::string bstringf(const char* format, ...);

    /*
     * System wrappers
     */

    [[nodiscard]] std::string strerror_wrapper(int err); // stupid C stuff, stupid microsoft stuff

    /*
     * Stacktrace implementation
     */

    // All in the .cpp

    struct opaque_trace {
        void* trace;
        ~opaque_trace();
        opaque_trace(void* t) : trace(t) {}
        opaque_trace(const opaque_trace&) = delete;
        opaque_trace(opaque_trace&&) = delete;
        opaque_trace& operator=(const opaque_trace&) = delete;
        opaque_trace& operator=(opaque_trace&&) = delete;
    };

    opaque_trace get_stacktrace_opaque();

    /*
     * metaprogramming utilities
     */

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

    template<typename T> inline constexpr bool is_arith_not_bool_char =
                                                       std::is_arithmetic_v<strip<T>> && !isa<T, bool> && !isa<T, char>;

    template<typename T> inline constexpr bool is_c_string =
           isa<std::decay_t<strip<T>>, char*> // <- covers literals (i.e. const char(&)[N]) too
        || isa<std::decay_t<strip<T>>, const char*>;

    template<typename T> inline constexpr bool is_string_type =
           isa<T, std::string>
        || isa<T, std::string_view>
        || is_c_string<T>;

    // char(&)[20], const char(&)[20], const char(&)[]
    template<typename T> inline constexpr bool is_string_literal =
           std::is_lvalue_reference<T>::value
        && std::is_array<typename std::remove_reference<T>::type>::value
        && isa<typename std::remove_extent<typename std::remove_reference<T>::type>::type, char>;

    template<typename T> typename std::add_lvalue_reference<T>::type decllval() noexcept;

    /*
     * expression decomposition
     */

    // Lots of boilerplate
    // Using int comparison functions here to support proper signed comparisons. Need to make sure
    // assert(map.count(1) == 2) doesn't produce a warning. It wouldn't under normal circumstances
    // but it would in this library due to the parameters being forwarded down a long chain.
    // And we want to provide as much robustness as possible anyways.
    // Copied and pasted from https://en.cppreference.com/w/cpp/utility/intcmp
    // Not using std:: versions because library is targetting C++17
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
        #ifdef ASSERT_DECOMPOSE_BINARY_LOGICAL
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
        template<typename U, typename std::enable_if<!isa<U, expression_decomposer>, int>::type = 0>
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
            if constexpr(std::is_lvalue_reference<A>::value) {
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
        #ifdef ASSERT_DECOMPOSE_BINARY_LOGICAL
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
    template<typename U> expression_decomposer(U&&)
             -> expression_decomposer<std::conditional_t<std::is_rvalue_reference_v<U>, std::remove_reference_t<U>, U>>;

    /*
     * C++ syntax analysis logic
     */

    enum class literal_format {
        character,
        dec,
        hex,
        octal,
        binary,
        none // needs to be at the end for sorting reasons
    };

    [[nodiscard]] std::string prettify_type(std::string type);

    [[nodiscard]] literal_format get_literal_format(const std::string& expression);

    [[nodiscard]] bool is_bitwise(std::string_view op);

    [[nodiscard]] std::pair<std::string, std::string> decompose_expression(
        const std::string& expression,
        std::string_view target_op
    );

    /*
     * stringification
     */

    LIBASSERT_ATTR_COLD [[nodiscard]]
    constexpr std::string_view substring_bounded_by(
        std::string_view sig,
        std::string_view l,
        std::string_view r
    ) noexcept {
        auto i = sig.find(l) + l.length();
        return sig.substr(i, sig.rfind(r) - i);
    }

    template<typename T>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    constexpr std::string_view type_name() noexcept {
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
         static_assert(false, "unsupported compiler");
        #endif
    }

    namespace stringification {
        [[nodiscard]] std::string stringify(const std::string&, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(const std::string_view&, literal_format = literal_format::none);
        // without nullptr_t overload msvc (without /permissive-) will call stringify(bool) and mingw
        [[nodiscard]] std::string stringify(std::nullptr_t, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(char, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(bool, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(short, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(int, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(long, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(long long, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(unsigned short, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(unsigned int, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(unsigned long, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(unsigned long long, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(float, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(double, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(long double, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(std::error_code ec, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(std::error_condition ec, literal_format = literal_format::none);
        #if __cplusplus >= 202002L
        [[nodiscard]] std::string stringify(std::strong_ordering, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(std::weak_ordering, literal_format = literal_format::none);
        [[nodiscard]] std::string stringify(std::partial_ordering, literal_format = literal_format::none);
        #endif

        #ifdef __cpp_lib_expected
        template<typename E>
        [[nodiscard]] std::string stringify(const std::unexpected<E>& x, literal_format fmt = literal_format::none) {
            return "unexpected " + stringify(x.error(), fmt == literal_format::none ? literal_format::dec : fmt);
        }

        template<typename T, typename E>
        [[nodiscard]] std::string stringify(const std::expected<T, E>& x, literal_format fmt = literal_format::none) {
            if(x.has_value()) {
                if constexpr(std::is_void_v<T>) {
                    return "expected void";
                } else {
                    return "expected " + stringify(*x, fmt == literal_format::none ? literal_format::dec : fmt);
                }
            } else {
                return "unexpected " + stringify(x.error(), fmt == literal_format::none ? literal_format::dec : fmt);
            }
        }
        #endif

        [[nodiscard]] std::string stringify_ptr(const void*, literal_format = literal_format::none);

        template<typename T, typename = void> class can_basic_stringify : public std::false_type {};
        template<typename T> class can_basic_stringify<
                                    T,
                                    std::void_t<decltype(stringify(std::declval<T>()))>
                                > : public std::true_type {};

        template<typename T, typename = void> class has_stream_overload : public std::false_type {};
        template<typename T> class has_stream_overload<
                                    T,
                                    std::void_t<decltype(std::declval<std::ostringstream>() << std::declval<T>())>
                                > : public std::true_type {};

        template<typename T, typename = void> class is_tuple_like : public std::false_type {};
        template<typename T> class is_tuple_like<
                                    T,
                                    std::void_t<
                                        typename std::tuple_size<T>::type,
                                        decltype(std::get<0>(std::declval<T>()))
                                    >
                                > : public std::true_type {};

        namespace adl {
            using std::size, std::begin, std::end; // ADL
            template<typename T, typename = void> class is_printable_container : public std::false_type {};
            template<typename T> class is_printable_container<
                                        T,
                                        std::void_t<
                                            decltype(size(decllval<T>())),
                                            decltype(begin(decllval<T>())),
                                            decltype(end(decllval<T>())),
                                            typename std::enable_if< // can stringify (and not just "instance of")
                                                has_stream_overload<decltype(*begin(decllval<T>()))>::value
                                                || std::is_enum<strip<decltype(*begin(decllval<T>()))>>::value
                                                || is_printable_container<strip<decltype(*begin(decllval<T>()))>>::value
                                                || is_tuple_like<strip<decltype(*begin(decllval<T>()))>>::value,
                                            void>::type
                                        >
                                    > : public std::true_type {};
        }

        template<typename T, size_t... I> std::string stringify_tuple_like(const T&, std::index_sequence<I...>);

        template<typename T> std::string stringify_tuple_like(const T& t) {
            return stringify_tuple_like(t, std::make_index_sequence<std::tuple_size<T>::value - 1>{});
        }

        template<typename T, typename std::enable_if<std::is_pointer<strip<typename std::decay<T>::type>>::value
                                                    || std::is_function<strip<T>>::value
                                                    || !can_basic_stringify<T>::value, int>::type = 0>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        std::string stringify(const T& t, [[maybe_unused]] literal_format fmt = literal_format::none) {
            if constexpr(
                has_stream_overload<T>::value && !is_string_type<T>
                && !std::is_pointer<strip<typename std::decay<T>::type>>::value
            ) {
                // clang-tidy bug here
                // NOLINTNEXTLINE(misc-const-correctness)
                std::ostringstream oss;
                oss<<t;
                return std::move(oss).str();
            } else if constexpr(adl::is_printable_container<T>::value && !is_c_string<T>) {
                using std::begin, std::end; // ADL
                std::string str = "[";
                const auto begin_it = begin(t);
                for(auto it = begin_it; it != end(t); it++) {
                    if(it != begin_it) {
                        str += ", ";
                    }
                    str += stringify(*it, literal_format::dec);
                }
                str += "]";
                return str;
            } else if constexpr(
                std::is_pointer<strip<typename std::decay<T>::type>>::value
                || std::is_function<strip<T>>::value
            ) {
                if constexpr(isa<typename std::remove_pointer<typename std::decay<T>::type>::type, char>) { // strings
                    const void* v = t; // circumvent -Wnonnull-compare
                    if(v != nullptr) {
                        return stringify(std::string_view(t)); // not printing type for now, TODO: reconsider?
                    }
                }
                return prettify_type(std::string(type_name<T>())) + ": "
                                                    + stringify_ptr(reinterpret_cast<const void*>(t), fmt);
            } else if constexpr(is_tuple_like<T>::value) {
                return stringify_tuple_like(t);
            }
            #ifdef ASSERT_USE_MAGIC_ENUM
            else if constexpr(std::is_enum<strip<T>>::value) {
                std::string_view name = magic_enum::enum_name(t);
                if(!name.empty()) {
                    return std::string(name);
                } else {
                    return bstringf("<instance of %s>", prettify_type(std::string(type_name<T>())).c_str());
                }
            }
            #endif
            else {
                return bstringf("<instance of %s>", prettify_type(std::string(type_name<T>())).c_str());
            }
        }

        // I'm going to assume at least one index because is_tuple_like requires index 0 to exist
        template<typename T, size_t... I> std::string stringify_tuple_like(const T& t, std::index_sequence<I...>) {
            using lf = literal_format;
            using stringification::stringify; // ADL
            return "["
                    + (stringify(std::get<0>(t), lf::dec) + ... + (", " + stringify(std::get<I + 1>(t), lf::dec)))
                    + "]";
        }
    }

    /*
     * assert diagnostics generation
     */

    constexpr size_t format_arr_length = 5;

    // TODO: Not yet happy with the naming of this function / how it's used
    template<typename T>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string generate_stringification(const T& v, literal_format fmt = literal_format::none) {
        using stringification::stringify; // ADL
        if constexpr((stringification::adl::is_printable_container<T>::value && !is_string_type<T>)) {
            using std::size; // ADL
            return prettify_type(std::string(type_name<T>()))
                       + " [size: " + std::to_string(size(v)) + "]: " + stringify(v, fmt);
        } else if constexpr(stringification::is_tuple_like<T>::value) {
            return prettify_type(std::string(type_name<T>())) + ": " + stringify(v, fmt);
        } else {
            return stringify(v, fmt);
        }
    }

    template<typename T>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::vector<std::string> generate_stringifications(const T& v, const literal_format (&formats)[format_arr_length]) {
        if constexpr((std::is_arithmetic<strip<T>>::value || std::is_enum<strip<T>>::value) && !isa<T, bool>) {
            std::vector<std::string> vec;
            for(literal_format fmt : formats) {
                if(fmt == literal_format::none) { break; }
                using stringification::stringify; // ADL
                // TODO: consider pushing empty fillers to keep columns aligned later on? Does not
                // matter at the moment because floats only have decimal and hex literals but could
                // if more formats are added.
                vec.push_back(stringify(v, fmt));
            }
            return vec;
        } else {
            return { generate_stringification(v) };
        }
    }

    struct binary_diagnostics_descriptor {
        std::vector<std::string> lstrings;
        std::vector<std::string> rstrings;
        std::string a_str;
        std::string b_str;
        bool multiple_formats;
        bool present = false;
        binary_diagnostics_descriptor(); // = default; in the .cpp
        binary_diagnostics_descriptor(
            std::vector<std::string>& lstrings,
            std::vector<std::string>& rstrings,
            std::string a_str,
            std::string b_str,
            bool multiple_formats
        );
        compl binary_diagnostics_descriptor(); // = default; in the .cpp
        binary_diagnostics_descriptor(const binary_diagnostics_descriptor&) = delete;
        binary_diagnostics_descriptor(binary_diagnostics_descriptor&&) noexcept; // = default; in the .cpp
        binary_diagnostics_descriptor& operator=(const binary_diagnostics_descriptor&) = delete;
        binary_diagnostics_descriptor&
        operator=(binary_diagnostics_descriptor&&) noexcept(GCC_ISNT_STUPID); // = default; in the .cpp
    };

    void sort_and_dedup(literal_format(&)[format_arr_length]);

    template<typename A, typename B>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    binary_diagnostics_descriptor generate_binary_diagnostic(
        const A& a,
        const B& b,
        const char* a_str,
        const char* b_str,
        std::string_view op
    ) {
        using lf = literal_format;
        // Note: op
        // figure out what information we need to print in the where clause
        // find all literal formats involved (literal_format::dec included for everything)
        auto lformat = get_literal_format(a_str);
        auto rformat = get_literal_format(b_str);
        // formerly used std::set here, now using array + sorting, `none` entries will be at the end and ignored
        constexpr bool either_is_character = isa<A, char> || isa<B, char>;
        constexpr bool either_is_arithmetic = is_arith_not_bool_char<A> || is_arith_not_bool_char<B>;
        lf formats[format_arr_length] = {
            either_is_arithmetic ? lf::dec :  lf::none,
            lformat, rformat, // ↓ always display binary for bitwise
            is_bitwise(op) ? lf::binary : lf::none,
            either_is_character ? lf::character : lf::none
        };
        sort_and_dedup(formats); // print in specific order, avoid duplicates
        if(formats[0] == lf::none) {
            formats[0] = lf::dec; // if no formats apply just print everything default, TODO this is a bit of a hack
        }
        // generate raw strings for given formats, without highlighting
        std::vector<std::string> lstrings = generate_stringifications(a, formats);
        std::vector<std::string> rstrings = generate_stringifications(b, formats);
        return binary_diagnostics_descriptor { lstrings, rstrings, a_str, b_str, formats[1] != lf::none };
    }

    #define LIBASSERT_X(x) #x
    #define LIBASSERT_Y(x) LIBASSERT_X(x)
    constexpr const std::string_view errno_expansion = LIBASSERT_Y(errno);
    #undef LIBASSERT_Y
    #undef LIBASSERT_X

    struct extra_diagnostics {
        ASSERTION fatality = ASSERTION::FATAL;
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

    template<typename T>
    LIBASSERT_ATTR_COLD
    // TODO
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void process_arg(extra_diagnostics& entry, size_t i, const char* const* const args_strings, const T& t) {
        if constexpr(isa<T, ASSERTION>) {
            entry.fatality = t;
        } else if constexpr(isa<T, pretty_function_name_wrapper>) {
            entry.pretty_function = t.pretty_function;
        } else {
            // three cases to handle: assert message, errno, and regular diagnostics
            #if LIBASSERT_IS_MSVC
             #pragma warning(push)
             #pragma warning(disable: 4127) // MSVC thinks constexpr should be used here. It should not.
            #endif
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
                        if constexpr(std::is_pointer<T>::value) {
                            if(t == nullptr) {
                                entry.message = "(nullptr)";
                                return;
                            }
                        }
                        entry.message = t;
                        return;
                    }
                }
                entry.entries.push_back({ args_strings[i], generate_stringification(t, literal_format::dec) });
            }
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

    struct lock {
        lock();
        compl lock();
        lock(const lock&) = delete;
        lock(lock&&) = delete;
        lock& operator=(const lock&) = delete;
        lock& operator=(lock&&) = delete;
    };

    // collection of assertion data that can be put in static storage and all passed by a single pointer
    struct assert_static_parameters {
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
    struct verification_failure : std::exception {
        // I must just this once
        // NOLINTNEXTLINE(cppcoreguidelines-explicit-virtual-functions,modernize-use-override)
        [[nodiscard]] virtual const char* what() const noexcept final override;
    };

    class assertion_printer {
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
        [[nodiscard]] std::string operator()(int width) const;
        // filename, line, function, message
        [[nodiscard]] std::tuple<const char*, int, std::string, const char*> get_assertion_info() const;
    };
}

/*
 * Public utilities
 */

namespace libassert::utility {
    // strip ansi escape sequences from a string
    [[nodiscard]] std::string strip_colors(const std::string& str);

    // replace 24-bit rgb ansi color sequences with traditional color sequences
    [[nodiscard]] std::string replace_rgb(std::string str);

    // returns the width of the terminal represented by fd, will be 0 on error
    [[nodiscard]] int terminal_width(int fd);

    // generates a stack trace, formats to the given width
    [[nodiscard]] std::string stacktrace(int width);

    // returns the type name of T
    template<typename T>
    [[nodiscard]] std::string_view type_name() noexcept {
        return detail::type_name<T>();
    }

    // returns the prettified type name for T
    template<typename T>
    [[nodiscard]] std::string pretty_type_name() noexcept {
        return detail::prettify_type(std::string(detail::type_name<T>()));
    }

    // returns a debug stringification of t
    template<typename T>
    [[nodiscard]] std::string stringify(const T& t) {
        using detail::stringification::stringify; // ADL
        using lf = detail::literal_format;
        return stringify(t, detail::isa<T, char> ? lf::character : lf::dec);
    }
}

/*
 * Configuration
 */

namespace libassert::config {
    // configures whether the default assertion handler prints in color or not to tty devices
    void set_color_output(bool);
    // configure whether to use 24-bit rgb ansi color sequences or traditional ansi color sequences
    void set_rgb_output(bool);
}

/*
 * Actual top-level assertion processing
 */

namespace libassert::detail {
    size_t count_args_strings(const char* const*);

    template<typename A, typename B, typename C, typename... Args>
    LIBASSERT_ATTR_COLD LIBASSERT_ATTR_NOINLINE
    // TODO: Re-evaluate forwarding here.
    void process_assert_fail(
        expression_decomposer<A, B, C>& decomposer,
        const assert_static_parameters* params,
        // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
        Args&&... args
    ) {
        const lock l;
        const auto* args_strings = params->args_strings;
        const size_t args_strings_count = count_args_strings(args_strings);
        const size_t sizeof_extra_diagnostics = sizeof...(args) - 1; // - 1 for pretty function signature
        LIBASSERT_PRIMITIVE_ASSERT(
            (sizeof...(args) == 1 && args_strings_count == 2) || args_strings_count == sizeof_extra_diagnostics + 1
        );
        // process_args needs to be called as soon as possible in case errno needs to be read
        const auto processed_args = process_args(args_strings, args...);
        const auto fatal = processed_args.fatality;
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
        ::ASSERT_FAIL(params->type, fatal, printer);
    }

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
                if constexpr(std::is_lvalue_reference<A>::value) {
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

inline void ERROR_ASSERTION_FAILURE_IN_CONSTEXPR_CONTEXT() {
    // This non-constexpr method is called from an assertion in a constexpr context if a failure occurs. It is
    // intentionally a no-op.
}

// NOLINTNEXTLINE(misc-unused-using-decls)
using libassert::ASSERTION;

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
 #define LIBASSERT_PRETTY_FUNCTION_ARG ,libassert::detail::pretty_function_name_wrapper{libassert_msvc_pfunc}
#else
 #define LIBASSERT_PRETTY_FUNCTION_ARG ,libassert::detail::pretty_function_name_wrapper{LIBASSERT_PFUNC}
#endif
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
#define ASSERT_INVOKE(expr, doreturn, check_expression, name, type, failaction, ...) \
        /* must push/pop out here due to nasty clang bug https://github.com/llvm/llvm-project/issues/63897 */ \
        /* must do awful stuff to workaround differences in where gcc and clang allow these directives to go */ \
        LIBASSERT_WARNING_PRAGMA_PUSH_CLANG \
        LIBASSERT_IGNORE_UNUSED_VALUE \
        LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_CLANG \
        LIBASSERT_STMTEXPR( \
          LIBASSERT_WARNING_PRAGMA_PUSH_GCC \
          LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA_GCC \
          auto libassert_decomposer = \
                         libassert::detail::expression_decomposer(libassert::detail::expression_decomposer{} << expr); \
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
                process_assert_fail(libassert_decomposer, libassert_params \
                                           LIBASSERT_VA_ARGS(__VA_ARGS__) LIBASSERT_PRETTY_FUNCTION_ARG); \
              } else { \
                /* std::move it to assert_fail_m, will be moved back to r */ \
                auto libassert_r = process_assert_fail_m(std::move(libassert_decomposer), libassert_params \
                                           LIBASSERT_VA_ARGS(__VA_ARGS__) LIBASSERT_PRETTY_FUNCTION_ARG); \
                /* can't move-assign back to decomposer if it holds reference members */ \
                LIBASSERT_DESTROY_DECOMPOSER; \
                new (&libassert_decomposer) libassert::detail::expression_decomposer(std::move(libassert_r)); \
              } \
            } \
          }, \
          /* Note: std::launder needed in 17 in case of placement new / move shenanigans above */ \
          /* https://timsong-cpp.github.io/cppwp/n4659/basic.life#8.3 */ \
          /* Note: Somewhat relying on this call being inlined so inefficiency is eliminated */ \
          libassert::detail::get_expression_return_value <doreturn LIBASSERT_COMMA \
            libassert_ret_lhs LIBASSERT_COMMA std::is_lvalue_reference<decltype(libassert_value)>::value> \
              (libassert_value, *std::launder(&libassert_decomposer)); \
        ) LIBASSERT_IF(doreturn)(.value,) \
        LIBASSERT_WARNING_PRAGMA_POP_CLANG

#ifdef NDEBUG
 #define LIBASSERT_ASSUME_ACTION LIBASSERT_UNREACHABLE;
#else
 #define LIBASSERT_ASSUME_ACTION
#endif

#ifndef NDEBUG
 #define DEBUG_ASSERT(expr, ...) ASSERT_INVOKE(expr, false, true, "DEBUG_ASSERT", debug_assertion, , __VA_ARGS__)
 #define ASSERT(expr, ...) ASSERT_INVOKE(expr, true, true, "ASSERT", assertion, , __VA_ARGS__)
#else
 #define DEBUG_ASSERT(expr, ...) (void)0
 #define ASSERT(expr, ...) ASSERT_INVOKE(expr, true, false, "ASSERT", assertion, , __VA_ARGS__)
#endif

#ifdef ASSERT_LOWERCASE
 #ifndef NDEBUG
  #define debug_assert(expr, ...) ASSERT_INVOKE(expr, false, true, "debug_assert", debug_assertion, , __VA_ARGS__)
 #else
  #define debug_assert(expr, ...) (void)0
 #endif
#endif

#ifdef NO_ASSERT_RELEASE_EVAL
 #undef ASSERT
 #ifndef NDEBUG
  #define ASSERT(expr, ...) ASSERT_INVOKE(expr, false, true, "ASSERT", assertion, , __VA_ARGS__)
 #else
  #define ASSERT(expr, ...) (void)0
 #endif
 #ifdef ASSERT_LOWERCASE
  #undef assert
  #ifndef NDEBUG
   #define assert(expr, ...) ASSERT_INVOKE(expr, false, true, "assert", assertion, , __VA_ARGS__)
  #else
   #define assert(expr, ...) (void)0
  #endif
 #endif
#endif

#define ASSUME(expr, ...) ASSERT_INVOKE(expr, true, true, "ASSUME", assumption, LIBASSERT_ASSUME_ACTION, __VA_ARGS__)

#define VERIFY(expr, ...) ASSERT_INVOKE(expr, true, true, "VERIFY", verification, , __VA_ARGS__)

#ifndef LIBASSERT_IS_CPP // keep macros for the .cpp
 #undef LIBASSERT_IS_CLANG
 #undef LIBASSERT_IS_GCC
 #undef LIBASSERT_IS_MSVC
 #undef LIBASSERT_ATTR_COLD
 #undef LIBASSERT_ATTR_NOINLINE
 #undef LIBASSERT_PRIMITIVE_ASSERT
#endif

#endif

// Intentionally done outside the include guard. Libc++ leaks `assert` (among other things), so the include for
// assert.hpp should go after other includes when using -DASSERT_LOWERCASE.
#ifdef ASSERT_LOWERCASE
 #ifdef assert
  #undef assert
 #endif
 #ifndef NDEBUG
  #define assert(expr, ...) ASSERT_INVOKE(expr, true, true, "assert", assertion, , __VA_ARGS__)
 #else
  #define assert(expr, ...) ASSERT_INVOKE(expr, true, false, "assert", assertion, , __VA_ARGS__)
 #endif
#endif
