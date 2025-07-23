#ifndef LIBASSERT_PLATFORM_HPP
#define LIBASSERT_PLATFORM_HPP

// Copyright (c) 2021-2025 Jeremy Rifkin under the MIT license
// https://github.com/jeremy-rifkin/libassert

#define LIBASSERT_ABI_NAMESPACE_TAG abiv2

#define LIBASSERT_BEGIN_NAMESPACE \
    namespace libassert { \
    inline namespace LIBASSERT_ABI_NAMESPACE_TAG {
#define LIBASSERT_END_NAMESPACE \
    } \
    }

// =====================================================================================================================
// || Preprocessor stuff                                                                                              ||
// =====================================================================================================================

// Set the C++ version number based on if we are on a dumb compiler like MSVC or not.
#ifdef _MSVC_LANG
 #define LIBASSERT_CPLUSPLUS _MSVC_LANG
#else
 #define LIBASSERT_CPLUSPLUS __cplusplus
#endif

#if LIBASSERT_CPLUSPLUS >= 202302L
 #define LIBASSERT_STD_VER 23
#elif LIBASSERT_CPLUSPLUS >= 202002L
 #define LIBASSERT_STD_VER 20
#elif LIBASSERT_CPLUSPLUS >= 201703L
 #define LIBASSERT_STD_VER 17
#else
 #error "libassert requires C++17 or newer"
#endif

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

#if defined(_MSC_VER) && !defined(__clang__) // clang on windows defines _MSC_VER
 #define LIBASSERT_IS_MSVC 1
 #define LIBASSERT_MSVC_VERSION _MSC_VER
#else
 #define LIBASSERT_IS_MSVC 0
 #define LIBASSERT_MSVC_VERSION 0
#endif

#ifdef __INTEL_COMPILER
 #define LIBASSERT_IS_ICC 1
#else
 #define LIBASSERT_IS_ICC 0
#endif

#ifdef __INTEL_LLVM_COMPILER
 #define LIBASSERT_IS_ICX 1
#else
 #define LIBASSERT_IS_ICX 0
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

#if LIBASSERT_STD_VER >= 23
 #include <utility>
 #define LIBASSERT_UNREACHABLE_CALL() (::std::unreachable())
#elif LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC
 #define LIBASSERT_UNREACHABLE_CALL() __builtin_unreachable()
#else
 #define LIBASSERT_UNREACHABLE_CALL() __assume(false)
#endif

#if LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC
 #define LIBASSERT_PFUNC __extension__ __PRETTY_FUNCTION__
 #define LIBASSERT_ATTR_COLD     [[gnu::cold]]
 #define LIBASSERT_ATTR_NOINLINE [[gnu::noinline]]
#else
 #define LIBASSERT_PFUNC __FUNCSIG__
 #define LIBASSERT_ATTR_COLD
 #define LIBASSERT_ATTR_NOINLINE __declspec(noinline)
#endif

#if LIBASSERT_IS_MSVC
 #define LIBASSERT_STRONG_EXPECT(expr, value) (expr)
#elif (defined(__clang__) && __clang_major__ >= 11) || __GNUC__ >= 9
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

#if defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL
 #define LIBASSERT_NON_CONFORMANT_MSVC_PREPROCESSOR 1
#else
 #define LIBASSERT_NON_CONFORMANT_MSVC_PREPROCESSOR 0
#endif

#if (LIBASSERT_IS_GCC || LIBASSERT_STD_VER >= 20) && !LIBASSERT_NON_CONFORMANT_MSVC_PREPROCESSOR
 // __VA_OPT__ needed for GCC, https://gcc.gnu.org/bugzilla/show_bug.cgi?id=44317
 #define LIBASSERT_VA_ARGS(...) __VA_OPT__(,) __VA_ARGS__
#else
 // clang properly eats the comma with ##__VA_ARGS__
 #define LIBASSERT_VA_ARGS(...) , ##__VA_ARGS__
#endif

///
/// C++20 functionality wrappers.
///

#ifdef __has_include
 #if __has_include(<version>)
  #include <version>
  #if !defined(LIBASSERT_NO_STD_FORMAT) && __has_include(<format>) && defined(__cpp_lib_format)
   #define LIBASSERT_USE_STD_FORMAT
  #endif
 #endif
#endif

#if LIBASSERT_IS_CLANG || LIBASSERT_IS_GCC
  #define LIBASSERT_WARNING_PRAGMA_PUSH _Pragma("GCC diagnostic push")
  #define LIBASSERT_WARNING_PRAGMA_POP _Pragma("GCC diagnostic pop")
#elif LIBASSERT_IS_MSVC
  #define LIBASSERT_WARNING_PRAGMA_PUSH _Pragma("warning(push)")
  #define LIBASSERT_WARNING_PRAGMA_POP _Pragma("warning(pop)")
#endif

#if LIBASSERT_IS_CLANG || LIBASSERT_IS_ICX
 // clang and icx support this as far back as this library could care
 #define LIBASSERT_BREAKPOINT() __builtin_debugtrap()
#elif LIBASSERT_IS_MSVC || LIBASSERT_IS_ICC
 // msvc and icc support this as far back as this library could care
 #define LIBASSERT_BREAKPOINT() __debugbreak()
#elif LIBASSERT_IS_GCC
 #if LIBASSERT_GCC_VERSION >= 1200
  #define LIBASSERT_IGNORE_CPP20_EXTENSION_WARNING _Pragma("GCC diagnostic ignored \"-Wc++20-extensions\"")
 #else
  #define LIBASSERT_IGNORE_CPP20_EXTENSION_WARNING
 #endif
 #define LIBASSERT_ASM_BREAKPOINT(instruction) \
  do { \
   LIBASSERT_WARNING_PRAGMA_PUSH \
   LIBASSERT_IGNORE_CPP20_EXTENSION_WARNING \
   __asm__ __volatile__(instruction) \
   ; \
   LIBASSERT_WARNING_PRAGMA_POP \
  } while(0)
 // precedence for these come from llvm's __builtin_debugtrap() implementation
 // arm: https://github.com/llvm/llvm-project/blob/e9954ec087d640809082f46d1c7e5ac1767b798d/llvm/lib/Target/ARM/ARMInstrInfo.td#L2393-L2394
 //  def : Pat<(debugtrap), (BKPT 0)>, Requires<[IsARM, HasV5T]>;
 //  def : Pat<(debugtrap), (UDF 254)>, Requires<[IsARM, NoV5T]>;
 // thumb: https://github.com/llvm/llvm-project/blob/e9954ec087d640809082f46d1c7e5ac1767b798d/llvm/lib/Target/ARM/ARMInstrThumb.td#L1444-L1445
 //  def : Pat<(debugtrap), (tBKPT 0)>, Requires<[IsThumb, HasV5T]>;
 //  def : Pat<(debugtrap), (tUDF 254)>, Requires<[IsThumb, NoV5T]>;
 // aarch64: https://github.com/llvm/llvm-project/blob/e9954ec087d640809082f46d1c7e5ac1767b798d/llvm/lib/Target/AArch64/AArch64FastISel.cpp#L3615-L3618
 //  case Intrinsic::debugtrap:
 //   BuildMI(*FuncInfo.MBB, FuncInfo.InsertPt, MIMD, TII.get(AArch64::BRK))
 //       .addImm(0xF000);
 //   return true;
 // x86: https://github.com/llvm/llvm-project/blob/e9954ec087d640809082f46d1c7e5ac1767b798d/llvm/lib/Target/X86/X86InstrSystem.td#L81-L84
 //  def : Pat<(debugtrap),
 //            (INT3)>, Requires<[NotPS]>;
 #if defined(__i386__) || defined(__x86_64__)
  #define LIBASSERT_BREAKPOINT() LIBASSERT_ASM_BREAKPOINT("int3")
 #elif defined(__arm__) || defined(__thumb__)
  #if __ARM_ARCH >= 5
   #define LIBASSERT_BREAKPOINT() LIBASSERT_ASM_BREAKPOINT("bkpt #0")
  #else
   #define LIBASSERT_BREAKPOINT() LIBASSERT_ASM_BREAKPOINT("udf #0xfe")
  #endif
 #elif defined(__aarch64__) || defined(_M_ARM64)
  #define LIBASSERT_BREAKPOINT() LIBASSERT_ASM_BREAKPOINT("brk #0xf000")
 #else
  // some architecture we aren't prepared for
  #define LIBASSERT_BREAKPOINT()
 #endif
#else
 // some compiler we aren't prepared for
 #define LIBASSERT_BREAKPOINT()
#endif

#endif
