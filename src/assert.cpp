#include <assert/assert.hpp>

// Copyright (c) 2021-2024 Jeremy Rifkin under the MIT license
// https://github.com/jeremy-rifkin/libassert

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <string_view>
#include <string>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <cpptrace/cpptrace.hpp>

#include "common.hpp"
#include "utils.hpp"
#include "analysis.hpp"
#include "platform.hpp"
#include "paths.hpp"
#include "printing.hpp"

#if LIBASSERT_IS_MSVC
 // wchar -> char string warning
 #pragma warning(disable : 4244)
#endif

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace libassert::detail {
    LIBASSERT_ATTR_COLD
    opaque_trace::~opaque_trace() {
        delete static_cast<cpptrace::raw_trace*>(trace);
    }

    LIBASSERT_ATTR_COLD
    opaque_trace get_stacktrace_opaque() {
        return {new cpptrace::raw_trace(cpptrace::generate_raw_trace())};
    }

    /*
     * stack trace printing
     */

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
        return std::pair(start, end);
    }

    LIBASSERT_ATTR_COLD [[nodiscard]]
    // TODO
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    std::string print_stacktrace(const cpptrace::raw_trace* raw_trace, int term_width, color_scheme scheme) {
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
            std::vector<std::string> paths;
            for(std::size_t i = start; i <= end; i++) {
                paths.push_back(trace.frames[i].filename);
            }
            auto [files, longest_file_width] = process_paths(paths);
            // figure out column widths
            const auto max_line_number =
                std::max_element(
                    // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
                    trace.begin() + start,
                    // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
                    trace.begin() + end + 1,
                    [](const cpptrace::stacktrace_frame& a, const cpptrace::stacktrace_frame& b) {
                        return a.line.value_or(0) < b.line.value_or(0);
                    }
                )->line;
            const size_t max_line_number_width = n_digits(max_line_number.value_or(0));
            const size_t max_frame_width = n_digits(end - start);
            // do the actual trace
            for(size_t i = start; i <= end; i++) {
                const auto& [address, line, col, source_path, signature, is_inline] = trace.frames[i];
                const std::string line_number = line.has_value() ? std::to_string(line.value()) : "?";
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
                    auto sig = highlight_blocks(signature + "(", scheme); // hack for the highlighter
                    sig.pop_back();
                    const size_t left = 2 + max_frame_width;
                    // todo: is this looking right...?
                    const size_t middle = std::max(line_number.size(), max_line_number_width);
                    const size_t remaining_width = term_width - (left + middle + 3 /* spaces */);
                    LIBASSERT_PRIMITIVE_ASSERT(remaining_width >= 2);
                    const size_t file_width = std::min({longest_file_width, remaining_width / 2, max_file_length});
                    const size_t sig_width = remaining_width - file_width;
                    stacktrace += wrapped_print(
                        {
                            { 1,          {{"", "#"}} },
                            { left - 2,   highlight_blocks(std::to_string(frame_number), scheme), true },
                            { file_width, {{"", files.at(source_path)}} },
                            { middle,     highlight_blocks(line_number, scheme), true }, // intentionally not coloring "?"
                            { sig_width,  sig }
                        },
                        scheme
                    );
                } else {
                    auto sig = highlight(signature + "(", scheme); // hack for the highlighter
                    sig = sig.substr(0, sig.rfind('('));
                    stacktrace += stringf(
                        "#%s%2d%s %s\n      at %s:%s%s%s\n",
                        std::string(scheme.number).c_str(),
                        (int)frame_number,
                        std::string(scheme.reset).c_str(),
                        sig.c_str(),
                        files.at(source_path).c_str(),
                        std::string(scheme.number).c_str(),
                        line_number.c_str(),
                        std::string(scheme.reset).c_str() // yes this is excessive; intentionally coloring "?"
                    );
                }
                if(recursion_folded) {
                    i += recursion_folded;
                    const std::string s = stringf("| %d layers of recursion were folded |", recursion_folded);
                    (((stacktrace += scheme.accent) += stringf("|%*s|", int(s.size() - 2), "")) += scheme.reset) += '\n';
                    (((stacktrace += scheme.accent) += stringf("%s", s.c_str())) += scheme.reset) += '\n';
                    (((stacktrace += scheme.accent) += stringf("|%*s|", int(s.size() - 2), "")) += scheme.reset) += '\n';
                }
            }
        } else {
            stacktrace += "Error while generating stack trace.\n";
        }
        return stacktrace;
    }

    LIBASSERT_ATTR_COLD binary_diagnostics_descriptor::binary_diagnostics_descriptor() = default;
    LIBASSERT_ATTR_COLD binary_diagnostics_descriptor::binary_diagnostics_descriptor(
        std::string&& _lstring,
        std::string&& _rstring,
        std::string _a_str,
        std::string _b_str,
        bool _multiple_formats
    ):
        lstring(_lstring),
        rstring(_rstring),
        a_str(std::move(_a_str)),
        b_str(std::move(_b_str)),
        multiple_formats(_multiple_formats),
        present(true) {}
    LIBASSERT_ATTR_COLD binary_diagnostics_descriptor::~binary_diagnostics_descriptor() = default;
    LIBASSERT_ATTR_COLD
    binary_diagnostics_descriptor::binary_diagnostics_descriptor(binary_diagnostics_descriptor&&) noexcept = default;
    LIBASSERT_ATTR_COLD binary_diagnostics_descriptor&
    binary_diagnostics_descriptor::operator=(binary_diagnostics_descriptor&&) noexcept(LIBASSERT_GCC_ISNT_STUPID) = default;

    LIBASSERT_ATTR_COLD
    static std::string print_values(const std::vector<std::string>& vec, size_t lw, color_scheme scheme) {
        LIBASSERT_PRIMITIVE_ASSERT(!vec.empty());
        std::string values;
        if(vec.size() == 1) {
            values += stringf("%s\n", indent(highlight(vec[0], scheme), 8 + lw + 4, ' ', true).c_str());
        } else {
            // spacing here done carefully to achieve <expr> =  <a>  <b>  <c>, or similar
            // no indentation done here for multiple value printing
            values += " ";
            for(const auto& str : vec) {
                values += stringf("%s", highlight(str, scheme).c_str());
                if(&str != &*--vec.end()) {
                    values += "  ";
                }
            }
            values += "\n";
        }
        return values;
    }

    LIBASSERT_ATTR_COLD
    static std::vector<highlight_block> get_values(const std::vector<std::string>& vec, color_scheme scheme) {
        LIBASSERT_PRIMITIVE_ASSERT(!vec.empty());
        if(vec.size() == 1) {
            return highlight_blocks(vec[0], scheme);
        } else {
            std::vector<highlight_block> blocks;
            // spacing here done carefully to achieve <expr> =  <a>  <b>  <c>, or similar
            // no indentation done here for multiple value printing
            blocks.push_back({"", " "});
            for(const auto& str : vec) {
                auto h = highlight_blocks(str, scheme);
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
    std::string print_binary_diagnostics(
        binary_diagnostics_descriptor& diagnostics,
        size_t term_width,
        color_scheme scheme
    ) {
        auto& [ lstring, rstring, a_sstr, b_sstr, multiple_formats, _ ] = diagnostics;
        const std::string& a_str = a_sstr;
        const std::string& b_str = b_sstr;
        // TODO: Temporary hack while reworking
        std::vector<std::string> lstrings = { lstring };
        std::vector<std::string> rstrings = { rstring };
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
            auto print_clause = [term_width, lw, &where, &scheme](
                const std::string& expr_str,
                const std::vector<std::string>& expr_strs
            ) {
                if(term_width >= min_term_width) {
                    where += wrapped_print(
                        {
                            { where_indent - 1, {{"", ""}} }, // 8 space indent, wrapper will add a space
                            { lw, highlight_blocks(expr_str, scheme) },
                            { 2, {{"", "=>"}} },
                            { term_width - lw - 8 /* indent */ - 4 /* arrow */, get_values(expr_strs, scheme) }
                        },
                        scheme
                    );
                } else {
                    where += stringf(
                        "        %s%*s => ",
                        highlight(expr_str, scheme).c_str(),
                        int(lw - expr_str.size()),
                        ""
                    );
                    where += print_values(expr_strs, lw, scheme);
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

    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string print_extra_diagnostics(
        const decltype(extra_diagnostics::entries)& extra_diagnostics,
        size_t term_width,
        color_scheme scheme
    ) {
        std::string output = "    Extra diagnostics:\n";
        size_t lw = 0;
        for(const auto& entry : extra_diagnostics) {
            lw = std::max(lw, entry.first.size());
        }
        for(const auto& entry : extra_diagnostics) {
            if(term_width >= min_term_width) {
                output += wrapped_print(
                    {
                        { 7, {{"", ""}} }, // 8 space indent, wrapper will add a space
                        { lw, highlight_blocks(entry.first, scheme) },
                        { 2, {{"", "=>"}} },
                        { term_width - lw - 8 /* indent */ - 4 /* arrow */, highlight_blocks(entry.second, scheme) }
                    },
                    scheme
                );
            } else {
                output += stringf(
                    "        %s%*s => %s\n",
                    highlight(entry.first, scheme).c_str(),
                    int(lw - entry.first.length()),
                    "",
                    indent(
                        highlight(entry.second, scheme),
                        8 + lw + 4,
                        ' ',
                        true
                    ).c_str()
                );
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
            case assert_type::debug_assertion: return "Debug Assertion failed";
            case assert_type::assertion:       return "Assertion failed";
            case assert_type::assumption:      return "Assumption failed";
            case assert_type::verification:    return "Verification failed";
            case assert_type::panic:           return "Panic";
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
    static std::atomic_bool output_colors = true;

    LIBASSERT_ATTR_COLD void set_color_output(bool enable) {
        output_colors = enable;
    }

    LIBASSERT_EXPORT color_scheme ansi_basic {
        BASIC_GREEN, /* string */
        BASIC_BLUE, /* escape */
        BASIC_PURPL, /* keyword */
        BASIC_ORANGE, /* named_literal */
        BASIC_CYAN, /* number */
        BASIC_PURPL, /* operator_token */
        BASIC_BLUE, /* call_identifier */
        BASIC_YELLOW, /* scope_resolution_identifier */
        BASIC_BLUE, /* identifier */
        BASIC_BLUE, /* accent */
        RESET
    };

    LIBASSERT_EXPORT color_scheme ansi_rgb {
        RGB_GREEN, /* string */
        RGB_BLUE, /* escape */
        RGB_PURPL, /* keyword */
        RGB_ORANGE, /* named_literal */
        RGB_CYAN, /* number */
        RGB_PURPL, /* operator_token */
        RGB_BLUE, /* call_identifier */
        RGB_YELLOW, /* scope_resolution_identifier */
        RGB_BLUE, /* identifier */
        RGB_BLUE, /* accent */
        RESET
    };

    LIBASSERT_EXPORT color_scheme blank_color_scheme;

    std::mutex color_scheme_mutex;
    color_scheme current_color_scheme = ansi_rgb;

    LIBASSERT_EXPORT void set_color_scheme(color_scheme scheme) {
        std::unique_lock lock(color_scheme_mutex);
        current_color_scheme = scheme;
    }

    LIBASSERT_EXPORT color_scheme get_color_scheme() {
        std::unique_lock lock(color_scheme_mutex);
        return current_color_scheme;
    }

    namespace detail {
        LIBASSERT_ATTR_COLD
        void libassert_default_failure_handler(
            assert_type type,
            const assertion_printer& printer
        ) {
            // TODO: Just throw instead of all of this?
            enable_virtual_terminal_processing_if_needed(); // for terminal colors on windows
            std::string message = printer(
                terminal_width(STDERR_FILENO),
                isatty(STDERR_FILENO) && output_colors ? get_color_scheme() : blank_color_scheme
            );
            std::cerr << message << std::endl;
            switch(type) {
                case assert_type::debug_assertion:
                case assert_type::assertion:
                    case assert_type::assumption: // switch-if-case, cursed!
                    (void)fflush(stderr);
                    std::abort();
                    // Breaking here as debug CRT allows aborts to be ignored, if someone wants to make a debug build of
                    // this library (on top of preventing fallthrough from nonfatal libassert)
                    break;
                case assert_type::verification:
                    throw verification_failure();
                    break;
                default:
                    LIBASSERT_PRIMITIVE_ASSERT(false);
            }
        }
    }

    static std::atomic failure_handler = detail::libassert_default_failure_handler;

    LIBASSERT_ATTR_COLD LIBASSERT_EXPORT
    void set_failure_handler(void (*handler)(assert_type, const assertion_printer&)) {
        failure_handler = handler;
    }

    namespace detail {
        LIBASSERT_ATTR_COLD LIBASSERT_EXPORT void fail(assert_type type, const assertion_printer& printer) {
            failure_handler.load()(type, printer);
        }
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

    LIBASSERT_ATTR_COLD std::string assertion_printer::operator()(int width, color_scheme scheme) const {
        const auto& [ name, type, expr_str, location, args_strings ] = *params;
        const auto& [ message, extra_diagnostics, pretty_function ] = processed_args;
        std::string output;
        // generate header
        const auto function = prettify_type(pretty_function);
        if(!message.empty()) {
            output += stringf(
                "%s at %s:%d: %s: %s\n",
                assert_type_name(type),
                location.file,
                location.line,
                highlight(function, scheme).c_str(),
                message.c_str()
            );
        } else {
            output += stringf(
                "%s at %s:%d: %s:\n",
                assert_type_name(type),
                location.file,
                location.line,
                highlight(function, scheme).c_str()
            );
        }
        output += stringf(
            "    %s\n",
            highlight(
                stringf(
                    "%s(%s%s);",
                    name,
                    expr_str,
                    sizeof_args > 0 ? (strlen(expr_str) == 0 ? "..." : ", ...") : ""
                ),
                scheme
            ).c_str()
        );
        // generate binary diagnostics
        if(binary_diagnostics.present) {
            output += print_binary_diagnostics(binary_diagnostics, width, scheme);
        }
        // generate extra diagnostics
        if(!extra_diagnostics.empty()) {
            output += print_extra_diagnostics(extra_diagnostics, width, scheme);
        }
        // generate stack trace
        output += "\nStack trace:\n";
        output += print_stacktrace(static_cast<cpptrace::raw_trace*>(raw_trace), width, scheme);
        return output;
    }

    LIBASSERT_ATTR_COLD
    std::tuple<const char*, int, std::string, const char*> assertion_printer::get_assertion_info() const {
        const auto& location = params->location;
        auto function = prettify_type(processed_args.pretty_function);
        return {location.file, location.line, std::move(function), processed_args.message.c_str()};
    }
}

namespace libassert {
    LIBASSERT_ATTR_COLD [[nodiscard]] std::string stacktrace(int) {
        auto trace = cpptrace::generate_raw_trace();
        return ""; //print_stacktrace(&trace, width); // TODO FIXME
    }
}
