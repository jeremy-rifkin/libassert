- [libassert 2.1.0](#libassert-210)
- [libassert 2.0.2](#libassert-202)
- [libassert 2.0.1](#libassert-201)
- [libassert 2.0.0](#libassert-200)
- [libassert 2.0.0-beta](#libassert-200-beta)
- [libassert 2.0.0-alpha](#libassert-200-alpha)
- [libassert 1.2.2](#libassert-122)
- [libassert 1.2.1](#libassert-121)
- [libassert 1.2](#libassert-12)
- [libassert 1.1](#libassert-11)
- [libassert 1.0 🎉](#libassert-10-)

## libassert 2.1.0

Added:
- Added software breakpoints to make assertion failures more debugger-friendly
- Added ANSI color support for catch2 integration now that it is supported in catch v3.6.0

Fixed:
- Fixed checking of stringifiable containers for types with `operator<<` overloads and fmt specializations

Other:
- Added support for gcc 8
- Improved testing

## libassert 2.0.2

Fixed:
- Fixed minor issue with `LIBASSERT_USE_MAGIC_ENUM` handling in libassert-config.cmake
- Fixed handling of edge case for internal string trimming utility

Other:
- Bumped cpptrace to 0.5.3

## libassert 2.0.1

Fixed:
- Fixed issue with determining stringifiability when `T::value_type` is a class named `value_type` with a constructor
  https://github.com/jeremy-rifkin/libassert/issues/90
- Fixed issue with an incorrect header being imported in `assert-gtest.hpp`
  https://github.com/jeremy-rifkin/libassert/issues/87

Other:
- Improved internal string formatting
- Improved Catch2 support

## libassert 2.0.0

Changes since v1:

**Assertion macros:**
- Replaced the previous `ASSERT`/`VERIFY` nomenclature with `DEBUG_ASSERT` and `ASSERT`
- Updated assertion macros to no longer return a value by default
- Added `DEBUG_ASSERT_VAL`, `ASSERT_VAL`, and `ASSUME_VAL` variants that return values
- Added `PANIC` and `UNREACHABLE`
- Added `LIBASSERT_PREFIX_ASSERTIONS` option to only add assertion macros with a `LIBASSERT_` prefix

**Assertion behavior:**
- Removed default throwing behavior for some assertion failures, all assertions now abort by default
- Removed `libassert::verification_failure`
- Removed non-fatal assertions

**Custom failure handlers:**
- Removed macro-based custom assertion failure handler system
- Added `libassert::set_failure_handler`
- Removed default safe signed-unsigned comparison behavior
- Added `LIBASSERT_SAFE_COMPARISONS` to opt into safe signed-unsigned comparisons
- Simplified assertion failure handler signatures

**Assertion information:**
- Removed `libassert::assertion_printer`
- Added `libassert::assertion_info` with much more comprehensive information and methods for working with assertion
  information

**Testing library support:**
- Added Catch2 and GoogleTest integrations

**Library structure:**
- Removed `libassert::utility` and `libassert::config` namespaces
- Updated the library's cmake target name to `libassert::assert`
- Updated the library's header to `<libasssert/assert.hpp>`

**Configuration:**
- Added configuration for literal formatting
- Added configuration for path shortening modes
- Added setting for output separator with `libassert::set_separator`
- Removed `libassert::set_color_output` and `libassert::set_rgb_output`
- Added `libassert::set_color_scheme`

**Stringification:**
- Improved stringification generation to handle more types and better decide when there is useful information to print
- Added stringification customization point instead of relying on `operator<<(std::ostream, const T&)`
- Added `LIBASSERT_NO_STRINGIFY_SMART_POINTER_OBJECTS` option
- Added limit to the number of items stringified for containers
- Added {fmt} support with `LIBASSERT_USE_FMT`

**Utilities:**
- Added `libassert::enable_virtual_terminal_processing_if_needed()` to the public interface
- Added `libassert::isatty` to the public interface as well as stdin/stdout/stderr constants
- Updated `libassert::stacktrace` to take color scheme and skip parameters
- Removed `libassert::replace_rgb`
- Removed `libassert::strip_colors`

**Output:**
- Improved stacktrace format to make "click to jump to source" possible in some IDEs

**Analysis:**
- Type prettifying
  - Added normalization for `std::__cxx11::` to to `std::`
  - Added normalization for msvc ``​`anonymous namespace'`` to `(anonymous namespace)`
- Overhauled C++ tokenizer to use a much better implementation

**Bug fixes:**
- Resolved long-standing crash caused by libc++'s std::regex implementation

**Internal improvements:**
- Improved how data is stored in `binary_diagnostics_descriptor`, `assert_static_parameters`, and `assertion_info`
- Improved internal argument processing
- Improved assertion macro expansion
- Added C++23 specialization for how static assertion information is stored which is hopefully faster for compile times
- Improved handling of literal formatting
- Removed the global lock used while processing assertions
- Improved assertion processing logic
- Lots of internal refactoring, cleanup, and changes to improve maintainability
- Overhauled CMake thanks to a contributor, fixing a lot of issues and greatly improving the setup
- Removed need for user to manually specify `LIBASSERT_STATIC` if using the project with cmake and a static build
- Magic enum is now grabbed with FetchContent or externally from an existing install or package manager
- Resolved inconsistent use of `ASSERT` vs `LIBASSERT` for the library's macro prefix
- Updated internal error handling to use cpptrace exceptions
- Added cmake integration testing on mingw to CI
- General CI improvements
- General testing improvements

## libassert 2.0.0-beta

Changes:
- Catch2 and GoogleTest integrations
- Implemented alternative path modes
- {fmt} support with `LIBASSERT_USE_FMT`
- Reworked stringification again
- Overhauled C++ tokenizer to use a much better implementation
- Added `libassert::enable_virtual_terminal_processing_if_needed()` to the public interface
- Added `libassert::isatty` to the public interface as well as stdin/stdout/stderr constants
- Renamed `libassert::generate_stringification` to `libassert::stringify`
- Added setting for output separator with `libassert::set_separator`
- Updated parameter passing for `libassert::set_color_scheme`
- Updated `libassert::stacktrace` to take color scheme and skip parameters
- Added C++23 specialization for how static assertion information is stored which is hopefully faster for compile times
- Added `LIBASSERT_PREFIX_ASSERTIONS` option to only add assertion macros with a `LIBASSERT_` prefix
- Added normalization for msvc ``​`anonymous namespace'``
- Added `LIBASSERT_NO_STRINGIFY_SMART_POINTER_OBJECTS` option
- Improved how data is stored in `binary_diagnostics_descriptor`, `assert_static_parameters`, and `assertion_info`
- Simplified up failure handler signature
- Added limit to the number of items stringified for containers
- Resolved long-standing bug related to libc++'s std::regex implementation
- Added methods for getting parts of assertion info outputs for easier custom failure handler implementation
- Improved internal argument processing
- Cmake fixes and improvements

## libassert 2.0.0-alpha

This is the first pre-release for version 2. Version 2 is an overhaul of the library taking lessons learned.

Major changes:
- Replaced the previous `ASSERT`/`VERIFY` nomenclature with `DEBUG_ASSERT` and `ASSERT`
- Updated assertion macros to no longer return a value by default
  - Debug-only assertions no longer have to evaluate the assertion expression, and removed `NO_ASSERT_RELEASE_EVAL`
- Added `DEBUG_ASSERT_VAL`, `ASSERT_VAL`, and `ASSUME_VAL` variants that return values
- Added `PANIC` and `UNREACHABLE`
- Removed default throwing behavior for some assertion failures, all assertions now abort by default
- Removed non-fatal assertions
- Improved stringification generation to handle more types and better decide when there is useful information to print
- Added stringification customization point instead of relying on `operator<<(std::ostream, const T&)`
- Removed macro-based custom assertion failure handler system
- Added `libassert::set_failure_handler`
- Removed default safe signed-unsigned comparison behavior
- Added `LIBASSERT_SAFE_COMPARISONS` to opt into safe signed-unsigned comparisons
- Added color palette configuration
- Added configuration for literal formatting
- Reworked structures used to represent assertion information
  - Cleaned up `binary_diagnostics_descriptor`
  - Removed up `extra_diagnostics` struct trying to do too much
  - Renamed `assertion_printer` to `assertion_info`
  - Made more information available in the `assertion_info` struct
  - Removed lifetime funniness with the `assertion_info` struct
- Type prettifying
  - `std::__cxx11::` is simplified to `std::`

Core changes:
- Cleaned up assertion macro expansion
- Cleaned up handling of literal formatting
- Cleaned up stringification generation
- Cleaned up assertion processing logic
- Removed the global lock used while processing assertions
- Lots of internal refactoring, cleanup, and changes to improve maintainability
- Overhauled CMake thanks to a contributor, fixing a lot of issues
- Removed need for user to manually specify `ASSERT_STATIC` if using the project with cmake and a static build
- Magic enum is now grabbed with FetchContent or externally from an existing install or package manager
- Reworked `README.md`
- Added `CONTRIBUTING.md`
- Resolved inconsistent use of `ASSERT` vs `LIBASSERT` for the library's macro prefix
- Fixed inconsistent use of `assert` vs `libassert` for referring to the library
- Updated the library's cmake target name to `libassert::assert`
- Updated the library's header to `<libasssert/assert.hpp>`
- Updated internal error handling to use cpptrace exceptions
- Added cmake integration testing on mingw to CI
- General CI improvements
- General testing improvements

## libassert 1.2.2

Small patch release that bumps the stack tracing back-end, part of getting vcpkg issues ironed out.

- Bumped cpptrace to version 0.3.0

## libassert 1.2.1
- More testing
- Minor fixes and refactors
- Added sonarcloud analysis
- Bumped cpptrace to a much improved version 0.2.1

## libassert 1.2
- Added `libassert::config::set_rgb_output` to control whether 24-bit ansi color sequences are used for coloring
- Added support for stringifying std::error_code, std::error_condition, std::expected, and std::unexpected
- Added -Wundef support
- Added support for old-style `ASSERT(foo && "Message")`
- Massively reworked stack traces so they are now much more reliable, portable, and efficient with the help of [cpptrace](https://github.com/jeremy-rifkin/cpptrace)
- Improved stack tracing on windows: Added support for more complex symbols in stack traces
- Improved MSVC behavior under `/Zc:preprocessor`
- Improved output for assertions that aren't currently decomposed into a left-hand / right-hand (e.g. `ASSERT(a + b);`)
- Improved cmake
- Improved automated build and test workflows
- Removed Makefiles
- Fixed bug with analysis of expressions containing `>>`
- Fixed bug with highlighting of escape sequences in strings
- Fixed bug with line number width calculations
- Fixed issue with a gcc bug via a workaround
- Fixed issue with gnu libstdc++ dual abi via a workaround
- Fixed issue with -Wuseless-cast
- General code cleanup and improvements
-
## libassert 1.1

Changelog:
+ Added `-DNO_ASSERT_RELEASE_EVAL` to make `ASSERT` behave more like traditional C assert
+ Renamed library's namespace to `libassert::`
+ Added customization point for `libassert::detail::stringify`
+ Added support for assertions in constexpr functions
+ Some internal refactoring, bug fixes, and typo corrections
+ Better cmake, big thanks to @qdewaghe!

## libassert 1.0 🎉

Changelog:

+ Renamed: `CHECK` to `DEBUG_ASSERT` and `ASSERT` no longer provides an optimizer hint in release. For the hint use
  `ASSUME` - I gave the naming a lot of thought and decided on making naming state intent clearly
+ Overhauled assertion handling to produce better codegen and only compute extra diagnostic expressions in the failure
  path
+ Library options:
    + No longer automatically decompose `&&` and `||` expressions because of short-circuiting (decided this behavior was
      too surprising), this can be re-enabled if desired with `-DASSERT_DECOMPOSE_BINARY_LOGICAL`
    + It's no longer the default to `#define` the lowercase `assert` alias, this alias can be enabled with
      `-DASSERT_LOWERCASE`
    + Removed `-DNCOLOR` in favor of `asserts::config::set_color_output` and `asserts::utils::strip_colors`. This allows
      easiest customization of the color printing behavior and making string replacements in this code path is not a
      performance concern
+ Assertions are printed only in color by the default handler when the output destination is a tty
+ Custom assert failure handlers
    + Overhauled assertion printing to allow custom assertion failure handlers to print for desired widths
    + Added interface for custom assertion handlers to get a filename, line number, function signature, and assertion
      message from the assertion printer
+ Improved handling for extra diagnostics and optional assertion messages
+ Ensuring clean compilation under strictest possible warning flags (everything short of -pedantic and /W4, including
  -Wshadow)
+ Support for MSVC compilers
+ Fixed windows dbghelp stack trace bugs 🎉
+ Improved windows stack trace symbol resolution (skip the `this` parameter)
+ The library now makes a few internal utilities public as they are immensely useful
    + The library now exposes the internal stack trace generator, pretty type name generator, and stringification
      utilities as bonuses (also exposes internal utilities to get the terminal width and strip colors for use in a
      custom failure handler)
+ Refactoring and cleanup
    + Continued moving as much logic as possible out of the .hpp and into the .cpp. Notably, nothing related to syntax
      highlighting is in the .hpp anymore, a lot of stringification logic has been moved out, much of the printing logic
      has also been moved out)
    + "Macro Hygiene"
    + Namespace reorganization
+ Improved handling of forwarding in the expression decomposer
+ CMake configuration
+ Improved documentation
+ Improvements to trace printing
+ Improved type name generation
+ Added cleaning for type names from the compiler
    + Example from MSVC:<br/>Before type cleaning: `class std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char>>,int,struct std::less<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char>>>,class std::allocator<struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char>> const ,int>>>`<br/>After type cleaning: `std::map<std::string, int, std::less<std::string>>`
+ Improved value printing and stringification:
    + Pointers are printed with their data type, function pointers are now supported
    + Better display of char values in conjunction with ints
    + Added printing for container types
    + Added printing for tuple-like types
    + Added optional support for magic enum for better stringification of enum class values (enabled with
      `-DASSERT_USE_MAGIC_ENUM`)
+ Added spaceship operator support
+ Added Velociraptors

Internal Development:
+ Improved build setup
+ Lots of work to setup automated unit and integration testing for the library
+ Automated static analysis
