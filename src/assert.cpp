#include <libassert/assert.hpp>

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
#include "microfmt.hpp"
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

    struct stacktrace_result {
        std::string printed;
    };

    LIBASSERT_ATTR_COLD [[nodiscard]]
    // TODO
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    std::string print_stacktrace(
        const cpptrace::stacktrace& trace,
        int term_width,
        const color_scheme& scheme,
        path_handler* path_handler
    ) {
        std::string stacktrace;
        if(!trace.empty()) {
            // [start, end] is an inclusive range
            auto [start, end] = get_trace_window(trace);
            // path preprocessing
            constexpr size_t hard_max_file_length = 50;
            size_t max_file_length = 0;
            for(std::size_t i = start; i <= end; i++) {
                max_file_length = std::max(
                    path_handler->resolve_path(trace.frames[i].filename).size(),
                    max_file_length
                );
            }
            max_file_length = std::min(max_file_length, hard_max_file_length);
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
                const auto& [raw_address, obj_address, line, col, source_path, signature_, is_inline] = trace.frames[i];
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
                auto signature = prettify_type(signature_);
                // pretty print with columns for wide terminals
                // split printing for small terminals
                if(term_width >= 50) {
                    auto sig = highlight_blocks(signature + "(", scheme); // hack for the highlighter
                    sig.pop_back();
                    const size_t left = 1 + max_frame_width;
                    // todo: is this looking right...?
                    const size_t line_number_width = std::max(line_number.size(), max_line_number_width);
                    const size_t remaining_width = term_width - (left + line_number_width + 2 /* spaces */ + 1 /* : */);
                    const size_t file_width = std::min({max_file_length, remaining_width / 2, max_file_length});
                    LIBASSERT_PRIMITIVE_ASSERT(remaining_width >= 2);
                    const size_t sig_width = remaining_width - file_width;
                    std::vector<highlight_block> location_blocks = concat(
                        {{"", std::string(path_handler->resolve_path(source_path)) + ":"}},
                        highlight_blocks(line_number, scheme)
                    );
                    stacktrace += wrapped_print(
                        {
                            { left, {{"", "#"}, {scheme.number, std::to_string(frame_number)}}, true },
                            { file_width + 1 + line_number_width, location_blocks },
                            { sig_width, sig }
                        },
                        scheme
                    );
                } else {
                    auto sig = highlight(signature + "(", scheme); // hack for the highlighter
                    sig = sig.substr(0, sig.rfind('('));
                    stacktrace += microfmt::format(
                        "#{}{>2}{} {}\n      at {}:{}{}{}\n",
                        scheme.number,
                        frame_number,
                        scheme.reset,
                        sig,
                        path_handler->resolve_path(source_path),
                        scheme.number,
                        line_number,
                        scheme.reset // yes this is excessive; intentionally coloring "?"
                    );
                }
                if(recursion_folded) {
                    i += recursion_folded;
                    const std::string s = microfmt::format("| {} layers of recursion were folded |", recursion_folded);
                    stacktrace += microfmt::format("{}|{<{}}|{}\n", scheme.accent, s.size() - 2, "", scheme.reset);
                    stacktrace += microfmt::format("{}{}{}\n", scheme.accent, s, scheme.reset);
                    stacktrace += microfmt::format("{}|{<{}}|{}\n", scheme.accent, s.size() - 2, "", scheme.reset);
                }
            }
        } else {
            stacktrace += "Error while generating stack trace.\n";
        }
        return stacktrace;
    }

    LIBASSERT_ATTR_COLD
    static std::string print_values(const std::vector<std::string>& vec, size_t lw, const color_scheme& scheme) {
        LIBASSERT_PRIMITIVE_ASSERT(!vec.empty());
        std::string values;
        if(vec.size() == 1) {
            values += microfmt::format("{}\n", indent(highlight(vec[0], scheme), 8 + lw + 4, ' ', true));
        } else {
            // spacing here done carefully to achieve <expr> =  <a>  <b>  <c>, or similar
            // no indentation done here for multiple value printing
            values += " ";
            for(const auto& str : vec) {
                values += microfmt::format("{}", highlight(str, scheme));
                if(&str != &*--vec.end()) {
                    values += "  ";
                }
            }
            values += "\n";
        }
        return values;
    }

    LIBASSERT_ATTR_COLD
    static std::vector<highlight_block> get_values(const std::vector<std::string>& vec, const color_scheme& scheme) {
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
    constexpr size_t where_indent = 8;
    std::string arrow = "=>";

    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string print_binary_diagnostics(
        const binary_diagnostics_descriptor& diagnostics,
        size_t term_width,
        const color_scheme& scheme
    ) {
        auto& [
            left_expression,
            right_expression,
            left_stringification,
            right_stringification,
            multiple_formats
        ] = diagnostics;
        // TODO: Temporary hack while reworking
        std::vector<std::string> lstrings = { left_stringification };
        std::vector<std::string> rstrings = { right_stringification };
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
            multiple_formats
                || lstrings.size() > 1
                || (left_expression != lstrings[0] && trim_suffix(left_expression) != lstrings[0]),
            multiple_formats
                || rstrings.size() > 1
                || (right_expression != rstrings[0] && trim_suffix(right_expression) != rstrings[0])
        };
        // print where clause
        std::string where;
        if(has_useful_where_clause.left || has_useful_where_clause.right) {
            size_t lw = std::max(
                has_useful_where_clause.left  ? left_expression.size() : 0,
                has_useful_where_clause.right ? right_expression.size() : 0
            );
            // Limit lw to about half the screen. TODO: Re-evaluate what we want to do here.
            if(term_width > 0) {
                lw = std::min(lw, term_width / 2 - where_indent - (arrow.size() + 2));
            }
            where += "    Where:\n";
            auto print_clause = [term_width, lw, &where, &scheme](
                std::string_view expr_str,
                const std::vector<std::string>& expr_strs
            ) {
                if(term_width >= min_term_width) {
                    where += wrapped_print(
                        {
                            { where_indent - 1, {{"", ""}} }, // 8 space indent, wrapper will add a space
                            { lw, highlight_blocks(expr_str, scheme) },
                            { arrow.size(), {{"", arrow}} },
                            { term_width - lw - 8 /* indent */ - 4 /* arrow */, get_values(expr_strs, scheme) }
                        },
                        scheme
                    );
                } else {
                    where += microfmt::format(
                        "        {}{<{}} {} ",
                        highlight(expr_str, scheme),
                        lw - expr_str.size(),
                        "",
                        arrow
                    );
                    where += print_values(expr_strs, lw, scheme);
                }
            };
            if(has_useful_where_clause.left) {
                print_clause(left_expression, lstrings);
            }
            if(has_useful_where_clause.right) {
                print_clause(right_expression, rstrings);
            }
        }
        return where;
    }

    LIBASSERT_ATTR_COLD [[nodiscard]]
    std::string print_extra_diagnostics(
        const std::vector<extra_diagnostic>& extra_diagnostics,
        size_t term_width,
        const color_scheme& scheme
    ) {
        std::string output = "    Extra diagnostics:\n";
        size_t lw = 0;
        for(const auto& entry : extra_diagnostics) {
            lw = std::max(lw, entry.expression.size());
        }
        for(const auto& entry : extra_diagnostics) {
            if(term_width >= min_term_width) {
                output += wrapped_print(
                    {
                        { 7, {{"", ""}} }, // 8 space indent, wrapper will add a space
                        { lw, highlight_blocks(entry.expression, scheme) },
                        { arrow.size(), {{"", arrow}} },
                        { term_width - lw - 8 /* indent */ - 4 /* arrow */, highlight_blocks(entry.stringification, scheme) }
                    },
                    scheme
                );
            } else {
                output += microfmt::format(
                    "        {}{<{}} {} {}\n",
                    highlight(entry.expression, scheme),
                    lw - entry.expression.length(),
                    "",
                    arrow,
                    indent(
                        highlight(entry.stringification, scheme),
                        8 + lw + 4,
                        ' ',
                        true
                    )
                );
            }
        }
        return output;
    }
}

namespace libassert {
    LIBASSERT_EXPORT const color_scheme color_scheme::ansi_basic {
        BASIC_GREEN, /* string */
        BASIC_BLUE, /* escape */
        BASIC_PURPL, /* keyword */
        BASIC_ORANGE, /* named_literal */
        BASIC_CYAN, /* number */
        "",
        BASIC_PURPL, /* operator */
        BASIC_BLUE, /* call_identifier */
        BASIC_YELLOW, /* scope_resolution_identifier */
        BASIC_BLUE, /* identifier */
        BASIC_BLUE, /* accent */
        BASIC_RED, /* unknown */
        RESET
    };

    LIBASSERT_EXPORT const color_scheme color_scheme::ansi_rgb {
        RGB_GREEN, /* string */
        RGB_BLUE, /* escape */
        RGB_PURPL, /* keyword */
        RGB_ORANGE, /* named_literal */
        RGB_CYAN, /* number */
        "",
        RGB_PURPL, /* operator */
        RGB_BLUE, /* call_identifier */
        RGB_YELLOW, /* scope_resolution_identifier */
        RGB_BLUE, /* identifier */
        RGB_BLUE, /* accent */
        RGB_RED, /* unknown */
        RESET
    };

    LIBASSERT_EXPORT const color_scheme color_scheme::blank;

    std::mutex color_scheme_mutex;
    color_scheme current_color_scheme = color_scheme::ansi_rgb;

    LIBASSERT_EXPORT void set_color_scheme(const color_scheme& scheme) {
        std::unique_lock lock(color_scheme_mutex);
        current_color_scheme = scheme;
    }

    LIBASSERT_EXPORT const color_scheme& get_color_scheme() {
        std::unique_lock lock(color_scheme_mutex);
        return current_color_scheme;
    }

    LIBASSERT_EXPORT void set_separator(std::string_view separator) {
        detail::arrow = separator;
    }

    std::atomic<path_mode> current_path_mode = path_mode::disambiguated;

    LIBASSERT_EXPORT void set_path_mode(path_mode mode) {
        current_path_mode = mode;
    }

    namespace detail {
        LIBASSERT_ATTR_COLD
        std::unique_ptr<detail::path_handler> new_path_handler() {
            auto mode = current_path_mode.load();
            switch(mode) {
                case path_mode::disambiguated:
                    return std::make_unique<disambiguating_path_handler>();
                case path_mode::basename:
                    return std::make_unique<basename_path_handler>();
                case path_mode::full:
                default:
                    return std::make_unique<identity_path_handler>();
            }
        }

        LIBASSERT_ATTR_COLD
        void libassert_default_failure_handler(const assertion_info& info) {
            enable_virtual_terminal_processing_if_needed(); // for terminal colors on windows
            std::string message = info.to_string(
                terminal_width(STDERR_FILENO),
                isatty(STDERR_FILENO) ? get_color_scheme() : color_scheme::blank
            );
            std::cerr << message << std::endl;
            switch(info.type) {
                case assert_type::assertion:
                case assert_type::debug_assertion:
                case assert_type::assumption:
                case assert_type::panic:
                case assert_type::unreachable:
                    (void)fflush(stderr);
                    std::abort();
                    // Breaking here as debug CRT allows aborts to be ignored, if someone wants to make a debug build of
                    // this library
                    break;
                default:
                    LIBASSERT_PRIMITIVE_ASSERT(false);
            }
        }
    }

    static std::atomic failure_handler = detail::libassert_default_failure_handler;

    LIBASSERT_ATTR_COLD LIBASSERT_EXPORT
    void set_failure_handler(void (*handler)(const assertion_info&)) {
        failure_handler = handler;
    }

    namespace detail {
        LIBASSERT_ATTR_COLD LIBASSERT_EXPORT void fail(const assertion_info& info) {
            failure_handler.load()(info);
        }
    }

    LIBASSERT_ATTR_COLD binary_diagnostics_descriptor::binary_diagnostics_descriptor() = default;
    LIBASSERT_ATTR_COLD binary_diagnostics_descriptor::binary_diagnostics_descriptor(
        std::string_view _left_expression,
        std::string_view _right_expression,
        std::string&& _left_stringification,
        std::string&& _right_stringification,
        bool _multiple_formats
    ):
        left_expression(_left_expression),
        right_expression(_right_expression),
        left_stringification(std::move(_left_stringification)),
        right_stringification(std::move(_right_stringification)),
        multiple_formats(_multiple_formats) {}
    LIBASSERT_ATTR_COLD binary_diagnostics_descriptor::~binary_diagnostics_descriptor() = default;
    LIBASSERT_ATTR_COLD
    binary_diagnostics_descriptor::binary_diagnostics_descriptor(binary_diagnostics_descriptor&&) noexcept = default;
    LIBASSERT_ATTR_COLD binary_diagnostics_descriptor&
    binary_diagnostics_descriptor::operator=(binary_diagnostics_descriptor&&) noexcept(LIBASSERT_GCC_ISNT_STUPID) = default;
}

namespace libassert {
    using namespace detail;

    LIBASSERT_ATTR_COLD assertion_info::assertion_info(
        const assert_static_parameters* static_params,
        cpptrace::raw_trace&& _raw_trace,
        size_t _n_args
    ) :
        macro_name(static_params->macro_name),
        type(static_params->type),
        expression_string(static_params->expr_str),
        file_name(static_params->location.file),
        line(static_params->location.line),
        function("<error>"),
        n_args(_n_args),
        trace(std::move(_raw_trace)) {}

    LIBASSERT_ATTR_COLD assertion_info::~assertion_info() = default;

    assertion_info::assertion_info(assertion_info&&) = default;
    assertion_info& assertion_info::operator=(assertion_info&&) = default;

    path_handler* assertion_info::get_path_handler() const {
        if(!path_handler) {
            path_handler = new_path_handler();
            // if this is a disambiguating handler or similar it needs to be fed all paths
            if(path_handler->has_add_path()) {
                path_handler->add_path(file_name);
                const auto& stacktrace = get_stacktrace();
                for(const auto& frame : stacktrace.frames) {
                    path_handler->add_path(frame.filename);
                }
                path_handler->finalize();
            }
        }
        return path_handler.get();
    }

    LIBASSERT_ATTR_COLD std::string_view assertion_info::action() const {
        switch(type) {
            case assert_type::debug_assertion: return "Debug Assertion failed";
            case assert_type::assertion:       return "Assertion failed";
            case assert_type::assumption:      return "Assumption failed";
            case assert_type::panic:           return "Panic";
            case assert_type::unreachable:     return "Unreachable reached";
            default:
                return "Unknown assertion";
        }
    }

    LIBASSERT_ATTR_COLD const cpptrace::raw_trace& assertion_info::get_raw_trace() const {
        try {
            return std::get<cpptrace::raw_trace>(trace);
        } catch(std::bad_variant_access&) {
            throw cpptrace::runtime_error("assertion_info::get_raw_trace may only be called before assertion_info::get_stacktrace is called because that resoles the trace internally");
        }
    }

    LIBASSERT_ATTR_COLD const cpptrace::stacktrace& assertion_info::get_stacktrace() const {
        if(trace.index() == 0) {
            // do resolution
            auto raw_trace = std::move(std::get<cpptrace::raw_trace>(trace));
            trace = raw_trace.resolve();
        }
        return std::get<cpptrace::stacktrace>(trace);
    }

    std::string assertion_info::header(int width, const color_scheme& scheme) const {
        return tagline(scheme)
            + statement(scheme)
            + print_binary_diagnostics(width, scheme)
            + print_extra_diagnostics(width, scheme);
    }

    std::string assertion_info::tagline(const color_scheme& scheme) const {
        const auto prettified_function = prettify_type(std::string(function));
        if(message && !message->empty()) {
            return microfmt::format(
                "{} at {}:{}: {}: {}\n",
                action(),
                get_path_handler()->resolve_path(file_name),
                line,
                highlight(prettified_function, scheme),
                *message
            );
        } else {
            return microfmt::format(
                "{} at {}:{}: {}:\n",
                action(),
                get_path_handler()->resolve_path(file_name),
                line,
                highlight(prettified_function, scheme)
            );
        }
    }

    std::string assertion_info::location() const {
        return microfmt::format("{}:{}", get_path_handler()->resolve_path(file_name), line);
    }

    std::string assertion_info::statement(const color_scheme& scheme) const {
        return microfmt::format(
            "    {}\n",
            highlight(
                microfmt::format(
                    "{}({}{});",
                    macro_name,
                    expression_string,
                    n_args > 0 ? (expression_string.empty() ? "..." : ", ...") : ""
                ),
                scheme
            )
        );
    }

    std::string assertion_info::print_binary_diagnostics(int width, const color_scheme& scheme) const {
        if(binary_diagnostics) {
            return libassert::detail::print_binary_diagnostics(*binary_diagnostics, width, scheme);
        } else {
            return "";
        }
    }

    std::string assertion_info::print_extra_diagnostics(int width, const color_scheme& scheme) const {
        if(!extra_diagnostics.empty()) {
            return libassert::detail::print_extra_diagnostics(extra_diagnostics, width, scheme);
        } else {
            return "";
        }
    }

    std::string assertion_info::print_stacktrace(int width, const color_scheme& scheme) const {
        std::string output = "Stack trace:\n";
        return libassert::detail::print_stacktrace(get_stacktrace(), width, scheme, get_path_handler());
    }

    LIBASSERT_ATTR_COLD std::string assertion_info::to_string(int width, const color_scheme& scheme) const {
        // auto& stacktrace = get_stacktrace(); // TODO
        // now do output
        std::string output;
        // generate statement
        output += tagline(scheme);
        output += statement(scheme);
        output += print_binary_diagnostics(width, scheme);
        output += print_extra_diagnostics(width, scheme);
        // generate stack trace
        output += "\nStack trace:\n";
        output += print_stacktrace(width, scheme);
        return output;
    }
}

namespace libassert {
    LIBASSERT_ATTR_COLD LIBASSERT_ATTR_NOINLINE [[nodiscard]]
    std::string stacktrace(int width, const color_scheme& scheme, std::size_t skip) {
        auto trace = cpptrace::generate_trace(skip + 1);
        detail::identity_path_handler handler;
        return print_stacktrace(trace, width, scheme, &handler);
    }
}
