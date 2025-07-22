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

LIBASSERT_BEGIN_NAMESPACE
namespace detail {
    // Lots of boilerplate
    // std:: implementations don't allow two separate types for lhs/rhs
    // Note: is this macro potentially bad when it comes to debugging(?)
    namespace ops {
        #define LIBASSERT_GEN_OP_BOILERPLATE(name, op) struct name { \
            static constexpr std::string_view op_string = #op; \
            template<typename A, typename B> \
            [[nodiscard]] \
            constexpr decltype(auto) operator()(A&& lhs, B&& rhs) const { /* no need to forward ints */ \
                return std::forward<A>(lhs) op std::forward<B>(rhs); \
            } \
        }
        #define LIBASSERT_GEN_OP_BOILERPLATE_SPECIAL(name, op, cmp) struct name { \
            static constexpr std::string_view op_string = #op; \
            template<typename A, typename B> \
            [[nodiscard]] \
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
             [[nodiscard]]
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

        inline constexpr bool ret_lhs(std::string_view op_string) {
            // return LHS for the following types;
            return op_string == "=="
                || op_string == "!="
                || op_string == "<"
                || op_string == ">"
                || op_string == "<="
                || op_string == ">="
                || op_string == "&&"
                || op_string == "||";
            // don't return LHS for << >> & ^ | and all assignments
        }
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

    // Ownership logic:
    //  One of two things can happen to this class
    //   - Either it is composed with another operation
    //         + The value of the subexpression represented by this is computed (either get_value()
    //           or operator bool), either A& or C()(a, b)
    //      + Or, just the lhs is moved B is nothing
    //   - Or this class represents the whole expression
    //      + The value is computed (either A& or C()(a, b))
    //      + a and b are referenced freely
    //      + Either the value is taken or a is moved out
    // Ensuring the value is only computed once is left to the assert handler

    // expression_decomposer is a template of the left hand type, right hand type, and the operator (as a function
    // object)
    // Specialized for
    //   empty, default-constructed base-case
    //   just a left-hand value
    //   full representation of a binary expression

    template<typename A = nothing, typename B = nothing, typename C = nothing>
    struct expression_decomposer;

    // empty expression_decomposer
    template<>
    struct expression_decomposer<nothing, nothing, nothing> {
        explicit constexpr expression_decomposer() = default;

        template<typename O> [[nodiscard]] constexpr auto operator<<(O&& operand) && {
            return expression_decomposer<O, nothing, nothing>(std::forward<O>(operand));
        }
    };

    #if __cplusplus >= 202002L
    #define LIBASSERT_GEN_OP_BOILERPLATE_SPACESHIP LIBASSERT_GEN_OP_BOILERPLATE(ops::spaceship, <=>)
    #else
    #define LIBASSERT_GEN_OP_BOILERPLATE_SPACESHIP
    #endif

    #ifdef LIBASSERT_DECOMPOSE_BINARY_LOGICAL
    #define LIBASSERT_GEN_OP_BOILERPLATE_BINARY_LOGICAL \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::land, &&) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::lor, ||)
    #else
    #define LIBASSERT_GEN_OP_BOILERPLATE_BINARY_LOGICAL
    #endif

    #define LIBASSERT_DO_GEN_OP_BOILERPLATE \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::shl, <<) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::shr, >>) \
        LIBASSERT_GEN_OP_BOILERPLATE_SPACESHIP \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::eq, ==) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::neq, !=) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::gt, >) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::lt, <) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::gteq, >=) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::lteq, <=) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::band, &) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::bxor, ^) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::bor, |) \
        LIBASSERT_GEN_OP_BOILERPLATE_BINARY_LOGICAL \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::assign, =) /* NOLINT(cppcoreguidelines-c-copy-assignment-signature, misc-unconventional-assign-operator) */ \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::add_assign, +=) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::sub_assign, -=) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::mul_assign, *=) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::div_assign, /=) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::mod_assign, %=) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::shl_assign, <<=) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::shr_assign, >>=) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::band_assign, &=) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::bxor_assign, ^=) \
        LIBASSERT_GEN_OP_BOILERPLATE(ops::bor_assign, |=)

    // just a left-hand value
    template<typename A>
    struct expression_decomposer<A, nothing, nothing> {
        A a;
        explicit constexpr expression_decomposer() = delete;
        template<typename U, typename std::enable_if_t<!isa<U, expression_decomposer>, int> = 0>
        // NOLINTNEXTLINE(bugprone-forwarding-reference-overload) // TODO
        explicit constexpr expression_decomposer(U&& _a) : a(std::forward<U>(_a)) {}
        [[nodiscard]]
        constexpr A& get_value() {
            return a;
        }
        [[nodiscard]]
        constexpr operator bool() { // for ternary support
            return static_cast<bool>(get_value());
        }
        // return true if the lhs should be returned, false if full computed value should be
        [[nodiscard]]
        constexpr bool ret_lhs() {
            // if there is no top-level binary operation, A is the only thing that can be returned
            return true;
        }
        [[nodiscard]]
        constexpr decltype(auto) take_lhs() { // should only be called if ret_lhs() == true
            // This use of std::forward may look surprising as it's not the traditional use of forwarding a T&& in a
            // template<typename T>. None the less, this still does what we want regarding forwarding lvalues as lvalues
            // and rvalues as rvalues.
            return std::forward<A>(a);
        }
        #define LIBASSERT_GEN_OP_BOILERPLATE(functor, op) \
        template<typename O> [[nodiscard]] constexpr auto operator op(O&& operand) && { \
            return expression_decomposer<A, O, functor>(std::forward<A>(a), std::forward<O>(operand)); \
        }
        LIBASSERT_DO_GEN_OP_BOILERPLATE
        #undef LIBASSERT_GEN_OP_BOILERPLATE
    };

    template<typename T> T deduce_type(T&&);

    template<typename A, typename B, typename C>
    struct expression_decomposer {
        static_assert(!is_nothing<A> && !is_nothing<B> && !is_nothing<C>);
        A a;
        B b;
        explicit constexpr expression_decomposer() = delete;
        template<typename U, typename V>
        explicit constexpr expression_decomposer(U&& _a, V&& _b) : a(std::forward<U>(_a)), b(std::forward<V>(_b)) {}
        [[nodiscard]]
        constexpr decltype(auto) get_value() {
            return C()(a, b);
        }
        [[nodiscard]]
        constexpr operator bool() { // for ternary support
            return static_cast<bool>(get_value());
        }
        // return true if the lhs should be returned, false if full computed value should be
        [[nodiscard]]
        constexpr bool ret_lhs() {
            return ops::ret_lhs(C::op_string);
        }
        [[nodiscard]]
        constexpr decltype(auto) take_lhs() { // should only be called if ret_lhs() == true
             // This use of std::forward may look surprising but it's correct
            return std::forward<A>(a);
        }
        #define LIBASSERT_GEN_OP_BOILERPLATE(functor, op) \
        template<typename O> [[nodiscard]] constexpr auto operator op(O&& operand) && { \
            static_assert(!is_nothing<A>); \
            using V = decltype(deduce_type(get_value())); /* deduce_type turns T&& into T while leaving T& as T& */ \
            return expression_decomposer<V, O, functor>(get_value(), std::forward<O>(operand)); \
        }
        LIBASSERT_DO_GEN_OP_BOILERPLATE
        #undef LIBASSERT_GEN_OP_BOILERPLATE
    };

    #undef LIBASSERT_GEN_OP_BOILERPLATE_SPACESHIP
    #undef LIBASSERT_GEN_OP_BOILERPLATE_BINARY_LOGICAL
    #undef LIBASSERT_DO_GEN_OP_BOILERPLATE

    // for ternary support
    template<typename U>
    expression_decomposer(U&&) -> expression_decomposer<U>;

    expression_decomposer() -> expression_decomposer<>;
}
LIBASSERT_END_NAMESPACE

#endif
