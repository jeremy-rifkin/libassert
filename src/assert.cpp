#define LIBASSERT_IS_CPP
#ifndef _CRT_SECURE_NO_WARNINGS
// NOLINTNEXTLINE(bugprone-reserved-identifier, cert-dcl37-c, cert-dcl51-cpp)
#define _CRT_SECURE_NO_WARNINGS // done only for strerror
#endif
#include "assert.hpp"

// Copyright (c) 2021-2023 Jeremy Rifkin under the MIT license
// https://github.com/jeremy-rifkin/libassert

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cctype>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string_view>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <cpptrace/cpptrace.hpp>

bool operator==(const cpptrace::stacktrace_frame& a, const cpptrace::stacktrace_frame& b) {
    return a.address == b.address
        && a.line == b.line
        && a.col == b.col
        && a.filename == b.filename
        && a.symbol == b.symbol;
}

bool operator!=(const cpptrace::stacktrace_frame& a, const cpptrace::stacktrace_frame& b) {
    return !(a == b);
}

#define IS_WINDOWS 0
#define IS_LINUX 0

#if defined(_WIN32)
 #undef IS_WINDOWS
 #define IS_WINDOWS 1
 #ifndef STDIN_FILENO
  #define STDIN_FILENO _fileno(stdin)
  #define STDOUT_FILENO _fileno(stdout)
  #define STDERR_FILENO _fileno(stderr)
 #endif
 #include <windows.h>
 #include <conio.h>
 #include <io.h>
 #include <process.h>
 #undef min // fucking windows headers, man
 #undef max
#elif defined(__linux)
 #undef IS_LINUX
 #define IS_LINUX 1
 #include <sys/ioctl.h>
 #include <unistd.h>
 // NOLINTNEXTLINE(misc-include-cleaner)
 #include <climits> // MAX_PATH
#else
 #error "no"
#endif

#if LIBASSERT_IS_MSVC
 // wchar -> char string warning
 #pragma warning(disable : 4244)
#endif

#define ESC "\033["
#define ANSIRGB(r, g, b) ESC "38;2;" #r ";" #g ";" #b "m"
#define RESET ESC "0m"
// Slightly modified one dark pro colors
// Original: https://coolors.co/e06b74-d19a66-e5c07a-98c379-62aeef-55b6c2-c678dd
// Modified: https://coolors.co/e06b74-d19a66-e5c07a-90cc66-62aeef-56c2c0-c678dd
#define RED    ANSIRGB(224, 107, 116)
#define ORANGE ANSIRGB(209, 154, 102)
#define YELLOW ANSIRGB(229, 192, 122)
#define GREEN  ANSIRGB(150, 205, 112) // modified
#define BLUE   ANSIRGB(98,  174, 239)
#define CYAN   ANSIRGB(86,  194, 192) // modified
#define PURPL  ANSIRGB(198, 120, 221)

// TODO
#define RED_ALT    ESC "31m"
#define ORANGE_ALT ESC "33m" // using yellow as orange
#define YELLOW_ALT ESC "34m" // based off use (identifiers in scope res) it's better to color as blue here
#define GREEN_ALT  ESC "32m"
#define BLUE_ALT   ESC "34m"
#define CYAN_ALT   ESC "36m"
#define PURPL_ALT  ESC "35m"

using namespace std::string_literals;
using namespace std::string_view_literals;

// Container utility
template<typename N> class small_static_map {
    // TODO: Re-evaluate
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const N& needle;
public:
    small_static_map(const N& n) : needle(n) {}
    template<typename K, typename V, typename... Rest>
    constexpr V lookup(const K& option, const V& result, const Rest&... rest) {
        if(needle == option) { return result; }
        if constexpr(sizeof...(Rest) > 0) { return lookup(rest...); }
        else { LIBASSERT_PRIMITIVE_ASSERT(false); LIBASSERT_UNREACHABLE; }
    }
    constexpr bool is_in() { return false; }
    template<typename T, typename... Rest>
    constexpr bool is_in(const T& option, const Rest&... rest) {
        if(needle == option) { return true; }
        return is_in(rest...);
    }
};

// Needed forward declaration
// TODO: Maybe just move all of this stuff out of the namespace
namespace libassert::detail {
    static void replace_all(std::string& str, std::string_view substr, std::string_view replacement);
}

namespace libassert::utility {
    LIBASSERT_ATTR_COLD
    std::string strip_colors(const std::string& str) {
        static const std::regex ansi_escape_re("\033\\[[^m]+m");
        return std::regex_replace(str, ansi_escape_re, "");
    }

    LIBASSERT_ATTR_COLD
    std::string replace_rgb(std::string str) {
        for(const auto& [rgb, alt] : std::initializer_list<std::pair<std::string_view, std::string_view>>{
            { RED, RED_ALT },
            { ORANGE, ORANGE_ALT },
            { YELLOW, YELLOW_ALT },
            { GREEN, GREEN_ALT },
            { BLUE, BLUE_ALT },
            { CYAN, CYAN_ALT },
            { PURPL, PURPL_ALT }
        }) {
            detail::replace_all(str, rgb, alt);
        }
        return str;
    }

    // https://stackoverflow.com/questions/23369503/get-size-of-terminal-window-rows-columns
    LIBASSERT_ATTR_COLD int terminal_width(int fd) {
        if(fd < 0) {
            return 0;
        }
        #ifdef _WIN32
         DWORD windows_handle = small_static_map(fd).lookup(
             STDIN_FILENO, STD_INPUT_HANDLE,
             STDOUT_FILENO, STD_OUTPUT_HANDLE,
             STDERR_FILENO, STD_ERROR_HANDLE
         );
         CONSOLE_SCREEN_BUFFER_INFO csbi;
         HANDLE h = GetStdHandle(windows_handle);
         if(h == INVALID_HANDLE_VALUE) { return 0; }
         if(!GetConsoleScreenBufferInfo(h, &csbi)) { return 0; }
         return csbi.srWindow.Right - csbi.srWindow.Left + 1;
        #else
         struct winsize w;
         // NOLINTNEXTLINE(misc-include-cleaner)
         if(ioctl(fd, TIOCGWINSZ, &w) == -1) { return 0; }
         return w.ws_col;
        #endif
    }
}

namespace libassert::config {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::atomic_bool output_colors = true;

    LIBASSERT_ATTR_COLD void set_color_output(bool enable) {
        output_colors = enable;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::atomic_bool output_rgb = true;

    LIBASSERT_ATTR_COLD void set_rgb_output(bool enable) {
        output_rgb = enable;
    }
}

namespace libassert::detail {
    LIBASSERT_ATTR_COLD
    void primitive_assert_impl(
        bool condition,
        bool verify,
        const char* expression,
        const char* signature,
        source_location location,
        const char* message
    ) {
        if(!condition) {
            const char* action = verify ? "Verification" : "Assertion";
            const char* name   = verify ? "verify"       : "assert";
            if(message == nullptr) {
                (void)fprintf(
                    stderr,
                    "%s failed at %s:%d: %s\n",
                    action,
                    location.file,
                    location.line,
                    signature
                );
            } else {
                (void)fprintf(
                    stderr,
                    "%s failed at %s:%d: %s: %s\n",
                    action,
                    location.file,
                    location.line,
                    signature,
                    message
                );
            }
            (void)fprintf(stderr, "    primitive_%s(%s);\n", name, expression);
            // NOLINTNEXTLINE(misc-include-cleaner)
            abort();
        }
    }

    // Still present in release mode, nonfatal
    #define internal_verify(c, ...) primitive_assert_impl(c, true, #c, LIBASSERT_PFUNC, {}, ##__VA_ARGS__)

    /*
     * string utilities
     */

    template<typename... T>
    LIBASSERT_ATTR_COLD
    std::string stringf(T... args) {
        const int length = snprintf(nullptr, 0, args...);
        if(length < 0) { LIBASSERT_PRIMITIVE_ASSERT(false, "Invalid arguments to stringf"); }
        std::string str(length, 0);
        (void)snprintf(str.data(), length + 1, args...);
        return str;
    }

    // to save template instantiation work in TUs a variadic stringf is used
    LIBASSERT_ATTR_COLD
    std::string bstringf(const char* format, ...) {
        va_list args1;
        va_list args2;
        va_start(args1, format);
        va_start(args2, format);
        const int length = vsnprintf(nullptr, 0, format, args1);
        if(length < 0) { LIBASSERT_PRIMITIVE_ASSERT(false, "Invalid arguments to stringf"); }
        std::string str(length, 0);
        (void)vsnprintf(str.data(), length + 1, format, args2);
        va_end(args1);
        va_end(args2);
        return str;
    }

    LIBASSERT_ATTR_COLD
    static std::vector<std::string> split(std::string_view s, std::string_view delims) {
        std::vector<std::string> vec;
        size_t old_pos = 0;
        size_t pos = 0;
        while((pos = s.find_first_of(delims, old_pos)) != std::string::npos) {
            vec.emplace_back(s.substr(old_pos, pos - old_pos));
            old_pos = pos + 1;
        }
        vec.emplace_back(s.substr(old_pos));
        return vec;
    }

    template<typename C>
    LIBASSERT_ATTR_COLD
    static std::string join(const C& container, const std::string_view delim) {
        auto iter = std::begin(container);
        auto end = std::end(container);
        std::string str;
        if(std::distance(iter, end) > 0) {
            str += *iter;
            while(++iter != end) {
                str += delim;
                str += *iter;
            }
        }
        return str;
    }

    constexpr const char * const ws = " \t\n\r\f\v";

    LIBASSERT_ATTR_COLD
    static std::string_view trim(const std::string_view s) {
        const size_t l = s.find_first_not_of(ws);
        const size_t r = s.find_last_not_of(ws) + 1;
        return s.substr(l, r - l);
    }

    LIBASSERT_ATTR_COLD
    static void replace_all_dynamic(std::string& str, std::string_view text, std::string_view replacement) {
        std::string::size_type pos = 0;
        while((pos = str.find(text.data(), pos, text.length())) != std::string::npos) {
            str.replace(pos, text.length(), replacement.data(), replacement.length());
            // advancing by one rather than replacement.length() in case replacement leads to
            // another replacement opportunity, e.g. folding > > > to >> > then >>>
            pos++;
        }
    }

    LIBASSERT_ATTR_COLD
    static void replace_all(std::string& str, const std::regex& re, std::string_view replacement) {
        std::smatch match;
        std::size_t i = 0;
        // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
        while(std::regex_search(str.cbegin() + i, str.cend(), match, re)) {
            str.replace(i + match.position(), match.length(), replacement);
            i += match.position() + replacement.length();
        }
    }

    LIBASSERT_ATTR_COLD
    static void replace_all(std::string& str, std::string_view substr, std::string_view replacement) {
        std::string::size_type pos = 0;
        // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
        while((pos = str.find(substr.data(), pos, substr.length())) != std::string::npos) {
            str.replace(pos, substr.length(), replacement.data(), replacement.length());
            pos += replacement.length();
        }
    }

    LIBASSERT_ATTR_COLD
    static void replace_all_template(std::string& str, const std::pair<std::regex, std::string_view>& rule) {
        const auto& [re, replacement] = rule;
        std::smatch match;
        std::size_t cursor = 0;
        // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
        while(std::regex_search(str.cbegin() + cursor, str.cend(), match, re)) {
            // find matching >
            const std::size_t match_begin = cursor + match.position();
            std::size_t end = match_begin + match.length();
            for(int c = 1; end < str.size() && c > 0; end++) {
                if(str[end] == '<') {
                    c++;
                } else if(str[end] == '>') {
                    c--;
                }
            }
            // make the replacement
            str.replace(match_begin, end - match_begin, replacement);
            cursor = match_begin + replacement.length();
        }
    };

    LIBASSERT_ATTR_COLD
    static std::string indent(const std::string_view str, size_t depth, char c = ' ', bool ignore_first = false) {
        size_t i = 0;
        size_t j;
        std::string output;
        while((j = str.find('\n', i)) != std::string::npos) {
            if(i != 0 || !ignore_first) { output.insert(output.end(), depth, c); }
            output.insert(output.end(), str.begin() + i, str.begin() + j + 1);
            i = j + 1;
        }
        if(i != 0 || !ignore_first) { output.insert(output.end(), depth, c); }
        output.insert(output.end(), str.begin() + i, str.end());
        return output;
    }

    template<size_t N>
    LIBASSERT_ATTR_COLD
    std::optional<std::array<std::string, N>> match(const std::string& s, const std::regex& r) {
        std::smatch match;
        if(std::regex_match(s, match, r)) {
            std::array<std::string, N> arr;
            for(size_t i = 0; i < N; i++) {
                arr[i] = match[i + 1].str();
            }
            return arr;
        } else {
            return {};
        }
    }

    /*
     * system wrappers
     */

    LIBASSERT_ATTR_COLD void enable_virtual_terminal_processing_if_needed() {
        // enable colors / ansi processing if necessary
        #ifdef _WIN32
         // https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences#example-of-enabling-virtual-terminal-processing
         #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
          constexpr DWORD ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x4;
         #endif
         HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
         DWORD dwMode = 0;
         if(hOut == INVALID_HANDLE_VALUE) return;
         if(!GetConsoleMode(hOut, &dwMode)) return;
         if(dwMode != (dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
         if(!SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) return;
        #endif
    }

    LIBASSERT_ATTR_COLD static bool isatty(int fd) {
        #ifdef _WIN32
         return _isatty(fd);
        #else
         return ::isatty(fd);
        #endif
    }

    LIBASSERT_ATTR_COLD std::string get_executable_path() {
        #ifdef _WIN32
         char buffer[MAX_PATH + 1];
         int s = GetModuleFileNameA(NULL, buffer, sizeof(buffer));
         LIBASSERT_PRIMITIVE_ASSERT(s != 0);
         return buffer;
        #else
         // NOLINTNEXTLINE(misc-include-cleaner)
         char buffer[PATH_MAX + 1];
         // NOLINTNEXTLINE(misc-include-cleaner)
         const ssize_t s = readlink("/proc/self/exe", buffer, PATH_MAX);
         LIBASSERT_PRIMITIVE_ASSERT(s != -1);
         buffer[s] = 0;
         return buffer;
        #endif
    }

    // NOTE: Not thread-safe
    LIBASSERT_ATTR_COLD std::string strerror_wrapper(int e) {
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        return strerror(e);
    }

    using trace_t = std::vector<cpptrace::stacktrace_frame>;

    LIBASSERT_ATTR_COLD
    void* get_stacktrace_opaque() {
        return new trace_t(cpptrace::generate_trace());
    }

    /*
     * C++ syntax analysis logic
     */

    struct highlight_block {
        std::string_view color;
        std::string content;
    };

    LIBASSERT_ATTR_COLD
    std::string union_regexes(std::initializer_list<std::string_view> regexes) {
        std::string composite;
        for(const std::string_view str : regexes) {
            if(!composite.empty()) {
                composite += "|";
            }
            composite += "(?:" + std::string(str) + ")";
        }
        return composite;
    }

    LIBASSERT_ATTR_COLD
    std::string prettify_type(std::string type) {
        // > > -> >> replacement
        // could put in analysis:: but the replacement is basic and this is more convenient for
        // using in the stringifier too
        replace_all_dynamic(type, "> >", ">>");
        // "," -> ", " and " ," -> ", "
        static const std::regex comma_re(R"(\s*,\s*)");
        replace_all(type, comma_re, ", ");
        // class C -> C for msvc
        static const std::regex class_re(R"(\b(class|struct)\s+)");
        replace_all(type, class_re, "");
        // rules to replace std::basic_string -> std::string and std::basic_string_view -> std::string_view
        static const std::pair<std::regex, std::string_view> basic_string = {
            std::regex(R"(std(::[a-zA-Z0-9_]+)?::basic_string<char)"), "std::string"
        };
        static const std::pair<std::regex, std::string_view> basic_string_view = {
            std::regex(R"(std(::[a-zA-Z0-9_]+)?::basic_string_view<char)"), "std::string_view"
        };
        // rule to replace ", std::allocator<whatever>"
        static const std::pair<std::regex, std::string_view> allocator = {
            std::regex(R"(,\s*std(::[a-zA-Z0-9_]+)?::allocator<)"), ""
        };
        replace_all_template(type, basic_string);
        replace_all_template(type, basic_string_view);
        replace_all_template(type, allocator);
        return type;
    }

    // forward declaration so this can be used in some printbugging
    LIBASSERT_ATTR_COLD
    std::string highlight(const std::string& expression);

    class analysis {
        enum class token_e {
            keyword,
            punctuation,
            number,
            string,
            named_literal,
            identifier,
            whitespace
        };

        struct token_t {
            token_e token_type;
            std::string str;
        };

    public:
        // Analysis singleton, lazy-initialize all the regex nonsense
        // 8 BSS bytes and <512 bytes heap bytes not a problem
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
        static analysis* analysis_singleton;
        static analysis& get() {
            if(analysis_singleton == nullptr) {
                analysis_singleton = new analysis();
            }
            return *analysis_singleton;
        }

        // could all be const but I don't want to try to pack everything into an init list
        std::vector<std::pair<token_e, std::regex>> rules; // could be std::array but I don't want to hard-code a size
        std::regex escapes_re;
        std::unordered_map<std::string_view, int> precedence;
        std::unordered_map<std::string_view, std::string_view> braces = {
            // template angle brackets excluded from this analysis
            { "(", ")" }, { "{", "}" }, { "[", "]" }, { "<:", ":>" }, { "<%", "%>" }
        };
        std::unordered_map<std::string_view, std::string_view> digraph_map = {
            { "<:", "[" }, { "<%", "{" }, { ":>", "]" }, { "%>", "}" }
        };
        // punctuators to highlight
        std::unordered_set<std::string_view> highlight_ops = {
            "~", "!", "+", "-", "*", "/", "%", "^", "&", "|", "=", "+=", "-=", "*=", "/=", "%=",
            "^=", "&=", "|=", "==", "!=", "<", ">", "<=", ">=", "<=>", "&&", "||", "<<", ">>",
            "<<=", ">>=", "++", "--", "and", "or", "xor", "not", "bitand", "bitor", "compl",
            "and_eq", "or_eq", "xor_eq", "not_eq"
        };
        // all operators
        std::unordered_set<std::string_view> operators = {
            ":", "...", "?", "::", ".", ".*", "->", "->*", "~", "!", "+", "-", "*", "/", "%", "^",
            "&", "|", "=", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "==", "!=", "<", ">",
            "<=", ">=", "<=>", "&&", "||", "<<", ">>", "<<=", ">>=", "++", "--", ",", "and", "or",
            "xor", "not", "bitand", "bitor", "compl", "and_eq", "or_eq", "xor_eq", "not_eq"
        };
        std::unordered_map<std::string_view, std::string_view> alternative_operators_map = {
            {"and", "&&"}, {"or", "||"}, {"xor", "^"}, {"not", "!"}, {"bitand", "&"},
            {"bitor", "|"}, {"compl", "~"}, {"and_eq", "&="}, {"or_eq", "|="}, {"xor_eq", "^="},
            {"not_eq", "!="}
        };
        std::unordered_set<std::string_view> bitwise_operators = {
            "^", "&", "|", "^=", "&=", "|=", "xor", "bitand", "bitor", "and_eq", "or_eq", "xor_eq"
        };
        std::vector<std::pair<std::regex, literal_format>> literal_formats;

    private:
        LIBASSERT_ATTR_COLD
        analysis() {
            // https://eel.is/c++draft/gram.lex
            // generate regular expressions
            std::string keywords[] = { "alignas", "constinit", "public", "alignof", "const_cast",
                "float", "register", "try", "asm", "continue", "for", "reinterpret_cast", "typedef",
                "auto", "co_await", "friend", "requires", "typeid", "bool", "co_return", "goto",
                "return", "typename", "break", "co_yield", "if", "short", "union", "case",
                "decltype", "inline", "signed", "unsigned", "catch", "default", "int", "sizeof",
                "using", "char", "delete", "long", "static", "virtual", "char8_t", "do", "mutable",
                "static_assert", "void", "char16_t", "double", "namespace", "static_cast",
                "volatile", "char32_t", "dynamic_cast", "new", "struct", "wchar_t", "class", "else",
                "noexcept", "switch", "while", "concept", "enum", "template", "const", "explicit",
                "operator", "this", "consteval", "export", "private", "thread_local", "constexpr",
                "extern", "protected", "throw" };
            std::string punctuators[] = { "{", "}", "[", "]", "(", ")", "<:", ":>", "<%",
                "%>", ";", ":", "...", "?", "::", ".", ".*", "->", "->*", "~", "!", "+", "-", "*",
                "/", "%", "^", "&", "|", "=", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "==",
                "!=", "<", ">", "<=", ">=", "<=>", "&&", "||", "<<", ">>", "<<=", ">>=", "++", "--",
                ",", "and", "or", "xor", "not", "bitand", "bitor", "compl", "and_eq", "or_eq",
                "xor_eq", "not_eq", "#" }; // # appears in some lambda signatures
            // Sort longest -> shortest (secondarily A->Z)
            const auto cmp = [](const std::string_view a, const std::string_view b) {
                if(a.length() > b.length()) { return true; }
                else if(a.length() == b.length()) { return a < b; }
                else { return false; }
            };
            std::sort(std::begin(keywords), std::end(keywords), cmp);
            std::sort(std::begin(punctuators), std::end(punctuators), cmp);
            // Escape special characters and add wordbreaks
            const std::regex special_chars { R"([-[\]{}()*+?.,\^$|#\s])" };
            std::transform(std::begin(punctuators), std::end(punctuators), std::begin(punctuators),
                [&special_chars](const std::string& str) {
                    return std::regex_replace(str + (isalpha(str[0]) ? "\\b" : ""), special_chars, "\\$&");
                });
            // https://eel.is/c++draft/lex.pptoken#3.2
            *std::find(std::begin(punctuators), std::end(punctuators), "<:") += "(?!:[^:>])";
            // regular expressions
            std::string keywords_re    = "(?:" + join(keywords, "|") + ")\\b";
            std::string punctuators_re = join(punctuators, "|");
            // numeric literals
            const std::string optional_integer_suffix = "(?:[Uu](?:LL?|ll?|Z|z)?|(?:LL?|ll?|Z|z)[Uu]?)?";
            const std::string int_binary  = "0[Bb][01](?:'?[01])*" + optional_integer_suffix;
            // slightly modified from grammar so 0 is lexed as a decimal literal instead of octal
            const std::string int_octal   = "0(?:'?[0-7])+" + optional_integer_suffix;
            const std::string int_decimal = "(?:0|[1-9](?:'?\\d)*)" + optional_integer_suffix;
            const std::string int_hex        = "0[Xx](?!')(?:'?[\\da-fA-F])+" + optional_integer_suffix;
            const std::string digit_sequence = "\\d(?:'?\\d)*";
            const std::string fractional_constant = stringf(
                "(?:(?:%s)?\\.%s|%s\\.)",
                digit_sequence.c_str(),
                digit_sequence.c_str(),
                digit_sequence.c_str()
            );
            const std::string exponent_part = "(?:[Ee][\\+-]?" + digit_sequence + ")";
            const std::string suffix = "[FfLl]";
            const std::string float_decimal = stringf(
                "(?:%s%s?|%s%s)%s?",
                fractional_constant.c_str(),
                exponent_part.c_str(),
                digit_sequence.c_str(),
                exponent_part.c_str(),
                suffix.c_str()
            );
            const std::string hex_digit_sequence = "[\\da-fA-F](?:'?[\\da-fA-F])*";
            const std::string hex_frac_const = stringf(
                "(?:(?:%s)?\\.%s|%s\\.)",
                hex_digit_sequence.c_str(),
                hex_digit_sequence.c_str(),
                hex_digit_sequence.c_str()
            );
            const std::string binary_exp = "[Pp][\\+-]?" + digit_sequence;
            const std::string float_hex = stringf(
                "0[Xx](?:%s|%s)%s%s?",
                hex_frac_const.c_str(),
                hex_digit_sequence.c_str(),
                binary_exp.c_str(),
                suffix.c_str()
            );
            // char and string literals
            const std::string escapes = R"(\\[0-7]{1,3}|\\x[\da-fA-F]+|\\.)";
            const std::string char_literal = R"((?:u8|[UuL])?'(?:)" + escapes + R"(|[^\n'])*')";
            const std::string string_literal = R"((?:u8|[UuL])?"(?:)" + escapes + R"(|[^\n"])*")";
                                 // \2 because the first capture is the match without the rest of the file
            const std::string raw_string_literal = R"((?:u8|[UuL])?R"([^ ()\\t\r\v\n]*)\((?:(?!\)\2\").)*\)\2")";
            escapes_re = std::regex(escapes);
            // final rule set
            // rules must be sequenced as a topological sort adhering to:
            // keyword > identifier
            // number > punctuation (for .1f)
            // float > int (for 1.5 and hex floats)
            // hex int > decimal int
            // named_literal > identifier
            // string > identifier (for R"(foobar)")
            // punctuation > identifier (for "not" and other alternative operators)
            const std::pair<token_e, std::string> rules_raw[] = {
                { token_e::keyword    , keywords_re },
                { token_e::number     , union_regexes({
                    float_decimal,
                    float_hex,
                    int_binary,
                    int_octal,
                    int_hex,
                    int_decimal
                }) },
                { token_e::punctuation, punctuators_re },
                { token_e::named_literal, "true|false|nullptr" },
                { token_e::string     , union_regexes({
                    char_literal,
                    raw_string_literal,
                    string_literal
                }) },
                { token_e::identifier , R"((?!\d+)(?:[\da-zA-Z_\$]|\\u[\da-fA-F]{4}|\\U[\da-fA-F]{8})+)" },
                { token_e::whitespace , R"(\s+)" }
            };
            rules.resize(std::size(rules_raw));
            for(size_t i = 0; i < std::size(rules_raw); i++) {
                                // [^] instead of . because . does not match newlines
                const std::string str = stringf("^(%s)[^]*", rules_raw[i].second.c_str());
                #ifdef _0_DEBUG_ASSERT_LEXER_RULES
                 fprintf(stderr, "%s : %s\n", rules_raw[i].first.c_str(), str.c_str());
                #endif
                rules[i] = { rules_raw[i].first, std::regex(str) };
            }
            // setup literal format rules
            literal_formats = {
                { std::regex(int_binary),    literal_format::binary },
                { std::regex(int_octal),     literal_format::octal },
                { std::regex(int_decimal),   literal_format::dec },
                { std::regex(int_hex),       literal_format::hex },
                { std::regex(float_decimal), literal_format::dec },
                { std::regex(float_hex),     literal_format::hex },
                { std::regex(char_literal),  literal_format::character }
            };
            // generate precedence table
            // bottom few rows of the precedence table:
            const std::unordered_map<int, std::vector<std::string_view>> precedences = {
                { -1, { "<<", ">>" } },
                { -2, { "<=>" } },
                { -3, { "<", "<=", ">=", ">" } },
                { -4, { "==", "!=" } },
                { -5, { "&" } },
                { -6, { "^" } },
                { -7, { "|" } },
                { -8, { "&&" } },
                { -9, { "||" } },
                            // Note: associativity logic currently relies on these having precedence -10
                { -10, { "?", ":", "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=" } },
                { -11, { "," } }
            };
            std::unordered_map<std::string_view, int> table;
            for(const auto& [ p, ops ] : precedences) {
                for(const auto& op : ops) {
                    table.insert({op, p});
                }
            }
            precedence = table;
        }

    public:
        LIBASSERT_ATTR_COLD
        std::string_view normalize_op(const std::string_view op) {
            // Operators need to be normalized to support alternative operators like and and bitand
            // Normalization instead of just adding to the precedence table because target operators
            // will always be the normalized operator even when the alternative operator is used.
            if(auto it = alternative_operators_map.find(op); it != alternative_operators_map.end()) {
                return it->second;
            } else {
                return op;
            }
        }

        LIBASSERT_ATTR_COLD
        std::string_view normalize_brace(const std::string_view brace) {
            // Operators need to be normalized to support alternative operators like and and bitand
            // Normalization instead of just adding to the precedence table because target operators
            // will always be the normalized operator even when the alternative operator is used.
            if(auto it = digraph_map.find(brace); it != digraph_map.end()) {
                return it->second;
            } else {
                return brace;
            }
        }

        LIBASSERT_ATTR_COLD
        std::vector<token_t> tokenize(const std::string& expression, bool decompose_shr = false) {
            std::vector<token_t> tokens;
            size_t i = 0;
            while(i < expression.length()) {
                std::smatch match;
                bool at_least_one_matched = false;
                for(const auto& [ type, re ] : rules) {
                    // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
                    if(std::regex_match(std::begin(expression) + i, std::end(expression), match, re)) {
                        #ifdef _0_DEBUG_ASSERT_TOKENIZATION
                         fprintf(stderr, "%s\n", match[1].str().c_str());
                         fflush(stdout);
                        #endif
                        if(decompose_shr && match[1].str() == ">>") { // Do >> decomposition now for templates
                            tokens.push_back({ type, ">" });
                            tokens.push_back({ type, ">" });
                        } else {
                            tokens.push_back({ type, match[1].str() });
                        }
                        i += match[1].length();
                        at_least_one_matched = true;
                        break;
                    }
                }
                if(!at_least_one_matched) {
                    throw "error: invalid token";
                }
            }
            return tokens;
        }

        LIBASSERT_ATTR_COLD
        std::vector<highlight_block> highlight_string(const std::string& str) const {
            std::vector<highlight_block> output;
            std::smatch match;
            std::size_t i = 0;
            // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
            while(std::regex_search(str.cbegin() + i, str.cend(), match, escapes_re)) {
                // add string part
                LIBASSERT_PRIMITIVE_ASSERT(match.position() > 0);
                if(match.position() > 0) {
                    output.push_back({GREEN, str.substr(i, match.position())});
                }
                output.push_back({BLUE, str.substr(i + match.position(), match.length())});
                i += match.position() + match.length();
            }
            if(i < str.length()) {
                output.push_back({GREEN, str.substr(i)});
            }
            return output;
        }

        LIBASSERT_ATTR_COLD
        // TODO: Refactor
        // NOLINTNEXTLINE(readability-function-cognitive-complexity)
        std::vector<highlight_block> highlight(const std::string& expression) try {
            const auto tokens = tokenize(expression);
            std::vector<highlight_block> output;
            for(size_t i = 0; i < tokens.size(); i++) {
                const auto& token = tokens[i];
                // Peek next non-whitespace token, return empty whitespace token if end is reached
                const auto peek = [i, &tokens](size_t j = 1) {
                    for(size_t k = 1; j > 0 && i + k < tokens.size(); k++) {
                        if(tokens[i + k].token_type != token_e::whitespace) {
                            if(--j == 0) {
                                return tokens[i + k];
                            }
                        }
                    }
                    LIBASSERT_PRIMITIVE_ASSERT(j != 0);
                    return token_t { token_e::whitespace, "" };
                };
                switch(token.token_type) {
                    case token_e::keyword:
                        output.push_back({PURPL, token.str});
                        break;
                    case token_e::punctuation:
                        if(highlight_ops.count(token.str)) {
                            output.push_back({PURPL, token.str});
                        } else {
                            output.push_back({"", token.str});
                        }
                        break;
                    case token_e::named_literal:
                        output.push_back({ORANGE, token.str});
                        break;
                    case token_e::number:
                        output.push_back({CYAN, token.str});
                        break;
                    case token_e::string:
                        {
                            auto string_tokens = highlight_string(token.str);
                            output.insert(output.end(), string_tokens.begin(), string_tokens.end());
                        }
                        break;
                    case token_e::identifier:
                        // NOLINTNEXTLINE(bugprone-branch-clone)
                        if(peek().str == "(") {
                            output.push_back({BLUE, token.str});
                        } else if(peek().str == "::") {
                            output.push_back({YELLOW, token.str});
                        } else {
                            output.push_back({BLUE, token.str});
                        }
                        break;
                    case token_e::whitespace:
                        output.push_back({"", token.str});
                        break;
                }
            }
            return output;
        } catch(...) {
            return {{"", expression}};
        }

        LIBASSERT_ATTR_COLD
        literal_format get_literal_format(const std::string& expression) {
            for(auto& [ re, type ] : literal_formats) {
                if(std::regex_match(expression, re)) {
                    return type;
                }
            }
            return literal_format::none; // not a literal
        }

        LIBASSERT_ATTR_COLD
        static token_t find_last_non_ws(const std::vector<token_t>& tokens, size_t i) {
            // returns empty token_e::whitespace on failure
            while(i--) {
                if(tokens[i].token_type != token_e::whitespace) {
                    return tokens[i];
                }
            }
            return {token_e::whitespace, ""};
        }

        LIBASSERT_ATTR_COLD
        static std::string_view get_real_op(const std::vector<token_t>& tokens, const size_t i) {
            // re-coalesce >> if necessary
            const bool is_shr = tokens[i].str == ">" && i < tokens.size() - 1 && tokens[i + 1].str == ">";
            return is_shr ? std::string_view(">>") : std::string_view(tokens[i].str);
        }

        // In this function we are essentially exploring all possible parse trees for an expression
        // an making an attempt to disambiguate as much as we can. It's potentially O(2^t) (?) with
        // t being the number of possible templates in the expression, but t is anticipated to
        // always be small.
        // Returns true if parse tree traversal was a success, false if depth was exceeded
        static constexpr int max_depth = 10;
        // TODO
        // NOLINTNEXTLINE(misc-no-recursion, readability-function-cognitive-complexity)
        LIBASSERT_ATTR_COLD bool pseudoparse(
            const std::vector<token_t>& tokens,
            const std::string_view target_op,
            size_t i,
            int current_lowest_precedence,
            int template_depth,
            int middle_index, // where the split currently is, current op = tokens[middle_index]
            int depth,
            std::set<int>& output
        ) {
            #ifdef _0_DEBUG_ASSERT_DISAMBIGUATION
            (void)fprintf(stderr, "*");
            #endif
            if(depth > max_depth) {
                (void)fprintf(stderr, "Max depth exceeded\n");
                return false;
            }
            // precedence table is binary, unary operators have highest precedence
            // we can figure out unary / binary easy enough
            enum {
                expecting_operator,
                expecting_term
            } state = expecting_term;
            for(; i < tokens.size(); i++) {
                const token_t& token = tokens[i];
                // scan forward to matching brace
                // can assume braces are balanced
                const auto scan_forward = [this, &i, &tokens](
                    const std::string_view open,
                    const std::string_view close)
                {
                    bool empty = true;
                    int count = 0;
                    while(++i < tokens.size()) {
                        if(normalize_brace(tokens[i].str) == normalize_brace(open)) {
                            count++;
                        } else if(normalize_brace(tokens[i].str) == normalize_brace(close)) {
                            if(count-- == 0) {
                                break;
                            }
                        } else if(tokens[i].token_type != token_e::whitespace) {
                            empty = false;
                        }
                    }
                    if(i == tokens.size() && count != -1) {
                        LIBASSERT_PRIMITIVE_ASSERT(false, "ill-formed expression input");
                    }
                    return empty;
                };
                switch(token.token_type) {
                    case token_e::punctuation:
                        if(operators.count(token.str)) {
                            if(state == expecting_term) {
                                // must be unary, continue
                            } else {
                                // template can only open with a < token, no need to check << or <<=
                                // also must be preceeded by an identifier
                                if(token.str == "<" && find_last_non_ws(tokens, i).token_type == token_e::identifier) {
                                    // branch 1: this is a template opening
                                    const bool success = pseudoparse(
                                        tokens,
                                        target_op,
                                        i + 1,
                                        current_lowest_precedence,
                                        template_depth + 1,
                                        middle_index, depth + 1,
                                        output
                                    );
                                    if(!success) { // early exit if we have to discard
                                        return false;
                                    }
                                    // branch 2: this is a binary operator // fallthrough
                                } else if(token.str == "<" && normalize_brace(find_last_non_ws(tokens, i).str) == "]") {
                                    // this must be a template parameter list, part of a generic lambda
                                    const bool empty = scan_forward("<", ">");
                                    LIBASSERT_PRIMITIVE_ASSERT(!empty);
                                    state = expecting_operator;
                                    continue;
                                }
                                if(template_depth > 0 && token.str == ">") {
                                    // No branch here: This must be a close. C++ standard
                                    // Disambiguates by treating > always as a template parameter
                                    // list close and >> is broken down.
                                    // >= and >>= don't need to be broken down and we don't need to
                                    // worry about re-tokenizing beyond just the simple breakdown.
                                    // I.e. we don't need to worry about x<2>>>1 which is tokenized
                                    // as x < 2 >> > 1 but perhaps being intended as x < 2 > >> 1.
                                    // Standard has saved us from this complexity.
                                    // Note: >> breakdown moved to initial tokenization so we can
                                    // take the token vector by reference.
                                    template_depth--;
                                    state = expecting_operator;
                                    continue;
                                }
                                // binary
                                if(template_depth == 0) { // ignore precedence in template parameter list
                                    // re-coalesce >> if necessary
                                    const std::string_view op = normalize_op(get_real_op(tokens, i));
                                    if(precedence.count(op)) // NOLINT(readability-braces-around-statements)
                                    if(precedence.at(op) < current_lowest_precedence
                                    || (precedence.at(op) == current_lowest_precedence && precedence.at(op) != -10)) {
                                        middle_index = (int)i;
                                        current_lowest_precedence = precedence.at(op);
                                    }
                                    if(op == ">>") {  // NOLINT(readability-misleading-indentation)
                                        i++;
                                    }
                                }
                                state = expecting_term;
                            }
                        } else if(braces.count(token.str)) {
                            // We can assume the given expression is valid.
                            // Braces must be balanced.
                            // Scan forward until finding matching brace, don't need to take into
                            // account other types of braces.
                            const std::string_view open = token.str;
                            const std::string_view close = braces.at(token.str);
                            const bool empty = scan_forward(open, close);
                            // Handle () and {} when they aren't a call/initializer
                            // [] may appear in lambdas when state == expecting_term
                            // [](){ ... }() is parsed fine because state == expecting_operator
                            // after the captures list. Not concerned with template parameters at
                            // the moment.
                            if(state == expecting_term && empty && normalize_brace(open) != "[") {
                                return true; // this is a failed parse tree
                            }
                            state = expecting_operator;
                        } else {
                            LIBASSERT_PRIMITIVE_ASSERT(false, "unhandled punctuation?");
                        }
                        break;
                    case token_e::keyword:
                    case token_e::named_literal:
                    case token_e::number:
                    case token_e::string:
                    case token_e::identifier:
                        state = expecting_operator;
                    case token_e::whitespace:
                        break;
                }
            }
            if(
                middle_index != -1
                && normalize_op(get_real_op(tokens, middle_index)) == target_op
                && template_depth == 0
                && state == expecting_operator
            ) {
                output.insert(middle_index);
            } else {
                // failed parse tree, ignore
            }
            return true;
        }

        LIBASSERT_ATTR_COLD
        std::pair<std::string, std::string> decompose_expression(
            const std::string& expression,
            const std::string_view target_op
        ) {
            // While automatic decomposition allows something like `assert(foo(n) == bar<n> + n);`
            // treated as `assert_eq(foo(n), bar<n> + n);` we only get the full expression's string
            // representation.
            // Here we attempt to parse basic info for the raw string representation. Just enough to
            // decomposition into left and right parts for diagnostic.
            // Template parameters make C++ grammar ambiguous without type information. That being
            // said, many expressions can be disambiguated.
            // This code will make guesses about the grammar, essentially doing a traversal of all
            // possibly parse trees and looking for ones that could work. This is O(2^t) with the
            // where t is the number of potential templates. Usually t is small but this should
            // perhaps be limited.
            // Will return {"left", "right"} if unable to decompose unambiguously.
            // Some cases to consider
            //   tgt  expr
            //   ==   a < 1 == 2 > ( 1 + 3 )
            //   ==   a < 1 == 2 > - 3 == ( 1 + 3 )
            //   ==   ( 1 + 3 ) == a < 1 == 2 > - 3 // <- ambiguous
            //   ==   ( 1 + 3 ) == a < 1 == 2 > ()
            //   <    a<x<x<x<x<x<x<x<x<x<1>>>>>>>>>
            //   ==   1 == something<a == b>>2 // <- ambiguous
            //   <    1 == something<a == b>>2 // <- should be an error
            //   <    1 < something<a < b>>2 // <- ambiguous
            // If there is only one candidate from the parse trees considered, expression has been
            // disambiguated. If there are no candidates that's an error. If there is more than one
            // candidate the expression may be ambiguous, but, not necessarily. For example,
            // a < 1 == 2 > - 3 == ( 1 + 3 ) is split the same regardless of whether a < 1 == 2 > is
            // treated as a template or not:
            //   left:  a < 1 == 2 > - 3
            //   right: ( 1 + 3 )
            //   ---
            //   left:  a < 1 == 2 > - 3
            //   right: ( 1 + 3 )
            //   ---
            // Because we're just splitting an expression in half, instead of storing tokens for
            // both sides we can just store an index of the split.
            // Note: We don't need to worry about expressions where there is only a single term.
            // This will only be called on decomposable expressions.
            // Note: The >> to > > token breakdown needed in template parameter lists is done in the
            // initial tokenization so we can pass the token vector by reference and avoid copying
            // for every recursive path (O(t^2)). This does not create an issue for syntax
            // highlighting as long as >> and > are highlighted the same.
            const auto tokens = tokenize(expression, true);
            // We're only looking for the split, we can just store a set of split indices. No need
            // to store a vector<pair<vector<token_t>, vector<token_t>>>
            std::set<int> candidates;
            const bool success = pseudoparse(tokens, target_op, 0, 0, 0, -1, 0, candidates);
            #ifdef _0_DEBUG_ASSERT_DISAMBIGUATION
             fprintf(stderr, "\n%d %d\n", (int)candidates.size(), success);
             for(size_t m : candidates) {
                 std::vector<std::string> left_strings;
                 std::vector<std::string> right_strings;
                 for(size_t i = 0; i < m; i++) left_strings.push_back(tokens[i].str);
                 for(size_t i = m + 1; i < tokens.size(); i++) right_strings.push_back(tokens[i].str);
                 fprintf(stderr, "left:  %s\n", libassert::detail::highlight(std::string(trim(join(left_strings, "")))).c_str());
                 fprintf(stderr, "right: %s\n", libassert::detail::highlight(std::string(trim(join(right_strings, "")))).c_str());
                 fprintf(stderr, "target_op: %s\n", target_op.data()); // should be null terminated
                 fprintf(stderr, "---\n");
             }
            #endif
            if(success && candidates.size() == 1) {
                std::vector<std::string> left_strings;
                std::vector<std::string> right_strings;
                const size_t m = *candidates.begin();
                for(size_t i = 0; i < m; i++) {
                    left_strings.push_back(tokens[i].str);
                }
                // >> is decomposed and requires special handling (m will be the index of the first > token)
                for(size_t i = m + 1 + (target_op == ">>" ? 1 : 0); i < tokens.size(); i++) {
                    right_strings.push_back(tokens[i].str);
                }
                return {
                    std::string(trim(join(left_strings, ""))),
                    std::string(trim(join(right_strings, "")))
                };
            } else {
                return { "left", "right" };
            }
        }
    };

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    analysis* analysis::analysis_singleton;

    // public static wrappers
    LIBASSERT_ATTR_COLD
    std::string highlight(const std::string& expression) {
        #ifdef NCOLOR
        return expression;
        #else
        auto blocks = analysis::get().highlight(expression);
        std::string str;
        for(auto& block : blocks) {
            str += block.color;
            str += block.content;
            if(!block.color.empty()) {
                str += RESET;
            }
        }
        return str;
        #endif
    }

    LIBASSERT_ATTR_COLD
    std::vector<highlight_block> highlight_blocks(const std::string& expression) {
        #ifdef NCOLOR
        return expression;
        #else
        return analysis::get().highlight(expression);
        #endif
    }

    LIBASSERT_ATTR_COLD literal_format get_literal_format(const std::string& expression) {
        return analysis::get().get_literal_format(expression);
    }

    LIBASSERT_ATTR_COLD std::string trim_suffix(const std::string& expression) {
        return expression.substr(0, expression.find_last_not_of("FfUuLlZz") + 1);
    }

    LIBASSERT_ATTR_COLD bool is_bitwise(std::string_view op) {
        return analysis::get().bitwise_operators.count(op);
    }

    LIBASSERT_ATTR_COLD
    std::pair<std::string, std::string> decompose_expression(
        const std::string& expression,
        const std::string_view target_op
    ) {
        return analysis::get().decompose_expression(expression, target_op);
    }

    /*
     * stringification
     */

    LIBASSERT_ATTR_COLD
    static std::string escape_string(const std::string_view str, char quote) {
        std::string escaped;
        escaped += quote;
        for(const char c : str) {
            if(c == '\\') escaped += "\\\\"; // NOLINT(readability-braces-around-statements)
            else if(c == '\t') escaped += "\\t"; // NOLINT(readability-braces-around-statements)
            else if(c == '\r') escaped += "\\r"; // NOLINT(readability-braces-around-statements)
            else if(c == '\n') escaped += "\\n"; // NOLINT(readability-braces-around-statements)
            else if(c == quote) escaped += "\\" + std::to_string(quote); // NOLINT(readability-braces-around-statements)
            else if(c >= 32 && c <= 126) escaped += c; // NOLINT(readability-braces-around-statements) // printable
            else {
                constexpr const char * const hexdig = "0123456789abcdef";
                escaped += std::string("\\x") + hexdig[c >> 4] + hexdig[c & 0xF];
            }
        }
        escaped += quote;
        return escaped;
    }

    namespace stringification {
        LIBASSERT_ATTR_COLD std::string stringify(const std::string& value, literal_format) {
            return escape_string(value, '"');
        }

        LIBASSERT_ATTR_COLD std::string stringify(const std::string_view& value, literal_format) {
            return escape_string(value, '"');
        }

        LIBASSERT_ATTR_COLD std::string stringify(std::nullptr_t, literal_format) {
            return "nullptr";
        }

        // NOLINTNEXTLINE(misc-no-recursion)
        LIBASSERT_ATTR_COLD std::string stringify(char value, literal_format fmt) {
            if(fmt == literal_format::character) {
                return escape_string({&value, 1}, '\'');
            } else {
                return stringify((int)value, fmt);
            }
        }

        LIBASSERT_ATTR_COLD std::string stringify(bool value, literal_format) {
                return value ? "true" : "false";
        }

        template<typename T, typename std::enable_if<is_integral_and_not_bool<T>, int>::type = 0>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        // NOLINTNEXTLINE(misc-no-recursion)
        static std::string stringify_integral(T value, literal_format fmt) {
            std::ostringstream oss;
            switch(fmt) {
                case literal_format::character:
                    if(cmp_greater_equal(value, std::numeric_limits<char>::min())
                    && cmp_less_equal(value, std::numeric_limits<char>::max())) {
                        return stringify(static_cast<char>(value), literal_format::character);
                    } else {
                        return "";
                    }
                case literal_format::dec:
                    break;
                case literal_format::hex:
                    oss<<std::showbase<<std::hex;
                    break;
                case literal_format::octal:
                    oss<<std::showbase<<std::oct;
                    break;
                case literal_format::binary:
                    oss<<"0b"<<std::bitset<sizeof(value) * 8>(value);
                    goto r;
                default:
                    LIBASSERT_PRIMITIVE_ASSERT(false, "unexpected literal format requested for printing");
            }
            oss<<value;
            r: return std::move(oss).str();
        }

        LIBASSERT_ATTR_COLD std::string stringify(short value, literal_format fmt) {
            return stringify_integral(value, fmt);
        }

        // NOLINTNEXTLINE(misc-no-recursion)
        LIBASSERT_ATTR_COLD std::string stringify(int value, literal_format fmt) {
            return stringify_integral(value, fmt);
        }

        LIBASSERT_ATTR_COLD std::string stringify(long value, literal_format fmt) {
            return stringify_integral(value, fmt);
        }

        LIBASSERT_ATTR_COLD std::string stringify(long long value, literal_format fmt) {
            return stringify_integral(value, fmt);
        }

        LIBASSERT_ATTR_COLD std::string stringify(unsigned short value, literal_format fmt) {
            return stringify_integral(value, fmt);
        }

        LIBASSERT_ATTR_COLD std::string stringify(unsigned int value, literal_format fmt) {
            return stringify_integral(value, fmt);
        }

        LIBASSERT_ATTR_COLD std::string stringify(unsigned long value, literal_format fmt) {
            return stringify_integral(value, fmt);
        }

        LIBASSERT_ATTR_COLD std::string stringify(unsigned long long value, literal_format fmt) {
            return stringify_integral(value, fmt);
        }

        template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        static std::string stringify_floating_point(T value, literal_format fmt) {
            std::ostringstream oss;
            switch(fmt) {
                case literal_format::character:
                    return "";
                case literal_format::dec:
                    break;
                case literal_format::hex:
                    // apparently std::hexfloat automatically prepends "0x" while std::hex does not
                    oss<<std::hexfloat;
                    break;
                case literal_format::octal:
                case literal_format::binary:
                    return "";
                default:
                    LIBASSERT_PRIMITIVE_ASSERT(false, "unexpected literal format requested for printing");
            }
            oss<<std::setprecision(std::numeric_limits<T>::max_digits10)<<value;
            std::string s = std::move(oss).str();
            // std::showpoint adds a bunch of unecessary digits, so manually doing it correctly here
            if(s.find('.') == std::string::npos) {
                s += ".0";
            }
            return s;
        }

        LIBASSERT_ATTR_COLD std::string stringify(float value, literal_format fmt) {
            return stringify_floating_point(value, fmt);
        }

        LIBASSERT_ATTR_COLD std::string stringify(double value, literal_format fmt) {
            return stringify_floating_point(value, fmt);
        }

        LIBASSERT_ATTR_COLD std::string stringify(long double value, literal_format fmt) {
            return stringify_floating_point(value, fmt);
        }

        LIBASSERT_ATTR_COLD std::string stringify(std::error_code ec, literal_format) {
            return ec.category().name() + (':' + std::to_string(ec.value())) + ' ' + ec.message();
        }

        LIBASSERT_ATTR_COLD std::string stringify(std::error_condition ec, literal_format) {
            return ec.category().name() + (':' + std::to_string(ec.value())) + ' ' + ec.message();
        }

        #if __cplusplus >= 202002L
        LIBASSERT_ATTR_COLD std::string stringify(std::strong_ordering value, literal_format) {
                if(value == std::strong_ordering::less)       return "std::strong_ordering::less";
                if(value == std::strong_ordering::equivalent) return "std::strong_ordering::equivalent";
                if(value == std::strong_ordering::equal)      return "std::strong_ordering::equal";
                if(value == std::strong_ordering::greater)    return "std::strong_ordering::greater";
                return "Unknown std::strong_ordering value";
        }
        LIBASSERT_ATTR_COLD std::string stringify(std::weak_ordering value, literal_format) {
                if(value == std::weak_ordering::less)       return "std::weak_ordering::less";
                if(value == std::weak_ordering::equivalent) return "std::weak_ordering::equivalent";
                if(value == std::weak_ordering::greater)    return "std::weak_ordering::greater";
                return "Unknown std::weak_ordering value";
        }
        LIBASSERT_ATTR_COLD std::string stringify(std::partial_ordering value, literal_format) {
                if(value == std::partial_ordering::less)       return "std::partial_ordering::less";
                if(value == std::partial_ordering::equivalent) return "std::partial_ordering::equivalent";
                if(value == std::partial_ordering::greater)    return "std::partial_ordering::greater";
                if(value == std::partial_ordering::unordered)  return "std::partial_ordering::unordered";
                return "Unknown std::partial_ordering value";
        }
        #endif

        LIBASSERT_ATTR_COLD std::string stringify_ptr(const void* value, literal_format) {
            if(value == nullptr) {
                return "nullptr";
            }
            std::ostringstream oss;
            // Manually format the pointer - ostream::operator<<(void*) falls back to %p which
            // is implementation-defined. MSVC prints pointers without the leading "0x" which
            // messes up the highlighter.
            oss<<std::showbase<<std::hex<<uintptr_t(value);
            return std::move(oss).str();
        }
    }

    /*
     * stack trace printing
     */

    struct column_t {
        size_t width;
        std::vector<highlight_block> blocks;
        bool right_align = false;
        LIBASSERT_ATTR_COLD column_t(size_t _width, std::vector<highlight_block> _blocks, bool _right_align = false)
                                               : width(_width), blocks(std::move(_blocks)), right_align(_right_align) {}
        LIBASSERT_ATTR_COLD column_t(const column_t&) = default;
        LIBASSERT_ATTR_COLD column_t(column_t&&) = default;
        LIBASSERT_ATTR_COLD ~column_t() = default;
        LIBASSERT_ATTR_COLD column_t& operator=(const column_t&) = default;
        LIBASSERT_ATTR_COLD column_t& operator=(column_t&&) = default;
    };

    LIBASSERT_ATTR_COLD
    static constexpr unsigned log10(unsigned n) {
        if(n <= 1) {
            return 1;
        }
        unsigned t = 1;
        for(int i = 0; i < [] {
                // was going to reuse `i` but https://bugs.llvm.org/show_bug.cgi?id=51986
                int j = 0, v = std::numeric_limits<int>::max();
                while(v /= 10) {
                    j++;
                }
                return j;
            } () - 1; i++) {
            if(n <= t) {
                return i;
            }
            t *= 10;
        }
        return t;
    }

    static_assert(log10(1) == 1);
    static_assert(log10(9) == 1);
    static_assert(log10(10) == 1);
    static_assert(log10(11) == 2);

    using path_components = std::vector<std::string>;

    LIBASSERT_ATTR_COLD
    static path_components parse_path(const std::string_view path) {
        #ifdef _WIN32
         constexpr std::string_view path_delim = "/\\";
        #else
         constexpr std::string_view path_delim = "/";
        #endif
        // Some cases to consider
        // projects/libassert/demo.cpp               projects   libassert  demo.cpp
        // /glibc-2.27/csu/../csu/libc-start.c  /  glibc-2.27 csu      libc-start.c
        // ./demo.exe                           .  demo.exe
        // ./../demo.exe                        .. demo.exe
        // ../x.hpp                             .. x.hpp
        // /foo/./x                                foo        x
        // /foo//x                                 f          x
        path_components parts;
        for(const std::string& part : split(path, path_delim)) {
            if(parts.empty()) {
                // first gets added no matter what
                parts.push_back(part);
            } else {
                if(part.empty()) { // NOLINT(bugprone-branch-clone)
                    // nop
                } else if(part == ".") {
                    // nop
                } else if(part == "..") {
                    // cases where we have unresolvable ..'s, e.g. ./../../demo.exe
                    if(parts.back() == "." || parts.back() == "..") {
                        parts.push_back(part);
                    } else {
                        parts.pop_back();
                    }
                } else {
                    parts.push_back(part);
                }
            }
        }
        LIBASSERT_PRIMITIVE_ASSERT(!parts.empty());
        LIBASSERT_PRIMITIVE_ASSERT(parts.back() != "." && parts.back() != "..");
        return parts;
    }

    class path_trie {
        // Backwards path trie structure
        // e.g.:
        // a/b/c/d/e     disambiguate to -> c/d/e
        // a/b/f/d/e     disambiguate to -> f/d/e
        //  2   2   1   1   1
        // e - d - c - b - a
        //      \   1   1   1
        //       \ f - b - a
        // Nodes are marked with the number of downstream branches
        size_t downstream_branches = 1;
        std::string root;
        std::unordered_map<std::string, path_trie*> edges;
    public:
        LIBASSERT_ATTR_COLD
        path_trie(std::string _root) : root(std::move(_root)) {};
        LIBASSERT_ATTR_COLD
        compl path_trie() {
            for(auto& [k, trie] : edges) {
                delete trie;
            }
        }
        path_trie(const path_trie&) = delete;
        LIBASSERT_ATTR_COLD
        path_trie(path_trie&& other) noexcept { // needed for std::vector
            downstream_branches = other.downstream_branches; // NOLINT(cppcoreguidelines-prefer-member-initializer)
            root = other.root; // NOLINT(cppcoreguidelines-prefer-member-initializer)
            for(auto& [k, trie] : edges) {
                delete trie;
            }
            edges = std::move(other.edges); // NOLINT(cppcoreguidelines-prefer-member-initializer)
        }
        path_trie& operator=(const path_trie&) = delete;
        path_trie& operator=(path_trie&&) = delete;
        LIBASSERT_ATTR_COLD
        void insert(const path_components& path) {
            LIBASSERT_PRIMITIVE_ASSERT(path.back() == root);
            insert(path, (int)path.size() - 2);
        }
        LIBASSERT_ATTR_COLD
        path_components disambiguate(const path_components& path) {
            path_components result;
            path_trie* current = this;
            LIBASSERT_PRIMITIVE_ASSERT(path.back() == root);
            result.push_back(current->root);
            for(size_t i = path.size() - 2; i >= 1; i--) {
                LIBASSERT_PRIMITIVE_ASSERT(current->downstream_branches >= 1);
                if(current->downstream_branches == 1) {
                    break;
                }
                const std::string& component = path[i];
                LIBASSERT_PRIMITIVE_ASSERT(current->edges.count(component));
                current = current->edges.at(component);
                result.push_back(current->root);
            }
            std::reverse(result.begin(), result.end());
            return result;
        }
    private:
        LIBASSERT_ATTR_COLD
        // NOLINTNEXTLINE(misc-no-recursion)
        void insert(const path_components& path, int i) {
            if(i < 0) {
                return;
            }
            if(!edges.count(path[i])) {
                if(!edges.empty()) {
                    downstream_branches++; // this is to deal with making leaves have count 1
                }
                edges.insert({path[i], new path_trie(path[i])});
            }
            downstream_branches -= edges.at(path[i])->downstream_branches;
            edges.at(path[i])->insert(path, i - 1);
            downstream_branches += edges.at(path[i])->downstream_branches;
        }
    };

    LIBASSERT_ATTR_COLD
    // TODO
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    static std::string wrapped_print(const std::vector<column_t>& columns) {
        // 2d array rows/columns
        struct line_content {
            size_t length;
            std::string content;
        };
        std::vector<std::vector<line_content>> lines;
        lines.emplace_back(columns.size());
        // populate one column at a time
        for(size_t i = 0; i < columns.size(); i++) {
            auto [width, blocks, _] = columns[i];
            size_t current_line = 0;
            for(auto& block : blocks) {
                size_t block_i = 0;
                // digest block
                while(block_i != block.content.size()) {
                    if(lines.size() == current_line) {
                        lines.emplace_back(columns.size());
                    }
                    // number of characters we can extract from the block
                    size_t extract = std::min(width - lines[current_line][i].length, block.content.size() - block_i);
                    LIBASSERT_PRIMITIVE_ASSERT(block_i + extract <= block.content.size());
                    auto substr = std::string_view(block.content).substr(block_i, extract);
                    // handle newlines
                    if(auto x = substr.find('\n'); x != std::string_view::npos) {
                        substr = substr.substr(0, x);
                        extract = x + 1; // extract newline but don't print
                    }
                    // append
                    lines[current_line][i].content += block.color;
                    lines[current_line][i].content += substr;
                    lines[current_line][i].content += block.color.empty() ? "" : RESET;
                    // advance
                    block_i += extract;
                    lines[current_line][i].length += extract;
                    // new line if necessary
                    // substr.size() != extract iff newline
                    if(lines[current_line][i].length >= width || substr.size() != extract) {
                        current_line++;
                    }
                }
            }
        }
        // print
        std::string output;
        for(auto& line : lines) {
            // don't print empty columns with no content in subsequent columns and more importantly
            // don't print empty spaces they'll mess up lines after terminal resizing even more
            size_t last_col = 0;
            for(size_t i = 0; i < line.size(); i++) {
                if(!line[i].content.empty()) {
                    last_col = i;
                }
            }
            for(size_t i = 0; i <= last_col; i++) {
                auto& content = line[i];
                if(columns[i].right_align) {
                    output += stringf("%-*s%s%s",
                                      i == last_col ? 0 : int(columns[i].width - content.length), "",
                                      content.content.c_str(),
                                      i == last_col ? "\n" : " ");
                } else {
                    output += stringf("%s%-*s%s",
                                      content.content.c_str(),
                                      i == last_col ? 0 : int(columns[i].width - content.length), "",
                                      i == last_col ? "\n" : " ");
                }
            }
        }
        return output;
    }

    LIBASSERT_ATTR_COLD
    auto get_trace_window(const trace_t& trace) {
        // Two boundaries: assert_detail and main
        // Both are found here, nothing is filtered currently at stack trace generation
        // (inlining and platform idiosyncrasies interfere)
        size_t start = 0;
        size_t end = trace.size() - 1;
        for(size_t i = 0; i < trace.size(); i++) {
            if(trace[i].symbol.find("libassert::detail::") != std::string::npos) {
                start = i + 1;
            }
            if(trace[i].symbol == "main" || trace[i].symbol.find("main(") == 0) {
                end = i;
            }
        }
        #if !LIBASSERT_IS_MSVC
         const int start_offset = 0;
        #else
         const int start_offset = 1; // accommodate for lambda being used as statement expression
        #endif
        return std::pair(start + start_offset, end);
    }

    LIBASSERT_ATTR_COLD
    auto process_paths(const trace_t& trace, size_t start, size_t end) {
        // raw full path -> components
        std::unordered_map<std::string, path_components> parsed_paths;
        // base file name -> path trie
        std::unordered_map<std::string, path_trie> tries;
        for(size_t i = start; i <= end; i++) {
            const auto& source_path = trace[i].filename;
            if(!parsed_paths.count(source_path)) {
                auto parsed_path = parse_path(source_path);
                auto& file_name = parsed_path.back();
                parsed_paths.insert({source_path, parsed_path});
                if(tries.count(file_name) == 0) {
                    tries.insert({file_name, path_trie(file_name)});
                }
                tries.at(file_name).insert(parsed_path);
            }
        }
        // raw full path -> minified path
        std::unordered_map<std::string, std::string> files;
        size_t longest_file_width = 0;
        for(auto& [raw, parsed_path] : parsed_paths) {
            const std::string new_path = join(tries.at(parsed_path.back()).disambiguate(parsed_path), "/");
            internal_verify(files.insert({raw, new_path}).second);
            if(new_path.size() > longest_file_width) {
                longest_file_width = new_path.size();
            }
        }
        return std::pair(files, std::min(longest_file_width, size_t(50)));
    }

    LIBASSERT_ATTR_COLD [[nodiscard]]
    // TODO
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    std::string print_stacktrace(trace_t* raw_trace, int term_width) {
        std::string stacktrace;
        if(raw_trace) {
            auto& trace = *raw_trace;
            // [start, end] is an inclusive range
            auto [start, end] = get_trace_window(trace);
            // prettify signatures
            for(auto& frame : trace) {
                frame.symbol = prettify_type(frame.symbol);
            }
            // path preprocessing
            constexpr size_t max_file_length = 50;
            auto [files, longest_file_width] = process_paths(trace, start, end);
            // figure out column widths
            const auto max_line_number =
                std::max_element(
                    // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
                    trace.begin() + start,
                    // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
                    trace.begin() + end + 1,
                    [](const cpptrace::stacktrace_frame& a, const cpptrace::stacktrace_frame& b) {
                        return a.line < b.line;
                    }
                )->line;
            // +1 for indices starting at 0, +1 again for log
            const size_t max_line_number_width = log10(max_line_number + 1 + 1);
            const size_t max_frame_width = log10(end - start + 1 + 1); // ^
            // do the actual trace
            for(size_t i = start; i <= end; i++) {
                const auto& [address, line, col, source_path, signature] = trace[i];
                const std::string line_number = line == 0 ? "?" : std::to_string(line);
                // look for repeats, i.e. recursion we can fold
                size_t recursion_folded = 0;
                if(end - i >= 4) {
                    size_t j = 1;
                    for( ; i + j <= end; j++) {
                        if(trace[i + j] != trace[i] || trace[i + j].symbol == "??") {
                            break;
                        }
                    }
                    if(j >= 4) {
                        recursion_folded = j - 2;
                    }
                }
                const size_t frame_number = i - start + 1;
                // pretty print with columns for wide terminals
                // split printing for small terminals
                if(term_width >= 50) {
                    auto sig = highlight_blocks(signature + "("); // hack for the highlighter
                    sig.pop_back();
                    const size_t left = 2 + max_frame_width;
                    // todo: is this looking right...?
                    const size_t middle = std::max(line_number.size(), max_line_number_width);
                    const size_t remaining_width = term_width - (left + middle + 3 /* spaces */);
                    LIBASSERT_PRIMITIVE_ASSERT(remaining_width >= 2);
                    const size_t file_width = std::min({longest_file_width, remaining_width / 2, max_file_length});
                    const size_t sig_width = remaining_width - file_width;
                    stacktrace += wrapped_print({
                        { 1,          {{"", "#"}} },
                        { left - 2,   highlight_blocks(std::to_string(frame_number)), true },
                        { file_width, {{"", files.at(source_path)}} },
                        { middle,     highlight_blocks(line_number), true }, // intentionally not coloring "?"
                        { sig_width,  sig }
                    });
                } else {
                    auto sig = highlight(signature + "("); // hack for the highlighter
                    sig = sig.substr(0, sig.rfind('('));
                    stacktrace += stringf(
                        "#" CYAN "%2d" RESET " %s\n      at %s:%s\n",
                        (int)frame_number,
                        sig.c_str(),
                        files.at(source_path).c_str(),
                        (CYAN + line_number + RESET).c_str() // yes this is excessive; intentionally coloring "?"
                    );
                }
                if(recursion_folded) {
                    i += recursion_folded;
                    const std::string s = stringf("| %d layers of recursion were folded |", recursion_folded);
                    stacktrace += stringf(BLUE "|%*s|" RESET "\n", int(s.size() - 2), "");
                    stacktrace += stringf(BLUE  "%s"   RESET "\n", s.c_str());
                    stacktrace += stringf(BLUE "|%*s|" RESET "\n", int(s.size() - 2), "");
                }
            }
        } else {
            stacktrace += "Error while generating stack trace.\n";
        }
        return stacktrace;
    }

    LIBASSERT_ATTR_COLD binary_diagnostics_descriptor::binary_diagnostics_descriptor() = default;
    LIBASSERT_ATTR_COLD binary_diagnostics_descriptor::binary_diagnostics_descriptor(
            std::vector<std::string>& _lstrings,
            std::vector<std::string>& _rstrings,
            std::string _a_str,
            std::string _b_str,
            bool _multiple_formats
        ):
            lstrings(_lstrings),
            rstrings(_rstrings),
            a_str(std::move(_a_str)),
            b_str(std::move(_b_str)),
            multiple_formats(_multiple_formats),
            present(true) {}
    LIBASSERT_ATTR_COLD binary_diagnostics_descriptor::~binary_diagnostics_descriptor() = default;
    LIBASSERT_ATTR_COLD
    binary_diagnostics_descriptor::binary_diagnostics_descriptor(binary_diagnostics_descriptor&&) noexcept = default;
    LIBASSERT_ATTR_COLD binary_diagnostics_descriptor&
    binary_diagnostics_descriptor::operator=(binary_diagnostics_descriptor&&) noexcept(GCC_ISNT_STUPID) = default;

    LIBASSERT_ATTR_COLD
    static std::string print_values(const std::vector<std::string>& vec, size_t lw) {
        LIBASSERT_PRIMITIVE_ASSERT(!vec.empty());
        std::string values;
        if(vec.size() == 1) {
            values += stringf("%s\n", indent(highlight(vec[0]), 8 + lw + 4, ' ', true).c_str());
        } else {
            // spacing here done carefully to achieve <expr> =  <a>  <b>  <c>, or similar
            // no indentation done here for multiple value printing
            values += " ";
            for(const auto& str : vec) {
                values += stringf("%s", highlight(str).c_str());
                if(&str != &*--vec.end()) {
                    values += "  ";
                }
            }
            values += "\n";
        }
        return values;
    }

    LIBASSERT_ATTR_COLD
    static std::vector<highlight_block> get_values(const std::vector<std::string>& vec) {
        LIBASSERT_PRIMITIVE_ASSERT(!vec.empty());
        if(vec.size() == 1) {
            return highlight_blocks(vec[0]);
        } else {
            std::vector<highlight_block> blocks;
            // spacing here done carefully to achieve <expr> =  <a>  <b>  <c>, or similar
            // no indentation done here for multiple value printing
            blocks.push_back({"", " "});
            for(const auto& str : vec) {
                auto h = highlight_blocks(str);
                blocks.insert(blocks.end(), h.begin(), h.end());
                if(&str != &*--vec.end()) {
                    blocks.push_back({"", "  "});
                }
            }
            return blocks;
        }
    }

    constexpr int min_term_width = 50;
    constexpr size_t arrow_width = " => "sv.size();
    constexpr size_t where_indent = 8;

    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string print_binary_diagnostics(size_t term_width, binary_diagnostics_descriptor& diagnostics) {
        auto& [ lstrings, rstrings, a_sstr, b_sstr, multiple_formats, _ ] = diagnostics;
        const char* a_str = a_sstr.c_str(), *b_str = b_sstr.c_str();
        LIBASSERT_PRIMITIVE_ASSERT(!lstrings.empty());
        LIBASSERT_PRIMITIVE_ASSERT(!rstrings.empty());
        // pad all columns where there is overlap
        // TODO: Use column printer instead of manual padding.
        for(size_t i = 0; i < std::min(lstrings.size(), rstrings.size()); i++) {
            // find which clause, left or right, we're padding (entry i)
            std::vector<std::string>& which = lstrings[i].length() < rstrings[i].length() ? lstrings : rstrings;
            const int difference = std::abs((int)lstrings[i].length() - (int)rstrings[i].length());
            if(i != which.size() - 1) { // last column excluded as padding is not necessary at the end of the line
                which[i].insert(which[i].end(), difference, ' ');
            }
        }
        // determine whether we actually gain anything from printing a where clause (e.g. exclude "1 => 1")
        const struct { bool left, right; } has_useful_where_clause = {
            multiple_formats || lstrings.size() > 1 || (a_str != lstrings[0] && trim_suffix(a_str) != lstrings[0]),
            multiple_formats || rstrings.size() > 1 || (b_str != rstrings[0] && trim_suffix(b_str) != rstrings[0])
        };
        // print where clause
        std::string where;
        if(has_useful_where_clause.left || has_useful_where_clause.right) {
            size_t lw = std::max(
                has_useful_where_clause.left  ? strlen(a_str) : 0,
                has_useful_where_clause.right ? strlen(b_str) : 0
            );
            // Limit lw to about half the screen. TODO: Re-evaluate what we want to do here.
            if(term_width > 0) {
                lw = std::min(lw, term_width / 2 - where_indent - arrow_width);
            }
            where += "    Where:\n";
            auto print_clause = [term_width, lw, &where](const char* expr_str, std::vector<std::string>& expr_strs) {
                if(term_width >= min_term_width) {
                    where += wrapped_print({
                        { where_indent - 1, {{"", ""}} }, // 8 space indent, wrapper will add a space
                        { lw, highlight_blocks(expr_str) },
                        { 2, {{"", "=>"}} },
                        { term_width - lw - 8 /* indent */ - 4 /* arrow */, get_values(expr_strs) }
                    });
                } else {
                    where += stringf("        %s%*s => ",
                                     highlight(expr_str).c_str(), int(lw - strlen(expr_str)), "");
                    where += print_values(expr_strs, lw);
                }
            };
            if(has_useful_where_clause.left) {
                print_clause(a_str, lstrings);
            }
            if(has_useful_where_clause.right) {
                print_clause(b_str, rstrings);
            }
        }
        return where;
    }

    LIBASSERT_ATTR_COLD
    void sort_and_dedup(literal_format (&formats)[format_arr_length]) {
        std::sort(std::begin(formats), std::end(formats));
        size_t write_index = 1, read_index = 1;
        for(; read_index < std::size(formats); read_index++) {
            if(formats[read_index] != formats[write_index - 1]) {
                formats[write_index++] = formats[read_index];
            }
        }
        while(write_index < std::size(formats)) {
            formats[write_index++] = literal_format::none;
        }
    }

    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string print_extra_diagnostics(
        size_t term_width,
        const decltype(extra_diagnostics::entries)& extra_diagnostics
    ) {
        std::string output = "    Extra diagnostics:\n";
        size_t lw = 0;
        for(const auto& entry : extra_diagnostics) {
            lw = std::max(lw, entry.first.size());
        }
        for(const auto& entry : extra_diagnostics) {
            if(term_width >= min_term_width) {
                output += wrapped_print({
                    { 7, {{"", ""}} }, // 8 space indent, wrapper will add a space
                    { lw, highlight_blocks(entry.first) },
                    { 2, {{"", "=>"}} },
                    { term_width - lw - 8 /* indent */ - 4 /* arrow */, highlight_blocks(entry.second) }
                });
            } else {
                output += stringf("        %s%*s => %s\n",
                                  highlight(entry.first).c_str(), int(lw - entry.first.length()), "",
                                  indent(highlight(entry.second), 8 + lw + 4, ' ', true).c_str());
            }
        }
        return output;
    }

    LIBASSERT_ATTR_COLD extra_diagnostics::extra_diagnostics() = default;
    LIBASSERT_ATTR_COLD extra_diagnostics::~extra_diagnostics() = default;
    LIBASSERT_ATTR_COLD extra_diagnostics::extra_diagnostics(extra_diagnostics&&) noexcept = default;

    // mingw has threading/std::mutex problems
    // TODO: This was for ancient mingw. Re-evaluate if this should still be done.
    #if LIBASSERT_IS_GCC && IS_WINDOWS
     CRITICAL_SECTION CriticalSection;
     [[gnu::constructor]] LIBASSERT_ATTR_COLD static void initialize_critical_section() {
         InitializeCriticalSectionAndSpinCount(&CriticalSection, 10);
     }
     LIBASSERT_ATTR_COLD lock::lock() {
         EnterCriticalSection(&CriticalSection);
     }
     LIBASSERT_ATTR_COLD lock::~lock() {
         EnterCriticalSection(&CriticalSection);
     }
    #else
     // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
     std::mutex global_thread_lock;
     LIBASSERT_ATTR_COLD lock::lock() {
         global_thread_lock.lock();
     }
     LIBASSERT_ATTR_COLD lock::~lock() {
         global_thread_lock.unlock();
     }
    #endif

    LIBASSERT_ATTR_COLD
    const char* assert_type_name(assert_type t) {
        switch(t) {
            case assert_type::debug_assertion: return "Debug Assertion";
            case assert_type::assertion:       return "Assertion";
            case assert_type::assumption:      return "Assumption";
            case assert_type::verification:    return "Verification";
            default:
                LIBASSERT_PRIMITIVE_ASSERT(false);
                return "";
        }
    }

    LIBASSERT_ATTR_COLD
    size_t count_args_strings(const char* const* const arr) {
        size_t c = 0;
        for(size_t i = 0; *arr[i] != 0; i++) {
            c++;
        }
        return c + 1; // plus one, count the empty string
    }
}

namespace libassert {
    using namespace detail;

    const char* verification_failure::what() const noexcept {
        return "VERIFY() call failed";
    }

    LIBASSERT_ATTR_COLD assertion_printer::assertion_printer(
                                const assert_static_parameters* _params, const extra_diagnostics& _processed_args,
                                binary_diagnostics_descriptor& _binary_diagnostics, void* _raw_trace,
                                size_t _sizeof_args): params(_params), processed_args(_processed_args),
                                binary_diagnostics(_binary_diagnostics), raw_trace(_raw_trace),
                                sizeof_args(_sizeof_args) {}

    LIBASSERT_ATTR_COLD assertion_printer::~assertion_printer() {
        auto* trace = static_cast<trace_t*>(raw_trace);
        delete trace;
    }

    LIBASSERT_ATTR_COLD std::string assertion_printer::operator()(int width) const {
        const auto& [ name, type, expr_str, location, args_strings ] = *params;
        const auto& [ fatal, message, extra_diagnostics, pretty_function ] = processed_args;
        std::string output;
        // generate header
        const auto function = prettify_type(pretty_function);
        if(!message.empty()) {
            output += stringf("%s failed at %s:%d: %s: %s\n",
                              assert_type_name(type), location.file, location.line,
                              highlight(function).c_str(), message.c_str());
        } else {
            output += stringf("%s failed at %s:%d: %s:\n", assert_type_name(type),
                              location.file, location.line, highlight(function).c_str());
        }
        output += stringf("    %s\n", highlight(stringf("%s(%s%s);", name, expr_str,
                                                        sizeof_args > 0 ? ", ..." : "")).c_str());
        // generate binary diagnostics
        if(binary_diagnostics.present) {
            output += print_binary_diagnostics(width, binary_diagnostics);
        }
        // generate extra diagnostics
        if(!extra_diagnostics.empty()) {
            output += print_extra_diagnostics(width, extra_diagnostics);
        }
        // generate stack trace
        output += "\nStack trace:\n";
        output += print_stacktrace(static_cast<trace_t*>(raw_trace), width);
        return output;
    }

    LIBASSERT_ATTR_COLD
    std::tuple<const char*, int, std::string, const char*> assertion_printer::get_assertion_info() const {
        const auto& location = params->location;
        auto function = prettify_type(processed_args.pretty_function);
        return {location.file, location.line, std::move(function), processed_args.message.c_str()};
    }
}

namespace libassert::utility {
    LIBASSERT_ATTR_COLD [[nodiscard]] std::string stacktrace(int width) {
        auto trace = cpptrace::generate_trace();
        return print_stacktrace(&trace, width);
    }
}

// Default handler

LIBASSERT_ATTR_COLD
void libassert_default_fail_action(
    libassert::assert_type type,
    ASSERTION fatal,
    const libassert::assertion_printer& printer
) {
    libassert::detail::enable_virtual_terminal_processing_if_needed(); // for terminal colors on windows
    std::string message = printer(libassert::utility::terminal_width(STDERR_FILENO));
    if(libassert::detail::isatty(STDERR_FILENO) && libassert::config::output_colors) {
        if(!libassert::config::output_rgb) {
            message = libassert::utility::replace_rgb(std::move(message));
        }
        std::cerr << message << std::endl;
    } else {
        std::cerr << libassert::utility::strip_colors(message) << std::endl;
    }
    switch(type) {
        case libassert::assert_type::debug_assertion:
        case libassert::assert_type::assertion:
            if(fatal == ASSERTION::FATAL) {
                case libassert::assert_type::assumption: // switch-if-case, cursed!
                (void)fflush(stderr);
                // NOLINTNEXTLINE(misc-include-cleaner)
                abort();
            }
            // Breaking here as debug CRT allows aborts to be ignored, if someone wants to make a debug build of
            // this library (on top of preventing fallthrough from nonfatal libassert)
            break;
        case libassert::assert_type::verification:
            if(fatal == ASSERTION::FATAL) {
                throw libassert::verification_failure();
            }
            break;
        default:
            LIBASSERT_PRIMITIVE_ASSERT(false);
    }
}
