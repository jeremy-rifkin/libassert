#ifndef LIBASSERT_ASSERT_MACROS_HPP
#define LIBASSERT_ASSERT_MACROS_HPP

// Copyright (c) 2021-2025 Jeremy Rifkin under the MIT license
// https://github.com/jeremy-rifkin/libassert

#include <libassert/platform.hpp>

#include <string_view>

#if LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC || !LIBASSERT_NON_CONFORMANT_MSVC_PREPROCESSOR
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
 #define LIBASSERT_MAP_SWITCH(...) \
     LIBASSERT_EXPAND(LIBASSERT_ARG_40(__VA_ARGS__, 2, 2, 2, 2, 2, 2, 2, 2, 2, \
             2, 2, 2, 2, 2, 2, 2, 2, 2, 2, \
             2, 2, 2, 2, 2, 2, 2, 2, 2, \
             2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0))
 #define LIBASSERT_MAP_A(...) LIBASSERT_PLUS_TEXT(LIBASSERT_MAP_NEXT_, \
                                            LIBASSERT_MAP_SWITCH(0, __VA_ARGS__)) (LIBASSERT_MAP_B, __VA_ARGS__)
 #define LIBASSERT_MAP_B(...) LIBASSERT_PLUS_TEXT(LIBASSERT_MAP_NEXT_, \
                                            LIBASSERT_MAP_SWITCH(0, __VA_ARGS__)) (LIBASSERT_MAP_A, __VA_ARGS__)
 #define LIBASSERT_MAP_CALL(fn, Value) LIBASSERT_EXPAND(fn(Value))
 #define LIBASSERT_MAP_OUT
 #define LIBASSERT_MAP_NEXT_2(...) \
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

#if LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC
 #if LIBASSERT_IS_GCC
  #define LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA \
     _Pragma("GCC diagnostic ignored \"-Wparentheses\"") \
     _Pragma("GCC diagnostic ignored \"-Wuseless-cast\"") // #49
 #else
  #define LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA \
     _Pragma("GCC diagnostic ignored \"-Wparentheses\"") \
     _Pragma("GCC diagnostic ignored \"-Woverloaded-shift-op-parentheses\"")
 #endif
#else
 // NOTE: If this is changed, LIBASSERT_ASSERT_STMT_EXPR below will need to be updated for MSVC
 #define LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA
#endif

LIBASSERT_BEGIN_NAMESPACE
    inline void ERROR_ASSERTION_FAILURE_IN_CONSTEXPR_CONTEXT() {
        // This non-constexpr method is called from an assertion in a constexpr context if a failure occurs. It is
        // intentionally a no-op.
    }
LIBASSERT_END_NAMESPACE

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

#define LIBASSERT_BREAKPOINT_IF_DEBUGGING() \
    do \
        if(libassert::is_debugger_present()) { \
            LIBASSERT_BREAKPOINT(); \
        } \
    while(0)

#ifdef LIBASSERT_BREAK_ON_FAIL
 #define LIBASSERT_BREAKPOINT_IF_DEBUGGING_ON_FAIL() LIBASSERT_BREAKPOINT_IF_DEBUGGING()
#else
 #define LIBASSERT_BREAKPOINT_IF_DEBUGGING_ON_FAIL()
#endif

// ifndef here to allow a library like rsl-test to use this as a customization point for fancy instrumentation
#ifndef LIBASSERT_ASSERT_MAIN_BODY
 #define LIBASSERT_ASSERT_MAIN_BODY(expr, name, type, failaction, decomposer_name, condition_value, pretty_function_arg, ...) \
     if(LIBASSERT_STRONG_EXPECT(!(condition_value), 0)) { \
         libassert::ERROR_ASSERTION_FAILURE_IN_CONSTEXPR_CONTEXT(); \
         LIBASSERT_BREAKPOINT_IF_DEBUGGING_ON_FAIL(); \
         failaction \
         LIBASSERT_STATIC_DATA(name, libassert::assert_type::type, #expr, __VA_ARGS__) \
         libassert::detail::process_assert_fail( \
             decomposer_name, \
             libassert_params \
             LIBASSERT_VA_ARGS(__VA_ARGS__) pretty_function_arg \
         ); \
     }
#endif
#ifndef LIBASSERT_PANIC_MAIN_BODY
 #define LIBASSERT_PANIC_MAIN_BODY(name, type, pretty_function_arg, ...) \
     libassert::ERROR_ASSERTION_FAILURE_IN_CONSTEXPR_CONTEXT(); \
     LIBASSERT_BREAKPOINT_IF_DEBUGGING_ON_FAIL(); \
     LIBASSERT_STATIC_DATA(name, libassert::assert_type::type, "", __VA_ARGS__) \
     libassert::detail::process_panic( \
         libassert_params \
         LIBASSERT_VA_ARGS(__VA_ARGS__) pretty_function_arg \
     );
#endif

#define LIBASSERT_INVOKE(expr, name, type, failaction, ...) \
    do { \
        LIBASSERT_WARNING_PRAGMA_PUSH \
        LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA \
        auto libassert_decomposer = libassert::detail::expression_decomposer( \
            libassert::detail::expression_decomposer{} << expr \
        ); \
        LIBASSERT_WARNING_PRAGMA_POP \
        LIBASSERT_ASSERT_MAIN_BODY( \
            expr, \
            name, \
            type, \
            failaction, \
            libassert_decomposer, \
            static_cast<bool>(libassert_decomposer.get_value()), \
            LIBASSERT_PRETTY_FUNCTION_ARG, \
            __VA_ARGS__ \
        ) \
    } while(0) \

#define LIBASSERT_INVOKE_PANIC(name, type, ...) \
    do { \
        LIBASSERT_PANIC_MAIN_BODY( \
            name, \
            type, \
            LIBASSERT_PRETTY_FUNCTION_ARG, \
            __VA_ARGS__ \
        ) \
    } while(0)

#if LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC
 // Extra set of parentheses here because clang treats __extension__ as a low-precedence unary operator which interferes
 // with decltype(auto) in an expression like decltype(auto) x = __extension__ ({...}).y;
 // Surpress warnings here because of a clang bug: https://github.com/llvm/llvm-project/issues/63897
 #define LIBASSERT_ASSERT_STMT_EXPR(B, R) (__extension__ ({ \
        LIBASSERT_WARNING_PRAGMA_PUSH \
        LIBASSERT_EXPRESSION_DECOMP_WARNING_PRAGMA \
        B \
        LIBASSERT_WARNING_PRAGMA_POP \
        R \
    }))
 #define LIBASSERT_STATIC_CAST_TO_BOOL(x) static_cast<bool>(x)
#else
 #define LIBASSERT_ASSERT_STMT_EXPR(B, R) [&](const char* libassert_msvc_pfunc) { B return R }(LIBASSERT_PFUNC)
 // Workaround for msvc bug
 #define LIBASSERT_STATIC_CAST_TO_BOOL(x) libassert::detail::static_cast_to_bool(x)
 LIBASSERT_BEGIN_NAMESPACE
 namespace detail {
     template<typename T> constexpr bool static_cast_to_bool(T&& t) {
         return static_cast<bool>(t);
     }
 }
 LIBASSERT_END_NAMESPACE
#endif

#define LIBASSERT_INVOKE_VAL(expr, check_expression, name, type, failaction, ...) \
    LIBASSERT_ASSERT_STMT_EXPR( \
        auto libassert_decomposer = libassert::detail::expression_decomposer( \
            libassert::detail::expression_decomposer{} << expr \
        ); \
        decltype(auto) libassert_value = libassert_decomposer.get_value(); \
        constexpr bool libassert_ret_lhs = libassert_decomposer.ret_lhs(); \
        if constexpr(check_expression) { \
            LIBASSERT_ASSERT_MAIN_BODY( \
                expr, \
                name, \
                type, \
                failaction, \
                libassert_decomposer, \
                /* For *some* godforsaken reason static_cast<bool> causes an ICE in MSVC here. Something very */ \
                /* specific about casting a decltype(auto) value inside a lambda. Workaround is to put it in a */ \
                /* wrapper. https://godbolt.org/z/Kq8Wb6q5j https://godbolt.org/z/nMnqnsMYx */ \
                LIBASSERT_STATIC_CAST_TO_BOOL(libassert_value), \
                LIBASSERT_INVOKE_VAL_PRETTY_FUNCTION_ARG, \
                __VA_ARGS__ \
            ) \
        }, \
        /* Note: Relying on this call being inlined so inefficiency is eliminated */ \
        libassert::detail::get_expression_return_value< \
            libassert_ret_lhs LIBASSERT_COMMA \
            std::is_lvalue_reference_v<decltype(libassert_value)> \
        >(libassert_value, libassert_decomposer); \
    ).value

#ifdef NDEBUG
 #define LIBASSERT_ASSUME_ACTION LIBASSERT_UNREACHABLE_CALL();
#else
 #define LIBASSERT_ASSUME_ACTION
#endif
#define LIBASSERT_NOP_ACTION

// assertion macros

// Debug assert
#ifndef NDEBUG
 #define LIBASSERT_DEBUG_ASSERT(expr, ...) LIBASSERT_INVOKE(expr, "DEBUG_ASSERT", debug_assertion, LIBASSERT_NOP_ACTION, __VA_ARGS__)
#else
 #define LIBASSERT_DEBUG_ASSERT(expr, ...) (void)0
#endif

// Assert
#define LIBASSERT_ASSERT(expr, ...) LIBASSERT_INVOKE(expr, "ASSERT", assertion, LIBASSERT_NOP_ACTION, __VA_ARGS__)
// lowercase version intentionally done outside of the include guard here

// Assume
#define LIBASSERT_ASSUME(expr, ...) LIBASSERT_INVOKE(expr, "ASSUME", assumption, LIBASSERT_ASSUME_ACTION, __VA_ARGS__)

// Panic
#define LIBASSERT_PANIC(...) LIBASSERT_INVOKE_PANIC("PANIC", panic, __VA_ARGS__)

// Unreachable
#ifndef NDEBUG
 #define LIBASSERT_UNREACHABLE(...) LIBASSERT_INVOKE_PANIC("UNREACHABLE", unreachable, __VA_ARGS__)
#else
 #define LIBASSERT_UNREACHABLE(...) LIBASSERT_UNREACHABLE_CALL()
#endif

// value variants

#ifndef NDEBUG
 #define LIBASSERT_DEBUG_ASSERT_VAL(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, "DEBUG_ASSERT_VAL", debug_assertion, LIBASSERT_NOP_ACTION, __VA_ARGS__)
#else
 #define LIBASSERT_DEBUG_ASSERT_VAL(expr, ...) LIBASSERT_INVOKE_VAL(expr, false, "DEBUG_ASSERT_VAL", debug_assertion, LIBASSERT_NOP_ACTION, __VA_ARGS__)
#endif

#define LIBASSERT_ASSUME_VAL(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, "ASSUME_VAL", assumption, LIBASSERT_ASSUME_ACTION, __VA_ARGS__)

#define LIBASSERT_ASSERT_VAL(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, "ASSERT_VAL", assertion, LIBASSERT_NOP_ACTION, __VA_ARGS__)

// non-prefixed versions

#ifndef LIBASSERT_PREFIX_ASSERTIONS
 #if LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC || !LIBASSERT_NON_CONFORMANT_MSVC_PREPROCESSOR
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
  #define debug_assert(expr, ...) LIBASSERT_INVOKE(expr, "debug_assert", debug_assertion, LIBASSERT_NOP_ACTION, __VA_ARGS__)
 #else
  #define debug_assert(expr, ...) (void)0
 #endif
#endif

#ifdef LIBASSERT_LOWERCASE
 #ifndef NDEBUG
  #define debug_assert_val(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, "debug_assert_val", debug_assertion, LIBASSERT_NOP_ACTION, __VA_ARGS__)
 #else
  #define debug_assert_val(expr, ...) LIBASSERT_INVOKE_VAL(expr, false, "debug_assert_val", debug_assertion, LIBASSERT_NOP_ACTION, __VA_ARGS__)
 #endif
#endif

#ifdef LIBASSERT_LOWERCASE
 #define assert_val(expr, ...) LIBASSERT_INVOKE_VAL(expr, true, "assert_val", assertion, LIBASSERT_NOP_ACTION, __VA_ARGS__)
#endif

#endif // LIBASSERT_HPP

// Intentionally done outside the include guard. Libc++ leaks `assert` (among other things), so the include for
// assert.hpp should go after other includes when using -DLIBASSERT_LOWERCASE.
#ifdef LIBASSERT_LOWERCASE
 #ifdef assert
  #undef assert
 #endif
 #ifndef NDEBUG
  #define assert(expr, ...) LIBASSERT_INVOKE(expr, "assert", assertion, LIBASSERT_NOP_ACTION, __VA_ARGS__)
 #else
  #define assert(expr, ...) LIBASSERT_INVOKE(expr, "assert", assertion, LIBASSERT_NOP_ACTION, __VA_ARGS__)
 #endif
#endif
