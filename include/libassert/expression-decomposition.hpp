#ifndef LIBASSERT_EXPRESSION_DECOMPOSITION_HPP
#define LIBASSERT_EXPRESSION_DECOMPOSITION_HPP

#include <string_view>
#include <type_traits>
#include <utility>

#include <libassert/platform.hpp>
#include <libassert/utilities.hpp>

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

#endif
