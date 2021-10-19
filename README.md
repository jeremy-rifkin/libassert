# Asserts <!-- omit in toc -->

<p align="center">The most over-engineered assertion library.</p>
<p align="center"><i>"Did you just implement syntax highlighting for an assertion library??"</i>
                      - My Russian friend Oleg</p>

**Summary:** Automatic expression decomposition, diagnostics on binary expressions, assert messages,
extra diagnostic values, stack traces, syntax highlighting, errno help, and more!

```cpp
assert(some_system_call(fd, buffer1, n) > 0, "Internal error with foobars", errno, fd, n);
```
![](screenshots/b.png)

**The Problem:**

Asserts are sanity checks for developers: Validating assumptions and helping identify problems at
their sources. Assertions should prioritize providing as much information and context to the
developer as possible to allow for speedy triage. Unfortunately throughout existing languages and
tooling a common theme exists: Assertions are very minimal and when `assert(n <= 12);` fails we get
no information about the value of `n`. There is no reason assertions should be excessively
lightweight.

This library is an exploration looking at how much helpful information and functionality we can pack
into assertions while still maintaining ease of use for the developer.

**The Ideal:**

Ideally assertions can do all of the following:
- Provide expression strings.
- Provide values involved in binary expressions, such as `assert(count > 0);`.
- Provide failure location and a stacktrace.
- Display values in useful formats.
- Support an optional diagnostic message (make assertions self documenting).
- Support extra diagnostic information being provided.

`cassert`/`assert.h` can't do most of these. No tool I know of can do all these, other than this
tool 😎 It's fair to say that this is the most overpowered assertion library out there and one of
the few libraries that can claim to be more bloated than Boost.

#### Table of Contents: <!-- omit in toc -->
- [Functionality This Library Provides](#functionality-this-library-provides)
- [Quick Library Documentation](#quick-library-documentation)
- [How To Use This Library](#how-to-use-this-library)
- [Comparison With Other Languages](#comparison-with-other-languages)

### Functionality This Library Provides

- Optional assertion messages
- Non-fatal assertions option
- `assert_eq` and variants for `!=`, `<`, `>`, `<=`, `>=`, `&&`, and `||`.
- **Automatic expression decomposition:** `assert(foo() == bar());` is automatically understood as
  `assert_eq(foo(), bar());`. `assert_eq` and variants may be deprecated once support for automatic
  decomposition improves.
  - Displaying good diagnostic info here requires some attempt to parse C++ expression grammar,
    which is ambiguous without type info.
- Comprehensive stringification (attempts to display a wide variety of types effectively and
  supports user-defined types).
- Smart diagnostic info
  - `1 => 1` and other such redundant expression-value diagnostics are not displayed.
  - The library tries to provide format consistency: If a comparison involves an expression and a
    hex literal, the values of the left and right side are printed in both decimal and hex.
- Support for providing extra diagnostic information.
- Automatic `strerror` for `errno`.
- Syntax highlighting, because why not!
- Signed-unsigned comparison is always done safely by the assertion processor.
- Custom assertion failure action.
- Optional assert assumptions in release mode.

Demo: (note that the call to `abort();` on assertion failure is commented out for this demo)
```cpp
assert(false, "Error while doing XYZ"); // optional assert message
assert(false);
```
![](screenshots/a.png)
```cpp
// Diagnostics omit redundant "2 => 2"
assert(map.count(1) == 2);
assert(map.count(1) >= 2 * garple(), "Error while doing XYZ");
```
![](screenshots/c.png)
```cpp
// Floating point stringificaiton done carefully to provide the most helpful diagnostic info
assert(1 == 1.5); // not stringified here, it would be redundant
assert(0.1 + 0.2 == 0.3); // stringified here to expose rounding error
```
![](screenshots/e.png)

```cpp
// Numbers are always printed in decimal but the assertion processor will also print binary, hex,
// or octal when they might be relevant. Here it will print decimal, binary, and hex because those
// are the literal formats involved.
assert_eq(1, 1 bitand 2);
assert(18446744073709551606ULL == -10); // signed-unsigned comparisons are always done safely
assert(mask bitand flag);
assert(0xf == 16);
```
![](screenshots/f.png)

```cpp
// Same care is taken with strings: No redundant diagnostics and strings are also escaped.
assert(s == "test2");
assert(s[i] == 'c', "", s, i);
assert(BLUE "test" RESET == "test");
// The assertion processor takes care not to segfault when attempting to stringify
assert_eq(buffer, thing);
```
![](screenshots/g.png)
```cpp
// S<T> has a custom printer (i.e. an std::ostream<< friend)
assert(S<S<int>>(2) == S<S<int>>(4));
S<void> e, f; // S<void> doesn't have a printer
assert(e == f);
```
![](screenshots/i.png)

**A note on performance:** I've kept the impact of `assert`s at callsites minimal. A lot of logic is
required to process assertion failures once they happen but failures are *the coldest* path in a
binary, I'm not concerned with performance in the assertion processor as long as it's not noticeably
slow. Automatic expression decomposition requires a lot of template shenanigans which is not free.

**A note on automatic expression decomposition:** In automatic decomposition the assertion processor
is only able to obtain a the string for the full expression instead of the left and right parts
independently. Because of this the library needs to do some basic expression parsing, just figuring
out the very top-level of the expression tree. Unfortunately C++ grammar is ambiguous without type
information. The assertion processor is able to disambiguate many expressions but will return
`{"left", "right"}` if it's unable to. Disambiguating expressions is currently done by essentially
traversing all possible parse trees. There is probably a more optimal way to do this.

### Quick Library Documentation

Library functions:

```cpp
void assert(<expression>, [optional assertion message], [optional extra diagnostics, ...]);

void ASSERT(<expression>, [optional assertion message], [optional extra diagnostics, ...]);
void ASSERT_OP(left, right, [optional assertion message], [optional extra diagnostics, ...]);

T&& VERIFY(<expression>, [optional assertion message], [optional extra diagnostics, ...]);
T&& VERIFY_OP(left, right, [optional assertion message], [optional extra diagnostics, ...]);

// Where `op` ∈ {`eq`, `neq`, `lt`, `gt`, `lteq`, `gteq`, `and`, `or`}.
```
The `<expression>` is automatically decomposed so diagnostic information can be printed for the left
and right sides. The resultant type must be convertible to boolean.

An optional assertion message may be provided. If the first argument following `<expression>` or
`left, right` is any string type it will be used as the message (if you want the first parameter,
which happens to be a string, to be an extra diagnostic simply pass an empty string first).

An aribtray number of extra diagnostic values may be provided. There is special handling when
`errno` is provided, `strerror` is called automatically. `ASSERT::FATAL` and `ASSERT::NONFATAL` may
be passed in any position (controlling whether the fail function is called).

`VERIFY` assertions are fatal but are not disabled with `NDEBUG`. They return the result of the
`<expression>` or the invocation of the comparison between `left` and `right`. One place this is
useful is verifying an std::optional has a value, similar to Rust's .unwrap():
```cpp
if(auto bar = *VERIFY(foo())) {
    ...
}
```

`assert` is provided as an alias for `ASSERT` to be compatible with C's `assert.h`.

**Note:** There is no short-circuiting for `ASSERT_AND` and `ASSERT_OR` or `&&` and `||` in
expression decomposition.

**Note:** Top-level integral comparisons are automatically done with sign safety.

**Note:** left and right hand types in automatically decomposed expressions require move semantics.

Build options:

- `-DNCOLOR` Turns off colors
- `-DNDEBUG` Disables assertions
- `-DASSUME_ASSERTS` Makes assertions serve as optimizer hints in `NDEBUG` mode. *Note:* This is not
  always a win. Sometimes assertion expressions have side effects that are undesirable at runtime in
  an `NDEBUG` build like exceptions which cannot be optimized away (e.g. `std::unordered_map::at`
  where the lookup cannot be optimized away and ends up not being a helpful compiler hint).
- `-DASSERT_FAIL_ACTION=...` Can be used to specify what is done on assertion failure, after
  diagnostic printing, e.g. throwing an exception instead of calling `abort()` which is the default.
  The function should have signature `void()`.

### How To Use This Library

This library targets >=C++17 and supports gcc and clang on windows and linux. This library is no
longer single header due to compile times.

1. Run `make` to compile static and shared libraries
2. Copy the static or shared library where you want it.
3. Copy [`include/assert.hpp`](include/assert.hpp) where you want it.
4. Add a `-I` path if needed, add a `-L` path if needed, link with the library (`-lassert`)
   - For the shared library you may need to add a path to your `LD_LIBRARY_PATH` environment
     variable.
   - If static linking, additionally link with dbghelp (`-ldbghelp`) on windows or lib dl (`-ldl`)
     on linux.

### Comparison With Other Languages

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
| Automatically Attach GDB At Failure Point | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | Will investigate further |
| Syntax Highlighting   | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
| Non-Fatal Assertions  | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
| Format Consistency    | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
| Safe signed-unsigned comparison | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
