#ifndef ASSERT_HPP
#define ASSERT_HPP

// Copyright 2022 Jeremy Rifkin
// https://github.com/jeremy-rifkin/asserts

#include <cstdio>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#if __cplusplus >= 202002L
 #include <compare>
#endif

#ifdef ASSERT_USE_MAGIC_ENUM
 #include "../third_party/magic_enum.hpp"
#endif

#if defined(__clang__)
 #define ASSERT_DETAIL_IS_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
 #define ASSERT_DETAIL_IS_GCC 1
#elif defined(_MSC_VER)
 #define ASSERT_DETAIL_IS_MSVC 1
 #include <iso646.h> // alternative operator tokens are standard but msvc requires the include or /permissive- or /Za
#else
 #error "Unsupported compiler"
#endif

#if ASSERT_DETAIL_IS_CLANG || ASSERT_DETAIL_IS_GCC
 #define ASSERT_DETAIL_PFUNC __extension__ __PRETTY_FUNCTION__
 #define ASSERT_DETAIL_ATTR_COLD     [[gnu::cold]]
 #define ASSERT_DETAIL_ATTR_NOINLINE [[gnu::noinline]]
 #define ASSERT_DETAIL_UNREACHABLE __builtin_unreachable()
#else
 #define ASSERT_DETAIL_PFUNC __FUNCSIG__
 #define ASSERT_DETAIL_ATTR_COLD
 #define ASSERT_DETAIL_ATTR_NOINLINE __declspec(noinline)
 #define ASSERT_DETAIL_UNREACHABLE __assume(false)
#endif

#if ASSERT_DETAIL_IS_MSVC
 #define ASSERT_DETAIL_STRONG_EXPECT(expr, value) (expr)
#elif defined(__clang__) && __clang_major__ >= 11 || __GNUC__ >= 9
 #define ASSERT_DETAIL_STRONG_EXPECT(expr, value) __builtin_expect_with_probability((expr), (value), 1)
#else
 #define ASSERT_DETAIL_STRONG_EXPECT(expr, value) __builtin_expect((expr), (value))
#endif

namespace asserts {
    enum class ASSERTION {
        NONFATAL, FATAL
    };

    enum class assert_type {
        assertion,
        verify,
        check
    };

    class assertion_printer;
}

#ifndef ASSERT_FAIL
 #define ASSERT_FAIL assert_detail_default_fail_action
#endif

void ASSERT_FAIL(asserts::assert_type type, asserts::ASSERTION fatal, const asserts::assertion_printer& printer);

// always_false is just convenient to use here
#define ASSERT_DETAIL_PHONY_USE(E) ((void)asserts::detail::always_false<decltype(E)>)

/*
 * Internal mechanisms
 *
 * Macros exposed: ASSERT_DETAIL_PRIMITIVE_ASSERT
 */

namespace asserts::detail {
    // Lightweight helper, eventually may use C++20 std::source_location if this library no longer
    // targets C++17. Note: __builtin_FUNCTION only returns the name, so __PRETTY_FUNCTION__ is
    // still needed.
    struct source_location {
        const char* const file;
        const char* const function;
        const int line;
        constexpr source_location(
            const char* _function /*= __builtin_FUNCTION()*/,
            const char* _file     = __builtin_FILE(),
            int _line             = __builtin_LINE()
        ) : file(_file), function(_function), line(_line) {}
    };

    // bootstrap with primitive implementations
    void primitive_assert_impl(bool condition, bool verify, const char* expression,
                               source_location location, const char* message = nullptr);

    #ifndef NDEBUG
     #define ASSERT_DETAIL_PRIMITIVE_ASSERT(c, ...) asserts::detail::primitive_assert_impl(c, false, #c, \
                                                                                     ASSERT_DETAIL_PFUNC, ##__VA_ARGS__)
    #else
     #define ASSERT_DETAIL_PRIMITIVE_ASSERT(c, ...) ASSERT_DETAIL_PHONY_USE(c)
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

    void* get_stacktrace_opaque();

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

    template<typename T> inline constexpr bool is_string_type =
           isa<T, std::string>
        || isa<T, std::string_view>
        || isa<std::decay_t<strip<T>>, char*> // <- covers literals (i.e. const char(&)[N]) too
        || isa<std::decay_t<strip<T>>, const char*>;

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
        #define ASSERT_DETAIL_GEN_OP_BOILERPLATE(name, op) struct name { \
            static constexpr std::string_view op_string = #op; \
            template<typename A, typename B> \
            ASSERT_DETAIL_ATTR_COLD [[nodiscard]] \
            constexpr decltype(auto) operator()(A&& lhs, B&& rhs) const { /* no need to forward ints */ \
                return std::forward<A>(lhs) op std::forward<B>(rhs); \
            } \
        }
        #define ASSERT_DETAIL_GEN_OP_BOILERPLATE_SPECIAL(name, op, cmp) struct name { \
            static constexpr std::string_view op_string = #op; \
            template<typename A, typename B> \
            ASSERT_DETAIL_ATTR_COLD [[nodiscard]] \
            constexpr decltype(auto) operator()(A&& lhs, B&& rhs) const { /* no need to forward ints */ \
                if constexpr(is_integral_and_not_bool<A> && is_integral_and_not_bool<B>) return cmp(lhs, rhs); \
                else return std::forward<A>(lhs) op std::forward<B>(rhs); \
            } \
        }
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(shl, <<);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(shr, >>);
        #if __cplusplus >= 202002L
         ASSERT_DETAIL_GEN_OP_BOILERPLATE(spaceship, <=>);
        #endif
        ASSERT_DETAIL_GEN_OP_BOILERPLATE_SPECIAL(eq,   ==, cmp_equal);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE_SPECIAL(neq,  !=, cmp_not_equal);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE_SPECIAL(gt,    >, cmp_greater);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE_SPECIAL(lt,    <, cmp_less);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE_SPECIAL(gteq, >=, cmp_greater_equal);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE_SPECIAL(lteq, <=, cmp_less_equal);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(band,   &);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(bxor,   ^);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(bor,    |);
        #ifdef ASSERT_DECOMPOSE_BINARY_LOGICAL
         ASSERT_DETAIL_GEN_OP_BOILERPLATE(land,   &&);
         ASSERT_DETAIL_GEN_OP_BOILERPLATE(lor,    ||);
        #endif
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(assign, =);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(add_assign,  +=);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(sub_assign,  -=);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(mul_assign,  *=);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(div_assign,  /=);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(mod_assign,  %=);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(shl_assign,  <<=);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(shr_assign,  >>=);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(band_assign, &=);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(bxor_assign, ^=);
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(bor_assign,  |=);
        #undef ASSERT_DETAIL_GEN_OP_BOILERPLATE
        #undef ASSERT_DETAIL_GEN_OP_BOILERPLATE_SPECIAL
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
        explicit expression_decomposer() = default;
        compl expression_decomposer() = default;
        // not copyable
        expression_decomposer(const expression_decomposer&) = delete;
        expression_decomposer& operator=(const expression_decomposer&) = delete;
        // allow move construction
        expression_decomposer(expression_decomposer&&)
        #if !ASSERT_DETAIL_IS_GCC || __GNUC__ >= 10 // gcc 9 has some issue with the move constructor being noexcept
         noexcept
        #endif
         = default;
        expression_decomposer& operator=(expression_decomposer&&) = delete;
        // value constructors
        template<typename U>
        // NOLINTNEXTLINE(bugprone-forwarding-reference-overload) // TODO
        explicit expression_decomposer(U&& _a) : a(std::forward<U>(_a)) {}
        template<typename U, typename V>
        explicit expression_decomposer(U&& _a, V&& _b) : a(std::forward<U>(_a)), b(std::forward<V>(_b)) {}
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
        decltype(auto) get_value() {
            if constexpr(is_nothing<C>) {
                static_assert(is_nothing<B> && !is_nothing<A>);
                return (((((((((((((((((((a)))))))))))))))))));
            } else {
                return C()(a, b);
            }
        }
        [[nodiscard]]
        operator bool() { // for ternary support
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
        A take_lhs() { // should only be called if ret_lhs() == true
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
        template<typename O> [[nodiscard]] auto operator<<(O&& operand) && {
            using Q = std::conditional_t<std::is_rvalue_reference_v<O>, std::remove_reference_t<O>, O>;
            if constexpr(is_nothing<A>) {
                static_assert(is_nothing<B> && is_nothing<C>);
                return expression_decomposer<Q, nothing, nothing>(std::forward<O>(operand));
            } else if constexpr(is_nothing<B>) {
                static_assert(is_nothing<C>);
                return expression_decomposer<A, Q, ops::shl>(std::forward<A>(a), std::forward<O>(operand));
            } else {
                static_assert(!is_nothing<C>);
                return expression_decomposer<decltype(get_value()), O, ops::shl>(std::forward<A>(get_value()), std::forward<O>(operand));
            }
        }
        #define ASSERT_DETAIL_GEN_OP_BOILERPLATE(functor, op) \
        template<typename O> [[nodiscard]] auto operator op(O&& operand) && { \
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
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::shr, >>)
        #if __cplusplus >= 202002L
         ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::spaceship, <=>)
        #endif
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::eq, ==)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::neq, !=)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::gt, >)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::lt, <)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::gteq, >=)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::lteq, <=)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::band, &)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::bxor, ^)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::bor, |)
        #ifdef ASSERT_DECOMPOSE_BINARY_LOGICAL
         ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::land, &&)
         ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::lor, ||)
        #endif
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::assign, =) // NOLINT(cppcoreguidelines-c-copy-assignment-signature, misc-unconventional-assign-operator)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::add_assign, +=)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::sub_assign, -=)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::mul_assign, *=)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::div_assign, /=)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::mod_assign, %=)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::shl_assign, <<=)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::shr_assign, >>=)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::band_assign, &=)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::bxor_assign, ^=)
        ASSERT_DETAIL_GEN_OP_BOILERPLATE(ops::bor_assign, |=)
        #undef ASSERT_DETAIL_GEN_OP_BOILERPLATE
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

    [[nodiscard]] std::pair<std::string, std::string> decompose_expression(const std::string& expression,
                                                                           std::string_view target_op);

    /*
     * stringification
     */

    ASSERT_DETAIL_ATTR_COLD [[nodiscard]]
    constexpr std::string_view substring_bounded_by(std::string_view sig, std::string_view l, std::string_view r)
                                                                                                              noexcept {
        auto i = sig.find(l) + l.length();
        return sig.substr(i, sig.rfind(r) - i);
    }

    template<typename T>
    ASSERT_DETAIL_ATTR_COLD [[nodiscard]]
    constexpr std::string_view type_name() noexcept {
        // Cases to handle:
        // gcc:   constexpr std::string_view ns::type_name() [with T = int; std::string_view = std::basic_string_view<char>]
        // clang: std::string_view ns::type_name() [T = int]
        // msvc:  class std::basic_string_view<char,struct std::char_traits<char> > __cdecl ns::type_name<int>(void)
        #if ASSERT_DETAIL_IS_CLANG
         return substring_bounded_by(ASSERT_DETAIL_PFUNC, "[T = ", "]");
        #elif ASSERT_DETAIL_IS_GCC
         return substring_bounded_by(ASSERT_DETAIL_PFUNC, "[with T = ", "; std::string_view = ");
        #elif ASSERT_DETAIL_IS_MSVC
         return substring_bounded_by(ASSERT_DETAIL_PFUNC, "type_name<", ">(void)");
        #else
         static_assert(false, "unsupported compiler");
        #endif
    }

    std::string stringify(const std::string&, literal_format = literal_format::none);
    std::string stringify(const std::string_view&, literal_format = literal_format::none);
    // without nullptr_t overload msvc (without /permissive-) will call stringify(bool) and mingw
    std::string stringify(std::nullptr_t, literal_format = literal_format::none);
    std::string stringify(char, literal_format = literal_format::none);
    std::string stringify(bool, literal_format = literal_format::none);
    std::string stringify(short, literal_format = literal_format::none);
    std::string stringify(int, literal_format = literal_format::none);
    std::string stringify(long, literal_format = literal_format::none);
    std::string stringify(long long, literal_format = literal_format::none);
    std::string stringify(unsigned short, literal_format = literal_format::none);
    std::string stringify(unsigned int, literal_format = literal_format::none);
    std::string stringify(unsigned long, literal_format = literal_format::none);
    std::string stringify(unsigned long long, literal_format = literal_format::none);
    std::string stringify(float, literal_format = literal_format::none);
    std::string stringify(double, literal_format = literal_format::none);
    std::string stringify(long double, literal_format = literal_format::none);
    #if __cplusplus >= 202002L
     std::string stringify(std::strong_ordering, literal_format = literal_format::none);
     std::string stringify(std::weak_ordering, literal_format = literal_format::none);
     std::string stringify(std::partial_ordering, literal_format = literal_format::none);
    #endif

    std::string stringify_ptr(const void*, literal_format = literal_format::none);

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

    template<typename T, typename = void> class is_printable_container : public std::false_type {};
    template<typename T> class is_printable_container<
                                T,
                                std::void_t<
                                    decltype(std::declval<T>().size()),
                                    decltype(begin(std::declval<T>())),
                                    decltype(end(std::declval<T>())),
                                    typename std::enable_if< // can stringify (and not just give an instance of string)
                                        has_stream_overload<decltype(*begin(std::declval<T>()))>::value
                                        || std::is_enum<strip<decltype(*begin(std::declval<T>()))>>::value
                                        || is_printable_container<strip<decltype(*begin(std::declval<T>()))>>::value
                                        || is_tuple_like<strip<decltype(*begin(std::declval<T>()))>>::value,
                                    void>::type
                                >
                            > : public std::true_type {};

    template<typename T, size_t... I> std::string stringify_tuple_like(const T&, std::index_sequence<I...>);

    template<typename T> std::string stringify_tuple_like(const T& t) {
        return stringify_tuple_like(t, std::make_index_sequence<std::tuple_size<T>::value - 1>{});
    }

    template<typename T, typename std::enable_if<std::is_pointer<strip<typename std::decay<T>::type>>::value
                                                 || std::is_function<strip<T>>::value
                                                 || !can_basic_stringify<T>::value, int>::type = 0>
    ASSERT_DETAIL_ATTR_COLD [[nodiscard]]
    std::string stringify(const T& t, [[maybe_unused]] literal_format fmt = literal_format::none) {
        if constexpr(std::is_pointer<strip<typename std::decay<T>::type>>::value || std::is_function<strip<T>>::value) {
            if constexpr(isa<typename std::remove_pointer<typename std::decay<T>::type>::type, char>) { // strings
                const void* v = t; // circumvent -Wnonnull-compare
                if(v != nullptr) {
                    return stringify(std::string_view(t)); // not printing type for now, TODO: reconsider?
                }
            }
            return prettify_type(std::string(type_name<T>())) + ": " + stringify_ptr(reinterpret_cast<const void*>(t), fmt);
        } else if constexpr(has_stream_overload<T>::value) {
            std::ostringstream oss;
            oss<<t;
            return std::move(oss).str();
        } else if constexpr(is_printable_container<T>::value) {
            std::string str = "[";
            for(auto it = begin(t); it != end(t); it++) {
                if(it != begin(t)) {
                    str += ", ";
                }
                str += stringify(*it, literal_format::dec);
            }
            str += "]";
            return str;
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
        return "[" + (stringify(std::get<0>(t), lf::dec) + ... + (", " + stringify(std::get<I + 1>(t), lf::dec))) + "]";
    }

    /*
     * assert diagnostics generation
     */

    constexpr size_t format_arr_length = 5;

    // TODO: Not yet happy with the naming of this function / how it's used
    template<typename T>
    ASSERT_DETAIL_ATTR_COLD [[nodiscard]]
    std::string generate_stringification(const T& v, literal_format fmt = literal_format::none) {
        if constexpr((is_printable_container<T>::value && !is_string_type<T>)) {
            return prettify_type(std::string(type_name<T>()))
                       + " [size: " + std::to_string(v.size()) + "]: " + stringify(v, fmt);
        } else if constexpr(is_tuple_like<T>::value) {
            return prettify_type(std::string(type_name<T>())) + ": " + stringify(v, fmt);
        } else {
            return stringify(v, fmt);
        }
    }

    template<typename T>
    ASSERT_DETAIL_ATTR_COLD [[nodiscard]]
    std::vector<std::string> generate_stringifications(const T& v, const literal_format (&formats)[format_arr_length]) {
        if constexpr((std::is_arithmetic<strip<T>>::value || std::is_enum<strip<T>>::value) && !isa<T, bool>) {
            std::vector<std::string> vec;
            for(literal_format fmt : formats) {
                if(fmt == literal_format::none) { break; }
                // TODO: consider pushing empty fillers to keep columns aligned later on? Does not
                // matter at the moment because floats only have decimal and hex literals but could
                // if more formats are added.
                std::string str = stringify(v, fmt);
                vec.push_back(std::move(str));
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
        binary_diagnostics_descriptor(std::vector<std::string>& lstrings, std::vector<std::string>& rstrings,
                                      std::string a_str, std::string b_str, bool multiple_formats);
        compl binary_diagnostics_descriptor(); // = default; in the .cpp
        binary_diagnostics_descriptor(const binary_diagnostics_descriptor&) = delete;
        binary_diagnostics_descriptor(binary_diagnostics_descriptor&&) noexcept; // = default; in the .cpp
        binary_diagnostics_descriptor& operator=(const binary_diagnostics_descriptor&) = delete;
        binary_diagnostics_descriptor& operator=(binary_diagnostics_descriptor&&) noexcept; // = default; in the .cpp
    };

    void sort_and_dedup(literal_format(&)[format_arr_length]);

    template<typename A, typename B>
    ASSERT_DETAIL_ATTR_COLD [[nodiscard]]
    binary_diagnostics_descriptor generate_binary_diagnostic(const A& a, const B& b,
                                                             const char* a_str, const char* b_str,
                                                             std::string_view op) {
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

    #define ASSERT_DETAIL_X(x) #x
    #define ASSERT_DETAIL_Y(x) ASSERT_DETAIL_X(x)
    constexpr const std::string_view errno_expansion = ASSERT_DETAIL_Y(errno);
    #undef ASSERT_DETAIL_Y
    #undef ASSERT_DETAIL_X

    struct extra_diagnostics {
        ASSERTION fatality = ASSERTION::FATAL;
        std::string message;
        std::vector<std::pair<std::string, std::string>> entries;
        #if ASSERT_DETAIL_IS_MSVC
         const char* pretty_function = "<error>";
        #endif
        extra_diagnostics();
        compl extra_diagnostics();
        extra_diagnostics(const extra_diagnostics&) = delete;
        extra_diagnostics(extra_diagnostics&&) noexcept; // = default; in the .cpp
        extra_diagnostics& operator=(const extra_diagnostics&) = delete;
        extra_diagnostics& operator=(extra_diagnostics&&) = delete;
    };

    #if ASSERT_DETAIL_IS_MSVC
     struct msvc_pretty_function_wrapper {
         const char* pretty_function;
     };
    #endif

    template<typename T>
    ASSERT_DETAIL_ATTR_COLD
    void process_arg(extra_diagnostics& entry, size_t i, const char* const* const args_strings, const T& t) {
        if constexpr(isa<T, ASSERTION>) {
            entry.fatality = t;
        }
        #if ASSERT_DETAIL_IS_MSVC
         if constexpr(isa<T, msvc_pretty_function_wrapper>) {
              entry.pretty_function = t.pretty_function;
         }
        #endif
        else {
            // three cases to handle: assert message, errno, and regular diagnostics
            #if ASSERT_DETAIL_IS_MSVC
             #pragma warning(push)
             #pragma warning(disable: 4127) // MSVC thinks constexpr should be used here. It should not.
            #endif
            if(isa<T, strip<decltype(errno)>> && args_strings[i] == errno_expansion) {
            #if ASSERT_DETAIL_IS_MSVC
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
    ASSERT_DETAIL_ATTR_COLD [[nodiscard]]
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

namespace asserts {
    struct verification_failure : std::exception {
        // I must just this once
        // NOLINTNEXTLINE(cppcoreguidelines-explicit-virtual-functions,modernize-use-override)
        [[nodiscard]] virtual const char* what() const noexcept final override;
    };

    struct check_failure : std::exception {
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
        assertion_printer(const detail::assert_static_parameters* params,
                          const detail::extra_diagnostics& processed_args,
                          detail::binary_diagnostics_descriptor& binary_diagnostics,
                          void* raw_trace, size_t sizeof_args);
        compl assertion_printer();
        assertion_printer(const assertion_printer&) = delete;
        assertion_printer(assertion_printer&&) = delete;
        assertion_printer& operator=(const assertion_printer&) = delete;
        assertion_printer& operator=(assertion_printer&&) = delete;
        [[nodiscard]] std::string operator()(int width) const;
    };
}

/*
 * Public utilities
 */

namespace asserts::utility {
    // strip ansi escape sequences from a string
    [[nodiscard]] std::string strip_colors(const std::string& str);

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

    // returns a text representation of t
    template<typename T>
    [[nodiscard]] std::string stringify(const T& t) {
        using lf = detail::literal_format;
        return detail::stringify(t, detail::isa<T, char> ? lf::character : lf::dec);
    }
}

/*
 * Configuration
 */

namespace asserts::config {
    void set_color_output(bool);
}

/*
 * Actual top-level assertion processing
 */

namespace asserts::detail {
    size_t count_args_strings(const char* const*);

    template<typename A, typename B, typename C, typename... Args>
    ASSERT_DETAIL_ATTR_COLD ASSERT_DETAIL_ATTR_NOINLINE
    void process_assert_fail(expression_decomposer<A, B, C>& decomposer,
                                const assert_static_parameters* params, Args&&... args) {
        lock l;
        const auto* args_strings = params->args_strings;
        size_t args_strings_count = count_args_strings(args_strings);
        size_t sizeof_extra_diagnostics = sizeof...(args)
        #ifdef ASSERT_DETAIL_IS_MSVC
        - 1
        #endif
        ;
        ASSERT_DETAIL_PRIMITIVE_ASSERT((sizeof...(args) == 0 && args_strings_count == 2)
                                       || args_strings_count == sizeof_extra_diagnostics + 1);
        // process_args needs to be called as soon as possible in case errno needs to be read
        const auto processed_args = process_args(args_strings, args...);
        const auto fatal = processed_args.fatality;
        void* raw_trace = get_stacktrace_opaque();
        // generate header
        std::string output;
        binary_diagnostics_descriptor binary_diagnostics;
        // generate binary diagnostics
        if constexpr(is_nothing<C>) {
            static_assert(is_nothing<B> && !is_nothing<A>);
            (void)decomposer; // suppress warning in msvc
        } else {
            auto [a_str, b_str] = decompose_expression(params->expr_str, C::op_string);
            binary_diagnostics = generate_binary_diagnostic(decomposer.a, decomposer.b,
                                                            a_str.c_str(), b_str.c_str(), C::op_string);
        }
        // send off
        assertion_printer printer {
            params,
            processed_args,
            binary_diagnostics,
            raw_trace,
            sizeof_extra_diagnostics
        };
        ::ASSERT_FAIL(params->type, fatal, printer);
    }

    template<typename A, typename B, typename C, typename... Args>
    ASSERT_DETAIL_ATTR_COLD ASSERT_DETAIL_ATTR_NOINLINE [[nodiscard]]
    expression_decomposer<A, B, C> process_assert_fail_m(expression_decomposer<A, B, C> decomposer,
                                           const assert_static_parameters* params, Args&&... args) {
        process_assert_fail(decomposer, params, std::forward<Args>(args)...);
        return decomposer;
    }

    template<typename T>
    struct assert_value_wrapper {
        T value;
    };

    template<bool R, bool ret_lhs, bool value_is_lval_ref,
             typename T, typename A, typename B, typename C>
    auto get_expression_return_value(T& value, expression_decomposer<A, B, C>& decomposer) {
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

// NOLINTNEXTLINE(misc-unused-using-decls)
using asserts::ASSERTION;

#if ASSERT_DETAIL_IS_CLANG || ASSERT_DETAIL_IS_GCC
 // Macro mapping utility by William Swanson https://github.com/swansontec/map-macro/blob/master/map.h
 #define ASSERT_DETAIL_EVAL0(...) __VA_ARGS__
 #define ASSERT_DETAIL_EVAL1(...) ASSERT_DETAIL_EVAL0(ASSERT_DETAIL_EVAL0(ASSERT_DETAIL_EVAL0(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL2(...) ASSERT_DETAIL_EVAL1(ASSERT_DETAIL_EVAL1(ASSERT_DETAIL_EVAL1(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL3(...) ASSERT_DETAIL_EVAL2(ASSERT_DETAIL_EVAL2(ASSERT_DETAIL_EVAL2(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL4(...) ASSERT_DETAIL_EVAL3(ASSERT_DETAIL_EVAL3(ASSERT_DETAIL_EVAL3(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL(...)  ASSERT_DETAIL_EVAL4(ASSERT_DETAIL_EVAL4(ASSERT_DETAIL_EVAL4(__VA_ARGS__)))
 #define ASSERT_DETAIL_MAP_END(...)
 #define ASSERT_DETAIL_MAP_OUT
 #define ASSERT_DETAIL_MAP_COMMA ,
 #define ASSERT_DETAIL_MAP_GET_END2() 0, ASSERT_DETAIL_MAP_END
 #define ASSERT_DETAIL_MAP_GET_END1(...) ASSERT_DETAIL_MAP_GET_END2
 #define ASSERT_DETAIL_MAP_GET_END(...) ASSERT_DETAIL_MAP_GET_END1
 #define ASSERT_DETAIL_MAP_NEXT0(test, next, ...) next ASSERT_DETAIL_MAP_OUT
 #define ASSERT_DETAIL_MAP_NEXT1(test, next) ASSERT_DETAIL_MAP_NEXT0(test, next, 0)
 #define ASSERT_DETAIL_MAP_NEXT(test, next)  ASSERT_DETAIL_MAP_NEXT1(ASSERT_DETAIL_MAP_GET_END test, next)
 #define ASSERT_DETAIL_MAP0(f, x, peek, ...) f(x) ASSERT_DETAIL_MAP_NEXT(peek, ASSERT_DETAIL_MAP1)(f, peek, __VA_ARGS__)
 #define ASSERT_DETAIL_MAP1(f, x, peek, ...) f(x) ASSERT_DETAIL_MAP_NEXT(peek, ASSERT_DETAIL_MAP0)(f, peek, __VA_ARGS__)
 #define ASSERT_DETAIL_MAP_LIST_NEXT1(test, next) ASSERT_DETAIL_MAP_NEXT0(test, ASSERT_DETAIL_MAP_COMMA next, 0)
 #define ASSERT_DETAIL_MAP_LIST_NEXT(test, next)  ASSERT_DETAIL_MAP_LIST_NEXT1(ASSERT_DETAIL_MAP_GET_END test, next)
 #define ASSERT_DETAIL_MAP_LIST0(f, x, peek, ...) \
                                   f(x) ASSERT_DETAIL_MAP_LIST_NEXT(peek, ASSERT_DETAIL_MAP_LIST1)(f, peek, __VA_ARGS__)
 #define ASSERT_DETAIL_MAP_LIST1(f, x, peek, ...) \
                                   f(x) ASSERT_DETAIL_MAP_LIST_NEXT(peek, ASSERT_DETAIL_MAP_LIST0)(f, peek, __VA_ARGS__)
 #define ASSERT_DETAIL_MAP(f, ...) ASSERT_DETAIL_EVAL(ASSERT_DETAIL_MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))
#else
 // https://stackoverflow.com/a/29474124/15675011
 #define ASSERT_DETAIL_PLUS_TEXT_(x,y) x ## y
 #define ASSERT_DETAIL_PLUS_TEXT(x, y) ASSERT_DETAIL_PLUS_TEXT_(x, y)
 #define ASSERT_DETAIL_ARG_1(_1, ...) _1
 #define ASSERT_DETAIL_ARG_2(_1, _2, ...) _2
 #define ASSERT_DETAIL_ARG_3(_1, _2, _3, ...) _3
 #define ASSERT_DETAIL_ARG_40( _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, \
                 _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, \
                 _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, \
                 _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, \
                 ...) _39
 #define ASSERT_DETAIL_OTHER_1(_1, ...) __VA_ARGS__
 #define ASSERT_DETAIL_OTHER_3(_1, _2, _3, ...) __VA_ARGS__
 #define ASSERT_DETAIL_EVAL0(...) __VA_ARGS__
 #define ASSERT_DETAIL_EVAL1(...) ASSERT_DETAIL_EVAL0(ASSERT_DETAIL_EVAL0(ASSERT_DETAIL_EVAL0(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL2(...) ASSERT_DETAIL_EVAL1(ASSERT_DETAIL_EVAL1(ASSERT_DETAIL_EVAL1(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL3(...) ASSERT_DETAIL_EVAL2(ASSERT_DETAIL_EVAL2(ASSERT_DETAIL_EVAL2(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL4(...) ASSERT_DETAIL_EVAL3(ASSERT_DETAIL_EVAL3(ASSERT_DETAIL_EVAL3(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL(...) ASSERT_DETAIL_EVAL4(ASSERT_DETAIL_EVAL4(ASSERT_DETAIL_EVAL4(__VA_ARGS__)))
 #define ASSERT_DETAIL_EXPAND(x) x
 #define ASSERT_DETAIL_MAP_SWITCH(...)\
     ASSERT_DETAIL_EXPAND(ASSERT_DETAIL_ARG_40(__VA_ARGS__, 2, 2, 2, 2, 2, 2, 2, 2, 2,\
             2, 2, 2, 2, 2, 2, 2, 2, 2, 2,\
             2, 2, 2, 2, 2, 2, 2, 2, 2,\
             2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0))
 #define ASSERT_DETAIL_MAP_A(...) ASSERT_DETAIL_PLUS_TEXT(ASSERT_DETAIL_MAP_NEXT_, \
                                            ASSERT_DETAIL_MAP_SWITCH(0, __VA_ARGS__)) (ASSERT_DETAIL_MAP_B, __VA_ARGS__)
 #define ASSERT_DETAIL_MAP_B(...) ASSERT_DETAIL_PLUS_TEXT(ASSERT_DETAIL_MAP_NEXT_, \
                                            ASSERT_DETAIL_MAP_SWITCH(0, __VA_ARGS__)) (ASSERT_DETAIL_MAP_A, __VA_ARGS__)
 #define ASSERT_DETAIL_MAP_CALL(fn, Value) ASSERT_DETAIL_EXPAND(fn(Value))
 #define ASSERT_DETAIL_MAP_OUT
 #define ASSERT_DETAIL_MAP_NEXT_2(...)\
     ASSERT_DETAIL_MAP_CALL(ASSERT_DETAIL_EXPAND(ASSERT_DETAIL_ARG_2(__VA_ARGS__)), \
     ASSERT_DETAIL_EXPAND(ASSERT_DETAIL_ARG_3(__VA_ARGS__))) \
     ASSERT_DETAIL_EXPAND(ASSERT_DETAIL_ARG_1(__VA_ARGS__)) \
     ASSERT_DETAIL_MAP_OUT \
     (ASSERT_DETAIL_EXPAND(ASSERT_DETAIL_ARG_2(__VA_ARGS__)), ASSERT_DETAIL_EXPAND(ASSERT_DETAIL_OTHER_3(__VA_ARGS__)))
 #define ASSERT_DETAIL_MAP_NEXT_0(...)
 #define ASSERT_DETAIL_MAP(...)    ASSERT_DETAIL_EVAL(ASSERT_DETAIL_MAP_A(__VA_ARGS__))
#endif

#define ASSERT_DETAIL_STRINGIFY(x) #x,
#define ASSERT_DETAIL_COMMA ,

// Church boolean
#define ASSERT_DETAIL_IF(b) ASSERT_DETAIL_IF_##b
#define ASSERT_DETAIL_IF_true(t,...) t
#define ASSERT_DETAIL_IF_false(t,f,...) f

#if ASSERT_DETAIL_IS_CLANG || ASSERT_DETAIL_IS_GCC
 // Extra set of parentheses here because clang treats __extension__ as a low-precedence unary operator which interferes
 // with decltype(auto) in an expression like decltype(auto) x = __extension__ ({...}).y;
 #define ASSERT_DETAIL_STMTEXPR(B, R) (__extension__ ({ B R }))
 #define ASSERT_DETAIL_WARNING_PRAGMA _Pragma("GCC diagnostic ignored \"-Wparentheses\"")
 #define ASSERT_DETAIL_PFUNC_INVOKER_VALUE ASSERT_DETAIL_PFUNC
 #define ASSERT_DETAIL_STATIC_CAST_TO_BOOL(x) static_cast<bool>(x)
#else
 #define ASSERT_DETAIL_STMTEXPR(B, R) [&] (const char* assert_detail_msvc_pfunc) { B return R } (ASSERT_DETAIL_PFUNC)
 #define ASSERT_DETAIL_WARNING_PRAGMA
 #define ASSERT_DETAIL_PFUNC_INVOKER_VALUE nullptr
 #define ASSERT_DETAIL_STATIC_CAST_TO_BOOL(x) asserts::detail::static_cast_to_bool(x)
 namespace asserts::detail {
     template<typename T> bool static_cast_to_bool(T&& t) {
         return static_cast<bool>(t);
     }
 }
#endif

#if ASSERT_DETAIL_IS_GCC
 // __VA_OPT__ needed for GCC, https://gcc.gnu.org/bugzilla/show_bug.cgi?id=44317
 #define ASSERT_DETAIL_VA_ARGS(...) __VA_OPT__(,) __VA_ARGS__
#else
 // clang properly eats the comma with ##__VA_ARGS__
 #define ASSERT_DETAIL_VA_ARGS(...) , ##__VA_ARGS__
#endif

// __PRETTY_FUNCTION__ used because __builtin_FUNCTION() used in source_location (like __FUNCTION__) is just the method
// name, not signature
#define ASSERT_DETAIL_STATIC_DATA(name, type, expr_str, ...) \
                                  /* extra string here because of extra comma from map, also serves as terminator */ \
                                  /* ASSERT_DETAIL_STRINGIFY ASSERT_DETAIL_VA_ARGS because msvc */ \
                                  static constexpr const char* const assert_detail_arg_strings[] = { \
                                    ASSERT_DETAIL_MAP(ASSERT_DETAIL_STRINGIFY ASSERT_DETAIL_VA_ARGS(__VA_ARGS__)) "" \
                                  }; \
                                  static constexpr asserts::detail::assert_static_parameters assert_detail_params = { \
                                    name ASSERT_DETAIL_COMMA \
                                    type ASSERT_DETAIL_COMMA \
                                    expr_str ASSERT_DETAIL_COMMA \
                                    ASSERT_DETAIL_PFUNC_INVOKER_VALUE ASSERT_DETAIL_COMMA \
                                    assert_detail_arg_strings ASSERT_DETAIL_COMMA \
                                  };

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
// Note: asserts::detail::expression_decomposer(asserts::detail::expression_decomposer{} << expr) done for ternary
#if ASSERT_DETAIL_IS_MSVC
 #define ASSERT_DETAIL_MSVC_PRETTY_FUNCTION_ARG ,asserts::detail::msvc_pretty_function_wrapper{assert_detail_msvc_pfunc}
#else
 #define ASSERT_DETAIL_MSVC_PRETTY_FUNCTION_ARG
#endif
#if ASSERT_DETAIL_IS_CLANG // -Wall in clang
 #define ASSERT_DETAIL_IGNORE_UNUSED_VALUE _Pragma("GCC diagnostic ignored \"-Wunused-value\"")
#else
 #define ASSERT_DETAIL_IGNORE_UNUSED_VALUE
#endif
#define ASSERT_INVOKE(expr, doreturn, name, type, failaction, ...) \
        ASSERT_DETAIL_IGNORE_UNUSED_VALUE \
        ASSERT_DETAIL_STMTEXPR( \
          ASSERT_DETAIL_WARNING_PRAGMA \
          auto assert_detail_decomposer = \
                             asserts::detail::expression_decomposer(asserts::detail::expression_decomposer{} << expr); \
          decltype(auto) assert_detail_value = assert_detail_decomposer.get_value(); \
          constexpr bool assert_detail_ret_lhs = assert_detail_decomposer.ret_lhs(); \
          /* For *some* godforsaken reason static_cast<bool> causes an ICE in MSVC here. Something very specific */ \
          /* about casting a decltype(auto) value inside a lambda. Workaround is to put it in a wrapper. */ \
          /* https://godbolt.org/z/Kq8Wb6q5j https://godbolt.org/z/nMnqnsMYx */ \
          if(ASSERT_DETAIL_STRONG_EXPECT(!ASSERT_DETAIL_STATIC_CAST_TO_BOOL(assert_detail_value), 0)) { \
            failaction \
            ASSERT_DETAIL_STATIC_DATA(name, asserts::assert_type::type, #expr, __VA_ARGS__) \
            if constexpr(sizeof assert_detail_decomposer > 32) { \
              process_assert_fail(assert_detail_decomposer, &assert_detail_params \
                                           ASSERT_DETAIL_VA_ARGS(__VA_ARGS__) ASSERT_DETAIL_MSVC_PRETTY_FUNCTION_ARG); \
            } else { \
              /* std::move it to assert_fail_m, will be moved back to r */ \
              auto assert_detail_r = process_assert_fail_m(std::move(assert_detail_decomposer), &assert_detail_params \
                                           ASSERT_DETAIL_VA_ARGS(__VA_ARGS__) ASSERT_DETAIL_MSVC_PRETTY_FUNCTION_ARG); \
              /* can't move-assign back to decomposer if it holds reference members */ \
              assert_detail_decomposer.compl expression_decomposer(); /* NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move) */ \
              new (&assert_detail_decomposer) asserts::detail::expression_decomposer(std::move(assert_detail_r)); \
            } \
          }, \
          /* Note: std::launder needed in 17 in case of placement new / move shenanigans above */ \
          /* https://timsong-cpp.github.io/cppwp/n4659/basic.life#8.3 */ \
          /* Note: Somewhat relying on this call being inlined so inefficiency is eliminated */ \
          asserts::detail::get_expression_return_value <doreturn ASSERT_DETAIL_COMMA \
            assert_detail_ret_lhs ASSERT_DETAIL_COMMA std::is_lvalue_reference<decltype(assert_detail_value)>::value> \
              (assert_detail_value, *std::launder(&assert_detail_decomposer)); \
        ) ASSERT_DETAIL_IF(doreturn)(.value,)

#ifdef NDEBUG
 #define ASSERT_DETAIL_NDEBUG_ACTION ASSERT_DETAIL_UNREACHABLE;
#else
 #define ASSERT_DETAIL_NDEBUG_ACTION
#endif

#define ASSERT(expr, ...) ASSERT_INVOKE(expr, true, "ASSERT", assertion, ASSERT_DETAIL_NDEBUG_ACTION, __VA_ARGS__)

#ifdef ASSERT_LOWERCASE
 #ifdef assert
  #undef assert
 #endif
 #define assert(expr, ...) ASSERT_INVOKE(expr, true, "assert", assertion, ASSERT_DETAIL_NDEBUG_ACTION, __VA_ARGS__)
#endif

#define VERIFY(expr, ...) ASSERT_INVOKE(expr, true, "VERIFY", verify, , __VA_ARGS__)

#ifndef NDEBUG
 #define CHECK(expr, ...) ASSERT_INVOKE(expr, false, "CHECK", check, , __VA_ARGS__)
#else
 // omitting the expression could cause unused variable warnings, surpressing for now
 // TODO: Is this the right design decision? Re-evaluated whether PHONY_USE should be used here or internally at all
 #define CHECK(expr, ...) ASSERT_DETAIL_PHONY_USE(expr)
#endif

#ifndef ASSERT_DETAIL_IS_CPP // keep macros for the .cpp
 #undef ASSERT_DETAIL_IS_CLANG
 #undef ASSERT_DETAIL_IS_GCC
 #undef ASSERT_DETAIL_IS_MSVC
 #undef ASSERT_DETAIL_ATTR_COLD
 #undef ASSERT_DETAIL_ATTR_NOINLINE
 #undef ASSERT_DETAIL_PRIMITIVE_ASSERT
#endif

#endif
