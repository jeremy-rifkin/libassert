- [libassert 1.1](#libassert-11)
- [libassert 1.0 🎉](#libassert-10-)

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
