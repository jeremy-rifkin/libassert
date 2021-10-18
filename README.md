# Asserts <!-- omit in toc -->

<p align="center">The most over-engineered assertion library.</p>
<p align="center"><i>"Did you just implement syntax highlighting for an assertion library??"</i>
                      - My Russian friend Oleg</p>

**Summary:** Automatic expression decomposition, diagnostics on binary expressions, user-provided
extra diagnostics stack traces, syntax highlighting, errno help, info messages, and more!

```cpp
assert(map.count(1) == 2);
```
![](screenshots/b.png)

**The Problem:**

Asserts are sanity checks for programs: Validating the programmer's assumptions and helping identify
problems at their sources. Assertions should provide as much information and context to the
developer as possible to allow for speedy triage. In practice, we often have to re-run in a debugger
after hitting an assertion failure. E.g. after an assert such as `assert(n <= 12);` fails you don't
know anything about the value of `n`.

`assert_eq`, `assert_lteq`, ... and other related variants are common extensions to the standard
library's assert functionality but they don't put much effort into displaying diagnostics most
effectively. Furthermore, ideally the programmer should be able to just write `assert(count > 0);`
and the language / library will provide diagnostic information for them.

Fail messages are valuable: They allow the programmer to explain the purpose of an assertion and
what it means if the assertion is failing. Adding these messages is often zero-cost, we would be
documenting the purpose of an assert in a comment anyway. While an optional message is allowed in
`static_assert`, we aren't able to provide an associated information message with asserts with
out-of-the-box assert.h/cassert asserts.

Throughout languages a common theme exists: Assertions are very minimal (even in the cold fail
paths). Why? Their purpose is to provide diagnostic info, being lightweight does not matter.
**Thesis:** Let's see how much helpful information and functionality we can add to assertions while
still maintaining ease of use for the developer.

#### Table of Contents: <!-- omit in toc -->
- [Functionality This Library Provides](#functionality-this-library-provides)
- [Quick Library Documentation](#quick-library-documentation)
- [How To Use This Library](#how-to-use-this-library)
- [Comparison With Other Languages](#comparison-with-other-languages)

### Functionality This Library Provides

- Optional info messages
- Non-fatal assertion option
- `assert_eq` and variants for `!=`, `<`, `>`, `<=`, `>=`, `&&`, and `||`.
- **Automatic expression decomposition:** `assert(foo() == bar());` is automatically understood as
  `assert_eq(foo(), bar());`. `assert_eq` and variants may be deprecated once support for automatic
  decomposition improves.
  - Displaying good diagnostic info here requires some attempt to parse C++ expression grammar,
    which is ambiguous without type info.
- Diagnostic info: Show the values of parts of the assert expression.
  - Comprehensive stringification (attempts to display a wide variety of types effectively and
  	supports user-defined types).
- Smart diagnostic info
  - `1 => 1` and other such redundant expression-value diagnostics are not displayed.
  - The library tries to provide format consistency: If a comparison involves an expression and a
    hex literal, the values of the left and right side are printed in both decimal and hex.
- Smart parenthesization: Re-constructed expressions from `assert_...` have parentheses
  automatically inserted if it would help readability or be otherwise important for precedence.
- Rough syntax highlighting because why not!
- Signed-unsigned comparison is always done safely by the assertion processor.

Demo: (note that the call to `abort();` on assertion failure is commented out for this demo)
```cpp
assert(false, "code should never do <xyz>"); // optional diagnostic message
assert(false);
```
![](screenshots/a.png)
```cpp
// Note below that important values are displayed but there's no redundant "2 => 2"
assert(map.count(1) == 2);
assert(map.count(1) >= 2 * bar(), "some data not received");
```
![](screenshots/b.png)
![](screenshots/c.png)
```cpp
// Floating point stringificaiton done carefully to provide the most helpful diagnostic info
assert(.1f == .1);
```
![](screenshots/e.png)
```cpp
// Parentheses are automatically added in the by the assertion processor to make the output correct and readable
assert_eq(0, 2 == bar());
```
![](screenshots/f.png)
```cpp
// Same care is taken with strings and characters: No redundant diagnostics and strings are escaped.
std::string s = "test";
assert(s == "test2");
assert(s[0] == 'c');
assert(BLUE "test" RESET == "test2");
// Note with this that the processor takes care not to segfault when attempting to stringify
char* buffer = nullptr;
char thing[] = "foo";
assert_eq(buffer, thing);
```
![](screenshots/g.png)
![](screenshots/k.png)
```cpp
// Numbers are always printed in decimal but the assertion processor will also print binary, hex,
// or octal when they might be relevant. Here it will print decimal, binary, and hex because those
// are the literal formats involved.
assert(0b1000000 == 0x3);
```
![](screenshots/h.png)
```cpp
template<class T> struct S {
    T x;
    S() = default;
    S(T&& x) : x(x) {}
    bool operator==(const S& s) const { return x == s.x; }
    friend std::ostream& operator<<(std::ostream& o, const S& s) {
        o<<"I'm S<"<<assert_detail::type_name<T>()<<"> and I contain:"<<std::endl;
        // to-string on s.x
        std::ostringstream oss; oss<<s.x;
        // print contents, assert_detail::indent is just a string utility to indent all lines in a string
		// the assert logic does its own indentation, too
        o<<assert_detail::indent(std::move(oss).str(), 4);
        return o;
    }
};
assert(S<S<int>>(2) == S<S<int>>(4));
```
![](screenshots/i.png)
```cpp
template<> struct S<void> {
    bool operator==(const S&) const { return false; }
};
// For a user-defined type with no stringification the assertion processor will fallback to type info
S<void> e, f;
assert(e == f);
```
![](screenshots/j.png)

**A note on performance:** I've kept the impact of `assert`s at callsites minimal. A lot of logic is
required to process assertion failures once they happen. Because assertion fails are *the coldest*
path in a binary, I'm not concerned with performance in the assertion processor as long as it's not
noticeably slow.

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
void assert(<expression>, Args...);
void wassert(<expression>, Args...);
void assert_op(left, right, Args...);
void wassert_op(left, right, Args...);
// Where `op` ∈ {`eq`, `neq`, `lt`, `gt`, `lteq`, `gteq`, `and`, `or`}.
```
The `<expression>` is automatically decomposed so diagnostic information can be printed for the left
and right sides. The resultant type must be convertible to boolean.

Extra diagnostic information may be provided in `Args...` (e.g. `errno`). If the first argument in
`Args...` is any string type it'll be used for the assertion message. `ASSERT::FATAL` and
`ASSERT::NONFATAL` may also be passed in any position.

`wassert` and `wassert_op` are shorthands for non-fatal assertions.

**Note:** There is no short-circuiting for `assert_and` and `assert_or` or `&&` and `||` in
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

1. Copy the header file [`include/assert.hpp`](include/assert.hpp) somewhere in your include path.
2. Link
   - On windows link with dbghelp (`-ldbghelp`).
   - On linux or windows with mingw link with lib dl (`-ldl`)

This library targets >=C++17 and supports gcc and clang on windows and linux.

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

Assertions should:
- Provide context.
  - E.g.: Expression, location, backtrace.
- Allow the programmer to provide an associated info message.
- If a comparison fails, provide the values involved.

|                        | C/C++ | Rust | C# | Java | Python | JavaScript | This Library |
|:--:                    |:--:   |:--:  |:--:|:--:  |:--:    |:--:        |:--:|
| Expression String      | ✔️   | ❌   | ❌ | ❌  | ❌    | ❌         | ✔️ |
| Location               | ✔️   | ✔️   | ✔️ | ✔️  | ✔️    | ✔️         | ✔️ |
| Backtrace              | ❌   | ✔️   | ✔️ | ✔️  | ✔️    | ✔️         | TODO |
| Info Message           | ❌   | ✔️   | ✔️ | ✔️  | ✔️    | ✔️         | ✔️ |
| Extra diagnostics      | ❌   | ❌*  | ❌*| ❌  | ❌*   | ❌*        | ✔️ |
| Binary specializations | ❌   | ✔️   | ❌ | ❌  | ❌    | ✔️         | ✔️ |
| Automatic expression decomposition | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |

`*`: Possible through string formatting but that is sub-ideal.

Extras:

|                 | C/C++ | Rust | C# | Java | Python | JavaScript | This Library |
|:--:             |:--:  |:--:  |:--: |:--:  |:--:    |:--:        |:--:|
| Automatically Attach GDB At Failure Point | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | Will investigate further |
| Syntax Highlighting   | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
| Non-Fatal Assertions  | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
| Format Consistency    | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
| Safe signed-unsigned comparison | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✔️ |
