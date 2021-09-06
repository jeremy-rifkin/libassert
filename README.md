# Asserts

<p align="center">The most over-engineered assertion library.</p>

Asserts are sanity checks for programs: Validating the programmer's assumptions and helping identify
problems at their sources. Assertions should provide as much information and context to the
developer as possible to allow for speedy triage. In practice, we often have to re-run in a debugger
after hitting an assertion failure. E.g. after an assert such as `assert(n <= 12);` fails you don't
know anything about the value of `n`.

`assert_eq`, `assert_lteq`, ... and other related variants are common extensions to the standard
library's assert functionality, with mixed success.

Fail messages are very valuable. They can explain the purpose of an assertion and what it means if
the assertion is failing. Often this is free, we would be documenting the purpose of an assert in a
comment anyway. We aren't able to provide an associated information message with asserts with
out-of-the-box C asserts either (`assert(("this should be unreachable", false));` is not an option
because of `-Wunused-value`).

Throughout languages a common theme exists: assertions are very lightweight (even in the cold fail
paths). Why? Their purpose is to provide diagnostic info, lightweight does not matter. **Thesis:**
Let's see how much helpful information and functionality we can add to assertions while still
maintaining ease of use for the developer.

### Functionality This Library Provides

- Optional info messages
- Non-fatal assertion option
- `assert_eq` and variants for `!=`, `<`, `>`, `<=`, `>=`, `&&`, and `||`
- Expression strings and values are displayed
  - Intelligent display of values (e.g. doesn't display `1 => 1` or other such redundant
    expression-value pairs)
- Smart parenthesization of re-constructed comparisons
- Robust value printing (attempts to display a a wide variety of types most effectively and supports user-defined types)
- Pretty printing
- Rough syntax highlighting because fuck it why not
- Try to provide format consistency (E.g.: if a comparison involves an expression and a hex literal,
  the values of the left and right side are printed )

Demo: (note that the call to `abort();` on assertion failure is commented out for this demo)
```cpp
assert(false, "this should be unreachable");
assert(false);
```
![](screenshots/a.png)
```cpp
// In this example, note that important values are displayed but there's no redundant "2 => 2"
assert_eq(map.count(1), 2);
assert_gteq(map.count(2 * bar()), 2, "some data not received");
```
![](screenshots/b.png)
![](screenshots/c.png)
```cpp
// In this example, note that precision issue is displayed but there's no redundant "2 => 2"
assert_eq(.1, 2);
assert_eq(.1f, .1);
```
![](screenshots/d.png)
![](screenshots/e.png)
```cpp
// In this example, note that parentheses are automatically added in on the right-side by the assertion processor to make the output correct
assert_eq(0, 2 == bar());
```
![](screenshots/f.png)
```cpp
// Note again no redundant literals and also string escaping
std::string s = "test";
assert_eq(s, "test2");
assert_eq(s[0], 'c');
assert_eq(BLUE "test" RESET, "test2");
```
![](screenshots/g.png)
```cpp
// Numbers are always printed in decimal but this expression involves other representations too: hex and binary. So the hex and binary forms are also displayed.
assert_eq(0b1000000, 0x3);
```
![](screenshots/h.png)
```cpp
template<class T> struct S {
    T x;
    S() = default;
    S(T&& x) : x(x) {}
    bool operator==(const S& s) const { return x == s.x; }
    friend std::ostream& operator<<(std::ostream& o, const S& s) {
        o<<"I'm S<"<<assert_impl_::type_name<T>()<<"> and I contain:"<<std::endl;
        // to-string on s.x
        std::ostringstream oss; oss<<s.x;
        // print contents, assert_impl_::indent is just a string utility to indent all lines in a string
        o<<assert_impl_::indent(std::move(oss).str(), 4);
        return o;
    }
};
assert_eq(S<S<int>>(2), S<S<int>>(4));
```
![](screenshots/i.png)
```cpp
template<> struct S<void> {
    bool operator==(const S&) const { return false; }
};
// For a user-defined type with no stringification the assertion processor will fallback to type info
S<void> e, f;
assert_eq(e, f);
```
![](screenshots/j.png)

**A note on performance:** A lot of logic is required to make some of these assertions happen. There
is a lot of inefficient string manipulation and io operations. The impact of `assert`s at callsites
is still minimal, the fail paths are marked `cold` and all heavy logic occurs in the fail paths.
While a lot of work is required to process a failed assertion as long it doesn't take a noticeable
amount of time it's fine. After all, assertion fails are *the coldest* path in a binary.

What I'd like to add and improve on further:
- Backtraces (a feature of every language's asserts *except* C/C++)
- Better syntax highlighting (difficult because C++ is not context-free)
- Allow extra objects to be provided and dumped (nodejs allows this)
- I think it would be really cool if we could enable the user to automatically, with the press of a
  button, attach gdb at the assertion fail point. It is tricky to implement this, though.

Possible pitfalls of this library:
- If there's a bug in the assert processing logic (e.g. something that could cause a crash) the
  purpose of this library would be defeated. This is a concern
- Library tries to be smart and stringify values aggressively. If an assert expression is of type
  `char*`, the library will try to print the string value. Fine in most cases, not fine if the
  result is a non-null pointer to a non-c string (e.g. a binary buffer).

### Library Documentation

Assertions are of the form:

- `void assert(expression, info?, fatal?)`
- `void assert_op(a, b, info?, fatal?)`
  - Where `op` is one of `eq`, `neq`, `lt`, `gt`, `lteq`, `gteq`, `and`, or `or`.

`info` is optional has overloads for `char*` and `std::string`.

`fatal` is optional and is `ASSERT::FATAL` or `ASSERT::NONFATAL`.

Build options:

- `-DNCOLOR` Turns off colors
- `-DNDEBUG` Disables assertions
- `-DASSERT_DEMO` Makes all assertions non-fatal

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

|                 | C/C++ | Rust | C# | Java | Python | JavaScript | This Library |
|:--:             |:--:  |:--:  |:--: |:--:  |:--:    |:--:        |:--:|
| Expression      | ✔️   | ❌   | ❌ | ❌  | ❌    | ❌         | ✔️ |
| Location        | ✔️   | ✔️   | ✔️ | ✔️  | ✔️    | ✔️         | ✔️ |
| Backtrace       | ❌   | ✔️   | ✔️ | ✔️  | ✔️    | ✔️         | TODO |
| Info Message    | ❌   | ✔️   | ✔️ | ✔️  | ✔️    | ✔️         | ✔️ |
| Values Involved | ❌   | ✔️   | ❌ | ❌  | ❌    | ✔️         | ✔️ |

Extras:

|                 | C/C++ | Rust | C# | Java | Python | JavaScript | This Library |
|:--:             |:--:  |:--:  |:--: |:--:  |:--:    |:--:        |:--:|
| Automatically Attach GDB At Failure Point | ❌   | ❌   | ❌ | ❌  | ❌    | ❌         | Will investigate further |
| Syntax Highlighting   | ❌   | ❌   | ❌ | ❌  | ❌    | ❌         | ✔️ |
| Non-Fatal Assertions  | ❌   | ❌   | ❌ | ❌  | ❌    | ❌         | ✔️ |
| Format Consistency    | ❌   | ❌   | ❌ | ❌  | ❌    | ❌         | ✔️ |
| Automatic expression decomposition | ❌   | ❌   | ❌ | ❌  | ❌    | ❌         | TODO |

Automatic expression decomposition (automatically understanding a binary comparison like
`assert(a == b);` instead of having to use a macro like `assert_eq`) is something I'd expect to
require language support (and even Rust's macro system doesn't allow inspecting an expression tree).
It turns out there's a [cool trick lest uses][lest trick] that enables this in C++ with the help of
macro abuse.

[lest trick]: https://github.com/martinmoene/lest/blob/master/include/lest/lest.hpp#L829-L853
