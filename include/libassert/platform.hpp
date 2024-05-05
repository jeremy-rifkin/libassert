#ifndef LIBASSERT_PLATFORM_HPP
#define LIBASSERT_PLATFORM_HPP

// Copyright (c) 2021-2024 Jeremy Rifkin under the MIT license
// https://github.com/jeremy-rifkin/libassert



// =====================================================================================================================
// || Preprocessor stuff                                                                                              ||
// =====================================================================================================================

// Set the C++ version number based on if we are on a dumb compiler like MSVC or not.
#ifdef _MSVC_LANG
    #define LIBASSERT_CPLUSPLUS _MSVC_LANG
#else
    #define LIBASSERT_CPLUSPLUS __cplusplus
#endif


// Validate that we are using a C++17 or newer compiler.
#if LIBASSERT_CPLUSPLUS < 201703L
    #error "libassert requires C++17 or newer"
#endif


// Here we assign the current C++ standard number to LIBASSERT_STD_VER if it is not already defined.
// Currently this check assumes that the base version is C++17 and if the version number is greater than
// 202303L then we assume that we are using C++26. Currently, not every compiler has a defined value for C++26
// and to some degree even checking for 202302L is not 100% reliable as some compilers have chosen to not actually define
// the version number for C++23 until they are fully compliant. Still this is a rare case and should be fine here.
// TODO: Once C++26 is fully released we should update this so the else instead uses the proper value.
#ifndef LIBASSERT_STD_VER
    #if LIBASSERT_CPLUSPLUS <= 201703L
        #define LIBASSERT_STD_VER 17
    #elif LIBASSERT_CPLUSPLUS <= 202002L
        #define LIBASSERT_STD_VER 20
    #elif LIBASSERT_CPLUSPLUS <= 202302L
        #define LIBASSERT_STD_VER 23
    #else
        #define LIBASSERT_STD_VER 26 // If our version number is higher than 202303L assume we are using C++26
    #endif // LIBASSERT_CPLUSPLUS
#endif // LIBASSERT_STD_VER


///
/// Detect compiler versions.
///


#if defined(__clang__) && !defined(__ibmxl__)
    #define LIBASSERT_IS_CLANG 1
    #define LIBASSERT_CLANG_VERSION (__clang_major__ * 100 + __clang_minor__)
#else
    #define LIBASSERT_IS_CLANG 0
    #define LIBASSERT_CLANG_VERSION 0
#endif


#if (defined(__GNUC__) || defined(__GNUG__)) && !defined(__clang__) && !defined(__INTEL_COMPILER)
    #define LIBASSERT_IS_GCC 1
    #define LIBASSERT_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
    #define LIBASSERT_IS_GCC 0
    #define LIBASSERT_GCC_VERSION 0
#endif


#if defined(_MSC_VER)
    #define LIBASSERT_IS_MSVC 1
    #define LIBASSERT_MSVC_VERSION _MSC_VER
    #include <iso646.h> // alternative operator tokens are standard but msvc requires the include or /permissive- or /Za
#else
    #define LIBASSERT_IS_MSVC 0
    #define LIBASSERT_MSVC_VERSION 0
#endif


///
/// Detect standard library versions.
///

// libstdc++
#ifdef _GLIBCXX_RELEASE
    #define LIBASSERT_GLIBCXX_RELEASE _GLIBCXX_RELEASE
#else
    #define LIBASSERT_GLIBCXX_RELEASE 0
#endif


#ifdef _LIBCPP_VERSION
    #define LIBASSERT_LIBCPP_VERSION _LIBCPP_VERSION
#else
    #define LIBASSERT_LIBCPP_VERSION 0
#endif


#ifdef _MSVC_LANG
    #define LIBASSERT_CPLUSPLUS _MSVC_LANG
#else
    #define LIBASSERT_CPLUSPLUS __cplusplus
#endif


///
/// Helper macros for compiler attributes.
///

#ifdef __has_cpp_attribute
    #define LIBASSERT_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
    #define LIBASSERT_HAS_CPP_ATTRIBUTE(x) 0
#endif


///
/// Compiler attribute support.
///

#if LIBASSERT_HAS_CPP_ATTRIBUTE(nodiscard) && LIBASSERT_STD_VER >= 20
    #define LIBASSERT_ATTR_NODISCARD_MSG(msg) [[nodiscard(msg)]]
#else // Assume we have normal C++17 nodiscard support.
    #define LIBASSERT_ATTR_NODISCARD_MSG(msg) [[nodiscard]]
#endif


#if LIBASSERT_HAS_CPP_ATTRIBUTE(no_unique_address)
    #define LIBASSERT_ATTR_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
    #define LIBASSERT_ATTR_NO_UNIQUE_ADDRESS
#endif


///
/// General project macros
///


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


#if LIBASSERT_IS_GCC || LIBASSERT_STD_VER >= 20
    // __VA_OPT__ needed for GCC, https://gcc.gnu.org/bugzilla/show_bug.cgi?id=44317
    #define LIBASSERT_VA_ARGS(...) __VA_OPT__(,) __VA_ARGS__
#else
    // clang properly eats the comma with ##__VA_ARGS__
    #define LIBASSERT_VA_ARGS(...) , ##__VA_ARGS__
#endif


///
/// C++20 functionality wrappers.
///

// Check if we can use std::is_constant_evaluated.
#ifdef __has_include
    #if __has_include(<version>)
        #include <version>
        #ifdef __cpp_lib_is_constant_evaluated
            #include <type_traits>
            #define LIBASSERT_HAS_IS_CONSTANT_EVALUATED
        #endif
    #endif
#endif


// Check if we have the builtin __builtin_is_constant_evaluated.
#ifdef __has_builtin
    #if __has_builtin(__builtin_is_constant_evaluated)
        #define LIBASSERT_HAS_BUILTIN_IS_CONSTANT_EVALUATED
    #endif
#endif


// GCC 9.1+ and later has __builtin_is_constant_evaluated
#if (__GNUC__ >= 9) && !defined(LIBASSERT_HAS_BUILTIN_IS_CONSTANT_EVALUATED)
    #define LIBASSERT_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#endif


// Visual Studio 2019 (19.25) and later supports __builtin_is_constant_evaluated
#if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 192528326)
    #define LIBASSERT_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#endif


namespace libassert::support
{
    /**
     * @brief C++17 implementation of is_constant_evaluated that detects whether the function call occurs within a constant-evaluated context. Returns true if the evaluation of the call occurs within the evaluation of an expression or conversion that is manifestly constant-evaluated; otherwise returns false.
     * @return true if the evaluation of the call occurs within the evaluation of an expression or conversion that is manifestly constant-evaluated; otherwise false.
     *
     * @note Works with C++17 under GCC 9.1+, Clang 9+, and MSVC 19.25.
     */
    constexpr bool is_constant_evaluated() noexcept {
        #if defined(LIBASSERT_HAS_IS_CONSTANT_EVALUATED)
            return std::is_constant_evaluated();
        #elif defined(LIBASSERT_HAS_BUILTIN_IS_CONSTANT_EVALUATED)
            return __builtin_is_constant_evaluated();
        #else
            return false;
        #endif
    }
}


#endif // LIBASSERT_PLATFORM_HPP
