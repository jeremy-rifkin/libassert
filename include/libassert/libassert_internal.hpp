#ifndef LIBASSERT_INTERNAL_HPP
#define LIBASSERT_INTERNAL_HPP

// Copyright (c) 2021-2024 Jeremy Rifkin under the MIT license
// https://github.com/jeremy-rifkin/libassert



// =====================================================================================================================
// || Preprocessor stuff                                                                                              ||
// =====================================================================================================================

// Validate that we are using a C++17 or newer compiler.
#if defined(_MSVC_LANG) && _MSVC_LANG < 201703L
    #error "libassert requires C++17 or newer"
#elif !defined(_MSVC_LANG) && __cplusplus < 201703L
    #pragma error "libassert requires C++17 or newer"
#endif

// Set the C++ version number based on if we are on a dumb compiler like MSVC or not.
#ifdef _MSVC_LANG
    #define LIBASSERT_CPLUSPLUS _MSVC_LANG
#else
    #define LIBASSERT_CPLUSPLUS __cplusplus
#endif

// LIBASSERT_STRINGIFY
//
// Example usage:
//     printf("Line: %s", LIBASSERT_STRINGIFY(__LINE__));
//
#ifndef LIBASSERT_STRINGIFY
    #define LIBASSERT_STRINGIFY(x) LIBASSERT_STRINGIFYIMPL(x)
    #define LIBASSERT_STRINGIFYIMPL(x) #x
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


// Detect Clang compiler.
#if defined(__clang__) && !defined(__ibmxl__)
    #define LIBASSERT_IS_CLANG 1
    #define LIBASSERT_CLANG_VERSION (__clang_major__ * 100 + __clang_minor__)
#else
    #define LIBASSERT_IS_CLANG 0
    #define LIBASSERT_CLANG_VERSION 0
#endif


// Detect GCC compiler.
#if (defined(__GNUC__) || defined(__GNUG__)) && !defined(__clang__) && !defined(__INTEL_COMPILER)
    #define LIBASSERT_IS_GCC 1
    #define LIBASSERT_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
    #define LIBASSERT_IS_GCC 0
    #define LIBASSERT_GCC_VERSION 0
#endif


// Detect MSVC compiler.
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

// Detect libstdc++ version.
#ifdef _GLIBCXX_RELEASE
    #define LIBASSERT_GLIBCXX_RELEASE _GLIBCXX_RELEASE
#else
    #define LIBASSERT_GLIBCXX_RELEASE 0
#endif


// Detect libc++ version.
#ifdef _LIBCPP_VERSION
    #define LIBASSERT_LIBCPP_VERSION _LIBCPP_VERSION
#else
    #define LIBASSERT_LIBCPP_VERSION 0
#endif


// Detect the version number of __cplusplus.
#ifdef _MSVC_LANG
    #define LIBASSERT_CPLUSPLUS _MSVC_LANG
#else
    #define LIBASSERT_CPLUSPLUS __cplusplus
#endif


///
/// Helper macros for compiler attributes.
///

// Check if we have __has_cpp_attribute support.
#ifdef __has_cpp_attribute
#  define LIBASSERT_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#  define LIBASSERT_HAS_CPP_ATTRIBUTE(x) 0
#endif


///
/// Compiler attribute support.
///

// Check if we have C++20's nodiscard("reason") attribute.
#if LIBASSERT_HAS_CPP_ATTRIBUTE(nodiscard) && LIBASSERT_STD_VER >= 20
    #define LIBASSERT_NODISCARD_MSG(msg) [[nodiscard(msg)]]
#else // Assume we have normal C++17 nodiscard support.
    #define LIBASSERT_NODISCARD_MSG(msg) [[nodiscard]]
#endif


// Check if we have C++20's likely attribute.
// Clang and GCC's builtin_expect compiler intrinsic appears to work 1:1 with C++20's likely attribute.
#if LIBASSERT_HAS_CPP_ATTRIBUTE(likely)
    #define LIBASSERT_LIKELY [[likely]]
#elif defined(__GNUC__) || defined(__clang__)
    #define LIBASSERT_LIKELY __builtin_expect(!!(expr), 1)
#else
    #define LIBASSERT_LIKELY
#endif


// Check if we have C++20's unlikely attribute.
// Clang and GCC's builtin_expect compiler intrinsic appears to work 1:1 with C++20's unlikely attribute.
#if LIBASSERT_HAS_CPP_ATTRIBUTE(unlikely)
    #define LIBASSERT_UNLIKELY [[unlikely]]
#elif defined(__GNUC__) || defined(__clang__)
    #define LIBASSERT_UNLIKELY __builtin_expect(!!(expr), 0)
#else
    #define LIBASSERT_UNLIKELY
#endif


// Check if we have C++20's no_unique_address attribute.
#if LIBASSERT_HAS_CPP_ATTRIBUTE(no_unique_address)
    #define LIBASSERT_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
    #define LIBASSERT_NO_UNIQUE_ADDRESS
#endif


// Check if we have C++23's assume attribute.
#if LIBASSERT_HAS_CPP_ATTRIBUTE(assume)
    #define LIBASSERT_ASSUME(expr) [[assume(expr)]]
#else
    #define LIBASSERT_ASSUME(expr)
#endif


///
/// C++20 feature support.
///

// Detect that we have access to consteval, C++20 constexpr extensions, and std::is_constant_evaluated.
#if !defined(__cpp_lib_is_constant_evaluated)
    #define LIBASSERT_USE_CONSTEVAL 0
#elif LIBASSERT_CPLUSPLUS < 201709L
    #define LIBASSERT_USE_CONSTEVAL 0
#elif LIBASSERT_GLIBCXX_RELEASE && LIBASSERT_GLIBCXX_RELEASE < 10
    #define LIBASSERT_USE_CONSTEVAL 0
#elif LIBASSERT_LIBCPP_VERSION && LIBASSERT_LIBCPP_VERSION < 10000
    #define LIBASSERT_USE_CONSTEVAL 0
#elif defined(__apple_build_version__) && __apple_build_version__ < 14000029L
    #define LIBASSERT_USE_CONSTEVAL 0  // consteval is broken in Apple clang < 14.
#elif LIBASSERT_MSC_VERSION && LIBASSERT_MSC_VERSION < 1929
    #define LIBASSERT_USE_CONSTEVAL 0  // consteval is broken in MSVC VS2019 < 16.10.
#elif defined(__cpp_consteval)
    #define LIBASSERT_USE_CONSTEVAL 1
#elif LIBASSERT_GCC_VERSION >= 1002 || LIBASSERT_CLANG_VERSION >= 1101
    #define LIBASSERT_USE_CONSTEVAL 1
#else
    #define LIBASSERT_USE_CONSTEVAL 0
#endif

#if LIBASSERT_USE_CONSTEVAL
    #define LIBASSERT_CONSTEVAL consteval
    #define LIBASSERT_CONSTEXPR20 constexpr
#else
    #define LIBASSERT_CONSTEVAL
    #define LIBASSERT_CONSTEXPR20
#endif



///
/// C++23 feature support.
///

// Permit static constexpr variables in constexpr functions in C++23.
#if defined(__cpp_constexpr) && __cpp_constexpr >= 202211L
    #define LIBASSERT_CONSTEXPR23_STATIC_VAR static constexpr
#else
    #define LIBASSERT_CONSTEXPR23_STATIC_VAR
#endif

#ifdef __cpp_if_consteval
    #define LIBASSERT_IF_CONSTEVAL if consteval
#else
    #define LIBASSERT_IF_CONSTEVAL if constexpr
#endif


///
/// C++20 functionality wrappers.
///

#ifdef __has_include
    # if __has_include(<version>)
        #  include <version>
        #  ifdef __cpp_lib_is_constant_evaluated
            #   include <type_traits>
            #   define LIBASSERT_HAS_IS_CONSTANT_EVALUATED
        #  endif
    # endif
#endif

#ifdef __has_builtin
    #  if __has_builtin(__builtin_is_constant_evaluated)
        #    define LIBASSERT_HAS_BUILTIN_IS_CONSTANT_EVALUATED
    #  endif
#endif

// GCC 9 and later has __builtin_is_constant_evaluated
#if (__GNUC__ >= 9) && !defined(LIBASSERT_HAS_BUILTIN_IS_CONSTANT_EVALUATED)
    #  define LIBASSERT_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#endif

// Visual Studio 2019 and later supports __builtin_is_constant_evaluated
#if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 192528326)
    #  define LIBASSERT_HAS_BUILTIN_IS_CONSTANT_EVALUATED
#endif


// Add a helper function for support to C++20's std::is_constant_evaluated.
namespace libassert::support
{
    constexpr bool is_constant_evaluated() noexcept
    {
#if defined(LIBASSERT_HAS_IS_CONSTANT_EVALUATED)
        return std::is_constant_evaluated();
#elif defined(LIBASSERT_HAS_BUILTIN_IS_CONSTANT_EVALUATED)
        return __builtin_is_constant_evaluated();
#else
        return false;
#endif
    }
}


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

#if LIBASSERT_IS_MSVC
#pragma warning(push)
// warning C4251: using non-dll-exported type in dll-exported type, firing on std::vector<frame_ptr> and others for
// some reason
// 4275 is the same thing but for base classes
#pragma warning(disable: 4251; disable: 4275)
#endif

#if LIBASSERT_IS_GCC || LIBASSERT_STD_VER >= 20

// __VA_OPT__ needed for GCC, https://gcc.gnu.org/bugzilla/show_bug.cgi?id=44317
#define LIBASSERT_VA_ARGS(...) __VA_OPT__(,) __VA_ARGS__
#else
// clang properly eats the comma with ##__VA_ARGS__
#define LIBASSERT_VA_ARGS(...) , ##__VA_ARGS__
#endif


#endif // LIBASSERT_INTERNAL_HPP
