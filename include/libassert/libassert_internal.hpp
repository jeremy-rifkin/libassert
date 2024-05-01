#ifndef LIBASSERT_INTERNAL_HPP
#define LIBASSERT_INTERNAL_HPP

// Copyright (c) 2021-2024 Jeremy Rifkin under the MIT license
// https://github.com/jeremy-rifkin/libassert


#if defined(_MSVC_LANG) && _MSVC_LANG < 201703L
    #error "libassert requires C++17 or newer"
#elif !defined(_MSVC_LANG) && __cplusplus < 201703L
    #pragma error "libassert requires C++17 or newer"
#endif

// LIBASSERT_CPP_VER
//
// Macro that checks we have available the macro __cplusplus and assigns its value to LIBASSERT_CPP_VER
#ifndef LIBASSERT_CPLUSPLUS
    #if defined(__cplusplus)
        #define LIBASSERT_CPLUSPLUS __cplusplus
    #else
            #error "Libassert requires that __cplusplus is defined!"
    #endif // __cplusplus
#endif // LIBASSERT_CPP_VER


// LIBASSERT_STRINGIFY
//
// Example usage:
//     printf("Line: %s", LIBASSERT_STRINGIFY(__LINE__));
//
#ifndef LIBASSERT_STRINGIFY
    #define LIBASSERT_STRINGIFY(x) LIBASSERT_STRINGIFYIMPL(x)
    #define LIBASSERT_STRINGIFYIMPL(x) #x
#endif



#ifndef LIBASSERT_STD_VER
    #if LIBASSERT_CPLUSPLUS <= 201703L
        #define LIBASSERT_STD_VER 17
    #elif LIBASSERT_CPLUSPLUS <= 202002L
        #define LIBASSERT_STD_VER 20
    #elif LIBASSERT_CPLUSPLUS <= 202302L
        #define LIBASSERT_STD_VER 23
    #else
        // TODO: Once C++26 is fully released we should update this to instead check for its defined value.
        //       Currently, at this time there is no reliable manner to truely know what the version number for C++26 is.
        #define LIBASSERT_STD_VER 26 // If our version number is higher than 202303L assume we are using C++26
    #endif // LIBASSERT_CPLUSPLUS
#endif // LIBASSERT_STD_VER

// Detect compiler versions.
#if defined(__clang__) && !defined(__ibmxl__)
    #define LIBASSERT_CLANG_VERSION (__clang_major__ * 100 + __clang_minor__)
#else
    #define LIBASSERT_CLANG_VERSION 0
#endif

#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#define LIBASSERT_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#define LIBASSERT_GCC_VERSION 0
#endif

#if defined(__ICL)
#define LIBASSERT_ICC_VERSION __ICL
#elif defined(__INTEL_COMPILER)
#define LIBASSERT_ICC_VERSION __INTEL_COMPILER
#else
#define LIBASSERT_ICC_VERSION 0
#endif

#if defined(_MSC_VER)
#define LIBASSERT_MSC_VERSION _MSC_VER
#else
#define LIBASSERT_MSC_VERSION 0
#endif


// Detect standard library versions.
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


#endif // LIBASSERT_INTERNAL_HPP
