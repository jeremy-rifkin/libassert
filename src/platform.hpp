#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include "common.hpp"

#ifndef _CRT_SECURE_NO_WARNINGS
// NOLINTNEXTLINE(bugprone-reserved-identifier, cert-dcl37-c, cert-dcl51-cpp)
#define _CRT_SECURE_NO_WARNINGS // done only for strerror
#endif

#if IS_WINDOWS
 #ifndef STDIN_FILENO
  #define STDIN_FILENO _fileno(stdin)
  #define STDOUT_FILENO _fileno(stdout)
  #define STDERR_FILENO _fileno(stderr)
 #endif
#else
 #include <unistd.h>
#endif

#include <libassert/assert.hpp>

#endif
