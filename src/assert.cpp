#ifndef _CRT_SECURE_NO_WARNINGS
// NOLINTNEXTLINE(bugprone-reserved-identifier, cert-dcl37-c, cert-dcl51-cpp)
#define _CRT_SECURE_NO_WARNINGS // done only for strerror
#endif
#include <assert/assert.hpp>

// Copyright (c) 2021-2023 Jeremy Rifkin under the MIT license
// https://github.com/jeremy-rifkin/libassert

#include <algorithm>              // for max, min, max_element, reverse, sort
#include <atomic>                 // for atomic_bool
#include <bitset>                 // for operator<<, bitset
#include <cstddef>                // for size_t
#include <cstdint>                // for uintptr_t
#include <cstdio>                 // for fflush, stderr
#include <cstdlib>                // for abort, abs
#include <cstring>                // for strerror
#include <initializer_list>       // for initializer_list
#include <iomanip>                // for operator<<, setprecision
#include <iostream>               // for cerr
#include <limits>                 // for numeric_limits
#include <memory>                 // for unique_ptr, make_unique
#include <regex>                  // for regex_replace, regex
#include <sstream>                // for basic_ostream, basic_ostringstream
#include <string_view>            // for basic_string_view, string_view, ope...
#include <string>                 // for basic_string, string, allocator
#include <system_error>           // for error_code, error_condition, error_...
#include <tuple>                  // for tuple
#include <type_traits>            // for enable_if, is_floating_point
#include <unordered_map>          // for unordered_map, operator!=, _Node_it...
#include <utility>                // for pair, move
#include <vector>                 // for vector

#include <cpptrace/cpptrace.hpp>

#include "common.hpp"
#include "utils.hpp"
#include "analysis.hpp"

#define IS_WINDOWS 0

#if defined(_WIN32)
 #undef IS_WINDOWS
 #define IS_WINDOWS 1
 #ifndef STDIN_FILENO
  #define STDIN_FILENO _fileno(stdin)
  #define STDOUT_FILENO _fileno(stdout)
  #define STDERR_FILENO _fileno(stderr)
 #endif
 #include <windows.h>
 #include <io.h>
 #undef min // fucking windows headers, man
 #undef max
#elif defined(__linux) || defined(__APPLE__) || defined(__unix__)
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

using namespace std::string_literals;
using namespace std::string_view_literals;

// Container utility
template<typename N> class small_static_map {
    // TODO: Re-evaluate
    const N& needle;
public:
    explicit small_static_map(const N& n) : needle(n) {}
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
        #if IS_WINDOWS
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
    static std::atomic_bool output_colors = true;

    LIBASSERT_ATTR_COLD void set_color_output(bool enable) {
        output_colors = enable;
    }

    static std::atomic_bool output_rgb = true;

    LIBASSERT_ATTR_COLD void set_rgb_output(bool enable) {
        output_rgb = enable;
    }
}

namespace libassert::detail {

    /*
     * system wrappers
     */

    LIBASSERT_ATTR_COLD void enable_virtual_terminal_processing_if_needed() {
        // enable colors / ansi processing if necessary
        #if IS_WINDOWS
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
        #if IS_WINDOWS
         return _isatty(fd);
        #else
         return ::isatty(fd);
        #endif
    }

    // NOTE: Not thread-safe. Must be called in a thread-safe manner.
    LIBASSERT_ATTR_COLD std::string strerror_wrapper(int e) {
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        return strerror(e);
    }

    LIBASSERT_ATTR_COLD
    opaque_trace::~opaque_trace() {
        delete static_cast<cpptrace::raw_trace*>(trace);
    }

    LIBASSERT_ATTR_COLD
    opaque_trace get_stacktrace_opaque() {
        return {new cpptrace::raw_trace(cpptrace::generate_raw_trace())};
    }

    /*
     * stringification
     */

    LIBASSERT_ATTR_COLD
    static std::string escape_string(const std::string_view str, char quote) {
        std::string escaped;
        escaped += quote;
        for(const char c : str) {
            if(c == '\\') escaped += "\\\\";
            else if(c == '\t') escaped += "\\t";
            else if(c == '\r') escaped += "\\r";
            else if(c == '\n') escaped += "\\n";
            else if(c == quote) escaped += "\\" + std::to_string(quote);
            else if(c >= 32 && c <= 126) escaped += c; // printable
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

    template<typename T>
    LIBASSERT_ATTR_COLD
    static constexpr T n_digits(T value) {
        return value < 10 ? 1 : 1 + n_digits(value / 10);
    }

    static_assert(n_digits(0) == 1);
    static_assert(n_digits(1) == 1);
    static_assert(n_digits(9) == 1);
    static_assert(n_digits(10) == 2);
    static_assert(n_digits(11) == 2);
    static_assert(n_digits(1024) == 4);

    using path_components = std::vector<std::string>;

    LIBASSERT_ATTR_COLD
    static path_components parse_path(const std::string_view path) {
        #if IS_WINDOWS
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
                if(part.empty()) {
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
        std::unordered_map<std::string, std::unique_ptr<path_trie>> edges;
    public:
        LIBASSERT_ATTR_COLD
        explicit path_trie(std::string _root) : root(std::move(_root)) {};
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
                current = current->edges.at(component).get();
                result.push_back(current->root);
            }
            std::reverse(result.begin(), result.end());
            return result;
        }
    private:
        LIBASSERT_ATTR_COLD
        void insert(const path_components& path, int i) {
            if(i < 0) {
                return;
            }
            if(!edges.count(path[i])) {
                if(!edges.empty()) {
                    downstream_branches++; // this is to deal with making leaves have count 1
                }
                edges.insert({path[i], std::make_unique<path_trie>(path[i])});
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
    auto get_trace_window(const cpptrace::stacktrace& trace) {
        // Two boundaries: assert_detail and main
        // Both are found here, nothing is filtered currently at stack trace generation
        // (inlining and platform idiosyncrasies interfere)
        size_t start = 0;
        size_t end = trace.frames.size() - 1;
        for(size_t i = 0; i < trace.frames.size(); i++) {
            if(trace.frames[i].symbol.find("libassert::detail::") != std::string::npos) {
                start = i + 1;
            }
            if(trace.frames[i].symbol == "main" || trace.frames[i].symbol.find("main(") == 0) {
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
    auto process_paths(const cpptrace::stacktrace& trace, size_t start, size_t end) {
        // raw full path -> components
        std::unordered_map<std::string, path_components> parsed_paths;
        // base file name -> path trie
        std::unordered_map<std::string, path_trie> tries;
        for(size_t i = start; i <= end; i++) {
            const auto& source_path = trace.frames[i].filename;
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
    std::string print_stacktrace(const cpptrace::raw_trace* raw_trace, int term_width) {
        std::string stacktrace;
        if(raw_trace && !raw_trace->empty()) {
            auto trace = raw_trace->resolve();
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
            const size_t max_line_number_width = n_digits(max_line_number);
            const size_t max_frame_width = n_digits(end - start);
            // do the actual trace
            for(size_t i = start; i <= end; i++) {
                const auto& [address, line, col, source_path, signature, is_inline] = trace.frames[i];
                const std::string line_number = line == 0 ? "?" : std::to_string(line);
                // look for repeats, i.e. recursion we can fold
                size_t recursion_folded = 0;
                if(end - i >= 4) {
                    size_t j = 1;
                    for( ; i + j <= end; j++) {
                        if(trace.frames[i + j] != trace.frames[i] || trace.frames[i + j].symbol == "??") {
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
        const std::string& a_str = a_sstr;
        const std::string& b_str = b_sstr;
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
                has_useful_where_clause.left  ? a_str.size() : 0,
                has_useful_where_clause.right ? b_str.size() : 0
            );
            // Limit lw to about half the screen. TODO: Re-evaluate what we want to do here.
            if(term_width > 0) {
                lw = std::min(lw, term_width / 2 - where_indent - arrow_width);
            }
            where += "    Where:\n";
            auto print_clause = [term_width, lw, &where](
                const std::string& expr_str,
                const std::vector<std::string>& expr_strs
            ) {
                if(term_width >= min_term_width) {
                    where += wrapped_print({
                        { where_indent - 1, {{"", ""}} }, // 8 space indent, wrapper will add a space
                        { lw, highlight_blocks(expr_str) },
                        { 2, {{"", "=>"}} },
                        { term_width - lw - 8 /* indent */ - 4 /* arrow */, get_values(expr_strs) }
                    });
                } else {
                    where += stringf(
                        "        %s%*s => ",
                        highlight(expr_str).c_str(),
                        int(lw - expr_str.size()),
                        ""
                    );
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
        const assert_static_parameters* _params,
        const extra_diagnostics& _processed_args,
        binary_diagnostics_descriptor& _binary_diagnostics,
        void* _raw_trace,
        size_t _sizeof_args
    ) :
        params(_params),
        processed_args(_processed_args),
        binary_diagnostics(_binary_diagnostics),
        raw_trace(_raw_trace),
        sizeof_args(_sizeof_args) {}

    LIBASSERT_ATTR_COLD assertion_printer::~assertion_printer() {
        auto* trace = static_cast<cpptrace::raw_trace*>(raw_trace);
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
        output += print_stacktrace(static_cast<cpptrace::raw_trace*>(raw_trace), width);
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
        auto trace = cpptrace::generate_raw_trace();
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
                std::abort();
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
