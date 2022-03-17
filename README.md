# Asserts <!-- omit in toc -->

![build status](https://github.com/jeremy-rifkin/asserts/workflows/build/badge.svg)
![tests status](https://github.com/jeremy-rifkin/asserts/workflows/tests/badge.svg)

<p align="center">The most over-engineered assertion library</p>

```cpp
void zoog(std::vector<int>& vec) {
    assert(vec.size() > min_items(), "vector doesn't have enough items");
}
```
![](screenshots/a.png)

```cpp
const char* path = "/home/foobar/baz";
FILE* f = VERIFY(fopen(path, "r") != nullptr, "Internal error with foobars", errno, path);
```
![](screenshots/b.png)

```cpp
std::optional<float> get_param();
float f = *assert(get_param());
```
![](screenshots/c.png)

### Table of Contents: <!-- omit in toc -->
- [Philosophy](#philosophy)
- [Methodology](#methodology)
- [Considerations](#considerations)
- [Features](#features)
- [Documentation](#documentation)
	- [Parameters](#parameters)
	- [Return value](#return-value)
	- [Failure](#failure)
	- [Configuration](#configuration)
	- [Utilities](#utilities)
	- [Namespace synopsis](#namespace-synopsis)
- [How To Use This Library](#how-to-use-this-library)
	- [1. Build](#1-build)
	- [2. Install](#2-install)
	- [3. Use](#3-use)
- [Replacing &lt;cassert&gt;](#replacing-cassert)
- [Comparison With Other Languages](#comparison-with-other-languages)

## Philosophy

Fundamentally the role of assertions is to verify assumptions made in software and identify
violations close to their sources. Assertion tooling should prioritize providing as much information
and context to the developer as possible to allow for speedy triage. Unfortunately, existing
language and library tooling provides very limited triage information.

For example with stdlib asserts, when `assert(n <= 12);` fails we may get no information about why
(i.e., the value of `n`) and we don't get stack trace. Ideally an assertion failure should provide
enough diagnostic information that we don't have to rerun in a debugger to pinpoint the problem.

This library is an exploration looking at how much helpful information and functionality can be
packed into assertions while also providing a quick and easy interface for the developer.

## Methodology

Different types of assumptions call for different handling. This library has a tiered assertion
system:
- For core assumptions that must always be true under normal operation use `ASSERT`
- For convenient assumptions, e.g. not worrying about an edge case for the time being, use `VERIFY`
- For quick development/sanity checks, use `CHECK`

| Name     | When to Use                 | Effect |
| -------- | --------------------------- | ------ |
| `ASSERT` / `assert`* | Core assumptions | Checked in debug, assumed in release |
| `VERIFY` | Convenient assumptions      | Checked always |
| `CHECK`  | Sanity checks               | Checked in debug, does nothing in release |

Rationale for `CHECK`: Sometimes it's problematic to assume expressions, e.g. a call to
`std::unordered_map::at` call can't be optimized away even if the result is unused.

Under `-DNDEBUG` assertions will mark the fail path as unreachable, potentially providing helpful
information to the optimizer. It's important to note the immediate consequence of this is tha
assertion failure in `-DNDEBUG` can lead to UB.

`VERIFY` and `CHECK` calls may specified to be nonfatal. If marked nonfatal `CHECK`/`VERIFY` will
simply log a failure message but not abort or throw an exception.

\*: `assert` as an alias for `ASSERT` is not enabled by default (see
[Replacing &lt;cassert&gt;](#replacing-cassert)). Provide `-DASSERT_LOWERCASE` to enable it. I will
use lowercase `assert` throughout this README.

## Considerations

Automatic expression decomposition requires a lot of template metaprogramming shenanigans. This adds
a lot of work at the callsite just to setup an assertion expression. These calls are swiftly inlined
in an optimized build, but it is a consideration for unoptimized builds.

All the TMP work required to setup and process assertions is a consideration for build speeds, there
will be an impact on build speeds from using this library. This is the price we pay for nice things.
As stated previously, this library is a proof of concept. Doing this better might require language
support.

As far as runtime performance goes, the impact at callsites is very minimal under `-Og` or higher,
potentially even less than a stdlib assert.

A lot of work is required to process assertion failures once they happen but failures are *the
coldest* path in a binary, I'm not concerned with performance in the assertion processor as long as
it's not noticeably slow.

## Features

Here are some of the most notable features I'd like to highlight:

#### Automatic Expression Decomposition <!-- omit in toc -->
The most important feature this library supports is automatic expression decomposition. No need for
`ASSERT_LT` or other such hassle, `assert(vec.size() > 10);` is automatically understood, as
showcased above.

#### Expression Diagnostics <!-- omit in toc -->
Values involved in assert expressions are displayed. Redundant diagnostics like `7 => 7` are
avoided.

```cpp
assert(vec.size() > 7);
```

![](screenshots/d.png)

Only the full assert expression is able to be extracted from a macro call. Showing which parts of
the expression correspond to what values requires some basic expression parsing. C++ grammar is
ambiguous but most expressions can be disambiguated.

#### Extra Diagnostics <!-- omit in toc -->
Asserts, checks, and verifies support optional diagnostic messages as well as arbitrary other
diagnostic messages.

```cpp
FILE* f = VERIFY(fopen(path, "r") != nullptr, "Internal error with foobars", errno, path);
```

Special handling is provided for `errno`, and strerror is automatically called.

![](screenshots/f.png)

#### Stack Traces <!-- omit in toc -->

A lot of work has been put into generating pretty stack traces and formatting them as nicely as
possible.

One feature worth noting is that instead of always printing full paths, only the minimum number of
directories needed to differentiate paths are printed.

![](screenshots/i.png)

Another feature worth pointing out is that the stack traces will fold deep recursion traces:

![](screenshots/g.png)

#### Automatic Safe Comparisons <!-- omit in toc -->

Because expressions are already being automatically decomposed, signed-unsigned comparisons are
automatically done with sign safety (same mechanism as C++20 `std::cmp_equal`, `std::cmp_less`,
...):

```cpp
assert(18446744073709551606ULL == -10);
```

![](screenshots/h.png)

#### Syntax Highlighting <!-- omit in toc -->

The assertion handler applies syntax highlighting wherever appropriate, as seen in all the
screenshots above. This is to help enhance readability.

#### Object Printing <!-- omit in toc -->

A lot of care is given to printing values as effectively as possible: Strings, characters, numbers,
should all be printed as you'd expect. If a user defined type overloads `operator<<(std::ostream& o,
const S& s)`, that overload will be called. Otherwise it a default message will be printed.

![](screenshots/j.png)

![](screenshots/k.png)

Assertion values are printed in hex or binary as well as decimal if hex/binary are used on either
side of an assertion expression:

```cpp
assert(get_mask() == 0b00001101);
```

![](screenshots/e.png)

## Documentation

The library provides a set of macros which effectively function as such:

```cpp
decltype(auto) assert(<expression>, [optional assertion message],
                                    [optional extra diagnostics, ...], fatal?);
decltype(auto) ASSERT(<expression>, [optional assertion message],
                                    [optional extra diagnostics, ...], fatal?);
decltype(auto) VERIFY(<expression>, [optional assertion message],
                                    [optional extra diagnostics, ...], fatal?);
void CHECK(<expression>, [optional assertion message],
                         [optional extra diagnostics, ...], fatal?);
```

The macros are all caps to conform with macro hygiene practice - "check" and "verify" they're likely
to conflict with other identifiers.

`-DASSERT_LOWERCASE` can be used to enable the `assert` alias for `ASSERT`. See:
[Replacing &lt;cassert&gt;](#replacing-cassert).

### Parameters

#### `expression` <!-- omit in toc -->

The `<expression>` is automatically decomposed so diagnostic information can be provided. The
resultant type must be convertible to boolean.

The operation between left and right hand sides of the top-level operation in the expression tree is
evaluated by a functor.

If the operation is a comparison (`==`, `!=`, `<`, `<=`, `>`, `>=`) and the operands are integers,
the comparison is automatically done with sign safety.

Note: Boolean logical operators (`&&` and `||`) are not decomposed by default due to short
circuiting.

#### `assertion message` <!-- omit in toc -->

An optional assertion message may be provided. If the first argument following `<expression>` is any
string type it will be used as the message (if you want the first parameter, which happens to be a
string, to be an extra diagnostic value instead simply pass an empty string first, i.e.
`assert(foo, "", str);`).

#### `extra diagnostics` <!-- omit in toc -->

An arbitrary number of extra diagnostic values may be provided. These are displayed below the
expression diagnostics if a check fails.

There is special handling when `errno` is provided: The value of `strerror` is displayed
automatically.

#### `fatal?` <!-- omit in toc -->

`ASSERT::FATAL` and `ASSERT::NONFATAL` may be passed in any position in a call. By default asserts,
verifies, and checks are fatal. If nonfatal, failure will simply be logged but abort isn't called
and exceptions aren't raised.

### Return value

To facilitate ease of integration into code, `ASSERT` and `VERIFY` return a value from the assert
expression. The returned value is the following:

- If there is no top-level binary operation (e.g. as in `assert(foo());` or `assert(false);`) in the
  `<expression>`, the value of the expression is simply returned.
- Otherwise if the top-level binary operation is `==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, or
  or any assignment or compound assignment then the value of the __left-hand operand__ is returned.
- Otherwise if the top-level binary operation is `&`, `|`, `^`, `<<`, `>>`, or any binary operator
  with precedence above bitshift then value of the whole expression is returned.

I.e., `assert(foo() > 2);` returns the computed result from `foo()` and `assert(x & y);` returns the
computed result of `x & y`;

If the value from `<expression>` selected to be returned is an lvalue, the type of the
`ASSERT`/`VERIFY` call will be an lvalue reference. If the value from `<expression>` is an rvalue
then the type of the call will be an rvalue.

`CHECK` does not return anything as its expression is not evaluated at all under `-DNDEBUG`.

### Failure

After the assertion handler processes the failure and prints diagnostic information it will invoke
an assert failure action. These may be overridden by the user on a per-TU basis, the default
behaviors are:

| Name     | Failure |
| -------- | ------- |
| `ASSERT` / `assert` | `abort()` is called. In `-DNDEBUG`, fail path is marked unreachable. |
| `VERIFY` | `asserts::verification_failure` is thrown |
| `CHECK`  | `asserts::check_failure` is thrown |

### Configuration

The following can be used to set application-wide settings:

- `asserts::config::set_color_output(bool)` Enables or disables colored assertion messages on TTY
  outputs. It is thread-safe (not that that should ever matter).

The following configurations can be applied on a per-TU basis:

- `-DNDEBUG` Disables assertion checks for release (assertion conditions are assumed for the
  optimizer's benefit)
- `-DASSERT_DECOMPOSE_BINARY_LOGICAL` Enables expression decomposition of `&&` and `||`. **Note:
  This disables short-circuiting in assert expressions**. (If you really need short-circuiting in a
  condition in this mode, an extra set of parentheses can be used: `assert((ptr && ptr->foo));`.)
- `-DASSERT_LOWERCASE` Enables `assert` alias for `ASSERT`
- `-DASSERT_FAIL=fn` Allows a custom failure handler to be provided

Custom failure actions: These are called when an assertion fails after diagnostic messages are
printed. Set these macros to the name of the failure action function, signature is expected to be
`void custom_fail(asserts::assertion_printer&, asserts::assert_type, ASSERTION)`.
The `printer` is used to allow the failure handler to format to a desired width. It accepts zero and
negative widths to indicate the message should be printed to work on any width (i.e., no pretty
columns in the output). `type` is the type of the assertion and `fatal` indicates whether an
assertion is fatal. A typical implementation looks like:
```cpp
void custom_fail(asserts::assertion_printer& printer, asserts::assert_type type, ASSERTION fatal) {
	std::string message = printer(asserts::utility::terminal_width(STDERR_FILENO));
    if(isatty(STDERR_FILENO)) {
        std::cerr<<message<<std::endl;
    } else {
        std::cerr<<asserts::utility::strip_colors(message)<<std::endl;
    }
    using asserts::assert_type;
    if(fatal == ASSERT::FATAL) {
        switch(type) {
            case assert_type::assertion:
                abort();
            case assert_type::verify:
                throw asserts::verification_failure();
            case assert_type::check:
                throw asserts::check_failure();
            default:
                assert(false);
        }
    }
}
```

### Utilities

The following utilities are made public in `asserts::utility::`:
- `std::string strip_colors(const std::string& str)` Strips ansi sequences from a string.
- `int terminal_width(int fd)` Returns the width of the TTY referenced by the given file descriptor,
  or 0 on error.
- `std::string stacktrace(int width)` Generates a stack trace, formatted for the given width (0 for
  a width-independent formatting).

### Namespace synopsis

```cpp
// Macros:
#define ASSERT(...) ...
#define VERIFY(...) ...
#define CHECK(...) ...
#ifdef ASSERT_LOWERCASE
 #define assert(...) ...
#endif
namespace asserts {
	// Core functionality:
	enum class ASSERTION { NONFATAL, FATAL };
	class assertion_printer {
		public: [[nodiscard]] std::string operator()(int width);
	};
	struct verification_failure : std::exception {
		virtual const char* what() const noexcept final override;
	}
	struct check_failure : std::exception {
		virtual const char* what() const noexcept final override;
	}
	// Other functionality:
	enum class assert_type { assertion, verify, check };
	namespace utility {
		[[nodiscard]] std::string strip_colors(const std::string& str);
		[[nodiscard]] int terminal_width(int fd);
		[[nodiscard]] std::string stacktrace(int width);
	};
	namespace config {
		void set_color_output(bool);
	}
	namespace detail { /* internals */ }
}
using asserts::ASSERTION;
// All macros of the form ASSERT_DETAIL_* are reserved
```

## How To Use This Library

This library targets >=C++17 and supports gcc, clang, and msvc.

Note: The library does rely on some compiler extensions and compiler specific features so it is not
compatible with `-pedantic`.

1. Run `make` to compile static and shared libraries
2. Copy the static or shared library where you want it.
3. Copy [`include/assert.hpp`](include/assert.hpp) where you want it.
4. Add a `-I` path if needed, add a `-L` path if needed, link with the library (`-lassert`)
   - For the shared library you may need to add a path to your `LD_LIBRARY_PATH` environment
     variable.
   - If static linking, additionally link with dbghelp (`-ldbghelp`) on windows or lib dl (`-ldl`)
     on linux.

### 1. Build

Option 1: `make`. Parameters:
- `TARGET`: `release` (default) or `debug`
- `COMPILER`: `g++` by default, you can specify any path / binary name or `msvc`.

Option 2: `cmake`

### 2. Install

Put the header (`include/assert.hpp`) and library files (in `bin/`) in a location of your choice.

### 3. Use

- Setup include / library paths appropriately
- Link appropriately
- Debug symbols will be needed for good stack traces

Special notes for generating debug symbols:

| Compiler | Linux | Windows |
|--|--|--|
| GCC | - | - |
| Clang | - | .pdb needed, pass -g to the linker |
| MSVC | N/A| .pdb needed, pass /DEBUG to the linker |

## Replacing &lt;cassert&gt;

With `-DASSERT_LOWERCASE` this library can be used as a drop-in replacement for `<cassert>` but it
is important to be aware of two things:

- `assert(expr);` will still evaluate `expr` under `-DNDEBUG`. Side effects will still be present,
  though there probably should not be any side effects. If there are no side effects `expr` should
  be optimized away, at the very least during LTO.
- Defining `assert` is [technically not allowed][16.4.5.3.3] by the standard but thi should not be
  an issues on any sane compiler.

## Comparison With Other Languages

Even when standard libraries provide constructs like `assert_eq` they don't always do a good job of
providing helpful diagnostics. E.g. Rust where the left and right values are displayed but not the
expressions themselves:

```rust
fn main() {
    let count = 4;
    assert_eq!(count, 2);
}
```
```
thread 'main' panicked at 'assertion failed: `(left == right)`
  left: `4`,
 right: `2`', /app/example.rs:3:5
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
```

This is not as helpful as it could be.

Functionality other languages / their standard libraries provide:

|                        | C/C++ | Rust | C# | Java | Python | JavaScript | This Library |
|:--                     |:--:   |:--:  |:--:|:--:  |:--:    |:--:        |:--:|
| Expression string      | ✔️   | ❌   | ❌ | ❌  | ❌    | ❌         | ✔️ |
| Location               | ✔️   | ✔️   | ✔️ | ✔️  | ✔️    | ✔️         | ✔️ |
| Backtrace              | ❌   | ✔️   | ✔️ | ✔️  | ✔️    | ✔️         | ✔️ |
| Assertion message      | ❌   | ✔️   | ✔️ | ✔️  | ✔️    | ✔️         | ✔️ |
| Extra diagnostics      | ❌   | ❌*  | ❌*| ❌  | ❌*   | ❌*        | ✔️ |
| Binary specializations | ❌   | ✔️   | ❌ | ❌  | ❌    | ✔️         | ✔️ |
| Automatic expression decomposition | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |

`*`: Possible through string formatting but that is sub-ideal.

Extras:

|                 | C/C++ | Rust | C# | Java | Python | JavaScript | This Library |
|:--              |:--:  |:--:  |:--: |:--:  |:--:    |:--:        |:--:|
| Syntax Highlighting   | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
| Non-Fatal Assertions  | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
| Format Consistency    | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
| Expression strings and expression values everywhere | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
| Safe signed-unsigned comparison | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
| Return values from the assert to allow asserts to be integrated into expressions inline | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |

[16.4.5.3.3]: https://eel.is/c++draft/reserved.names#macro.names-1
