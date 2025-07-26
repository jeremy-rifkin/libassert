#ifndef LIBASSERT_HPP
#define LIBASSERT_HPP

// Copyright (c) 2021-2025 Jeremy Rifkin under the MIT license
// https://github.com/jeremy-rifkin/libassert

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <string_view>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <libassert/platform.hpp>
#include <libassert/utilities.hpp>
#include <libassert/stringification.hpp>
#include <libassert/expression-decomposition.hpp>
#include <libassert/assert-macros.hpp>

#if defined(__has_include) && __has_include(<cpptrace/forward.hpp>)
 #include <cpptrace/forward.hpp>
#else
 #include <cpptrace/cpptrace.hpp>
#endif

#if LIBASSERT_IS_MSVC
 #pragma warning(push)
 // warning C4251: using non-dll-exported type in dll-exported type, firing on std::vector<frame_ptr> and others for
 // some reason
 // 4275 is the same thing but for base classes
 #pragma warning(disable: 4251; disable: 4275)
#endif

// =====================================================================================================================
// || Libassert public interface                                                                                      ||
// =====================================================================================================================

LIBASSERT_BEGIN_NAMESPACE
    // returns the width of the terminal represented by fd, will be 0 on error
    [[nodiscard]] LIBASSERT_EXPORT int terminal_width(int fd);

    // Enable virtual terminal processing on windows terminals
    LIBASSERT_EXPORT void enable_virtual_terminal_processing_if_needed();

    inline constexpr int stdin_fileno = 0;
    inline constexpr int stdout_fileno = 1;
    inline constexpr int stderr_fileno = 2;

    LIBASSERT_EXPORT bool isatty(int fd);

    LIBASSERT_EXPORT bool is_debugger_present() noexcept;
    enum class debugger_check_mode {
        check_once,
        check_every_time,
    };
    LIBASSERT_EXPORT void set_debugger_check_mode(debugger_check_mode mode) noexcept;

    // returns the type name of T
    template<typename T>
    [[nodiscard]] std::string_view type_name() noexcept {
        return detail::type_name<T>();
    }

    // returns the prettified type name for T
    template<typename T> // TODO: Use this above....
    [[nodiscard]] std::string pretty_type_name() noexcept {
        return detail::prettify_type(std::string(detail::type_name<T>()));
    }

    // returns a debug stringification of t
    template<typename T>
    [[nodiscard]] std::string stringify(const T& t) {
        return detail::generate_stringification(t);
    }

    // NOTE: string view underlying data should have static storage duration, or otherwise live as long as the scheme
    // is in use
    struct color_scheme {
        std::string_view string;
        std::string_view escape;
        std::string_view keyword;
        std::string_view named_literal;
        std::string_view number;
        std::string_view punctuation;
        std::string_view operator_token;
        std::string_view call_identifier;
        std::string_view scope_resolution_identifier;
        std::string_view identifier;
        std::string_view accent;
        std::string_view unknown;
        std::string_view highlight_delete;
        std::string_view highlight_insert;
        std::string_view highlight_replace;
        std::string_view reset;
        LIBASSERT_EXPORT const static color_scheme ansi_basic;
        LIBASSERT_EXPORT const static color_scheme ansi_rgb;
        LIBASSERT_EXPORT const static color_scheme blank;
    };

    LIBASSERT_EXPORT void set_color_scheme(const color_scheme&);
    LIBASSERT_EXPORT const color_scheme& get_color_scheme();

    LIBASSERT_EXPORT void set_diff_highlighting(bool);

    // set separator used for diagnostics, by default it is "=>"
    // note: not thread-safe
    LIBASSERT_EXPORT void set_separator(std::string_view separator);

    std::string highlight(std::string_view expression, const color_scheme& scheme = get_color_scheme());

    template<typename T>
    [[nodiscard]] std::string highlight_stringify(const T& t, const color_scheme& scheme = get_color_scheme()) {
        return highlight(stringify(t), scheme);
    }

    enum class literal_format : unsigned {
        // integers and floats are decimal by default, chars are of course chars, and everything else only has one
        // format that makes sense
        default_format = 0,
        integer_hex = 1,
        integer_octal = 2,
        integer_binary = 4,
        integer_character = 8, // format integers as characters and characters as integers
        float_hex = 16,
    };

    [[nodiscard]] constexpr literal_format operator|(literal_format a, literal_format b) {
        return static_cast<literal_format>(
            static_cast<std::underlying_type<literal_format>::type>(a) |
            static_cast<std::underlying_type<literal_format>::type>(b)
        );
    }

    enum class literal_format_mode {
        infer, // infer literal formats based on the assertion condition
        no_variations, // don't do any literal format variations, just default
        fixed_variations // use a fixed set of formats always; note the default format will always be used
    };

    // NOTE: Should not be called during handling of an assertion in the current thread
    LIBASSERT_EXPORT void set_literal_format_mode(literal_format_mode);

    // NOTE: Should not be called during handling of an assertion in the current thread
    // set a fixed literal format configuration, automatically changes the literal_format_mode; note that the default
    // format will always be used along with others
    LIBASSERT_EXPORT void set_fixed_literal_format(literal_format);

    enum class path_mode {
        // full path is used
        full,
        // only enough folders needed to disambiguate are provided
        disambiguated, // TODO: Maybe just a bad idea...?
        // only the file name is used
        basename,
    };
    LIBASSERT_EXPORT void set_path_mode(path_mode mode);
    LIBASSERT_EXPORT path_mode get_path_mode();

    // generates a stack trace, formats to the given width
    [[nodiscard]] LIBASSERT_ATTR_NOINLINE LIBASSERT_EXPORT
    std::string stacktrace(int width = 0, const color_scheme& scheme = get_color_scheme(), std::size_t skip = 0);

    // formats a stacktrace
    [[nodiscard]] LIBASSERT_EXPORT
    std::string print_stacktrace(
        const cpptrace::stacktrace& trace,
        int width = 0,
        const color_scheme& scheme = get_color_scheme(),
        path_mode = get_path_mode()
    );

    enum class assert_type {
        debug_assertion,
        assertion,
        assumption,
        panic,
        unreachable
    };

    struct assertion_info;

    [[noreturn]] LIBASSERT_EXPORT void default_failure_handler(const assertion_info& info);

    using handler_ptr = void(*)(const assertion_info&);
    LIBASSERT_EXPORT handler_ptr get_failure_handler();
    LIBASSERT_EXPORT void set_failure_handler(handler_ptr handler);

    struct LIBASSERT_EXPORT binary_diagnostics_descriptor {
        std::string left_expression;
        std::string right_expression;
        std::string left_stringification;
        std::string right_stringification;
        bool multiple_formats;
        binary_diagnostics_descriptor(); // = default; in the .cpp
        binary_diagnostics_descriptor(
            std::string_view left_expression,
            std::string_view right_expression,
            std::string&& left_stringification,
            std::string&& right_stringification,
            bool multiple_formats
        );
        ~binary_diagnostics_descriptor(); // = default; in the .cpp
        binary_diagnostics_descriptor(const binary_diagnostics_descriptor&);
        binary_diagnostics_descriptor(binary_diagnostics_descriptor&&) noexcept;
        binary_diagnostics_descriptor& operator=(const binary_diagnostics_descriptor&);
        binary_diagnostics_descriptor& operator=(binary_diagnostics_descriptor&&) noexcept(LIBASSERT_GCC_ISNT_STUPID);
    };

    namespace detail {
        struct sv_span {
            const std::string_view* data;
            std::size_t size;
        };

        // collection of assertion data that can be put in static storage and all passed by a single pointer
        struct LIBASSERT_EXPORT assert_static_parameters {
            std::string_view macro_name;
            assert_type type;
            std::string_view expr_str;
            source_location location;
            sv_span args_strings;
        };
    }

    struct extra_diagnostic {
        std::string_view expression;
        std::string stringification;
    };

    namespace detail {
        class path_handler {
        public:
            virtual ~path_handler() = default;
            virtual std::unique_ptr<detail::path_handler> clone() const = 0;
            virtual std::string_view resolve_path(std::string_view) = 0;
            virtual bool has_add_path() const;
            virtual void add_path(std::string_view);
            virtual void finalize();
        };
        struct trace_holder;
        // deleter needed so unique ptr move/delete can work on the opaque pointer
        struct LIBASSERT_EXPORT trace_holder_deleter {
            void operator()(trace_holder*);
        };
        LIBASSERT_ATTR_NOINLINE LIBASSERT_EXPORT std::unique_ptr<trace_holder, trace_holder_deleter> generate_trace();
    }

    struct LIBASSERT_EXPORT assertion_info {
        std::string_view macro_name;
        assert_type type;
        std::string_view expression_string;
        std::string_view file_name;
        std::uint32_t line;
        std::string_view function;
        std::optional<std::string> message;
        std::optional<binary_diagnostics_descriptor> binary_diagnostics;
        std::vector<extra_diagnostic> extra_diagnostics;
        size_t n_args;
    private:
        std::unique_ptr<detail::trace_holder> trace;
        mutable std::unique_ptr<detail::path_handler> path_handler;
        detail::path_handler* get_path_handler() const; // will get and setup the path handler
    public:
        assertion_info() = delete;
        assertion_info(
            const detail::assert_static_parameters* static_params,
            std::unique_ptr<detail::trace_holder, detail::trace_holder_deleter> trace,
            size_t n_args
        );
        ~assertion_info();
        assertion_info(const assertion_info&);
        assertion_info(assertion_info&&);
        assertion_info& operator=(const assertion_info&);
        assertion_info& operator=(assertion_info&&);

        std::string_view action() const;

        const cpptrace::raw_trace& get_raw_trace() const;
        const cpptrace::stacktrace& get_stacktrace() const;

        [[nodiscard]] std::string header(int width = 0, const color_scheme& scheme = get_color_scheme()) const;
        [[nodiscard]] std::string tagline(const color_scheme& scheme = get_color_scheme()) const;
        [[nodiscard]] std::string location() const;
        [[nodiscard]] std::string statement(const color_scheme& scheme = get_color_scheme()) const;
        [[nodiscard]] std::string print_binary_diagnostics(int width = 0, const color_scheme& scheme = get_color_scheme()) const;
        [[nodiscard]] std::string print_extra_diagnostics(int width = 0, const color_scheme& scheme = get_color_scheme()) const;
        [[nodiscard]] std::string print_stacktrace(int width = 0, const color_scheme& scheme = get_color_scheme()) const;

        [[nodiscard]] std::string to_string(int width = 0, const color_scheme& scheme = get_color_scheme()) const;
    };
LIBASSERT_END_NAMESPACE

// =====================================================================================================================
// || Library core                                                                                                    ||
// =====================================================================================================================

LIBASSERT_BEGIN_NAMESPACE
namespace detail {
    /*
     * C++ syntax analysis and literal formatting
     */

    // get current literal_format configuration for the thread
    [[nodiscard]] LIBASSERT_EXPORT literal_format get_thread_current_literal_format();

    // sets the current literal_format configuration for the thread
    LIBASSERT_EXPORT void set_thread_current_literal_format(literal_format format);

    LIBASSERT_EXPORT literal_format set_literal_format(
        std::string_view left_expression,
        std::string_view right_expression,
        std::string_view op,
        bool integer_character
    );
    LIBASSERT_EXPORT void restore_literal_format(literal_format);
    // does the current literal format config have multiple formats
    LIBASSERT_EXPORT bool has_multiple_formats();

    [[nodiscard]] LIBASSERT_EXPORT std::pair<std::string, std::string> decompose_expression(
        std::string_view expression,
        std::string_view target_op
    );

    /*
     * assert diagnostics generation
     */

    template<typename A, typename B>
    LIBASSERT_ATTR_COLD [[nodiscard]]
    binary_diagnostics_descriptor generate_binary_diagnostic(
        const A& left,
        const B& right,
        std::string_view left_str,
        std::string_view right_str,
        std::string_view op
    ) {
        constexpr bool either_is_character = isa<A, char> || isa<B, char>;
        constexpr bool either_is_arithmetic = is_arith_not_bool_char<A> || is_arith_not_bool_char<B>;
        literal_format previous_format = set_literal_format(
            left_str,
            right_str,
            op,
            either_is_character && either_is_arithmetic
        );
        binary_diagnostics_descriptor descriptor(
            left_str,
            right_str,
            generate_stringification(left),
            generate_stringification(right),
            has_multiple_formats()
        );
        restore_literal_format(previous_format);
        return descriptor;
    }

    struct pretty_function_name_wrapper {
        const char* pretty_function;
    };

    inline void process_arg( // TODO: Don't inline
        assertion_info& info,
        size_t,
        sv_span,
        const pretty_function_name_wrapper& t
    ) {
        info.function = t.pretty_function;
    }

    LIBASSERT_EXPORT void set_message(assertion_info& info, const char* value);
    LIBASSERT_EXPORT void set_message(assertion_info& info, std::string_view value);
    // used to enable errno stuff
    LIBASSERT_EXPORT extra_diagnostic make_extra_diagnostic(std::string_view expression, int value);

    template<typename T>
    LIBASSERT_ATTR_COLD
    // TODO
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void process_arg(assertion_info& info, size_t i, sv_span args_strings, const T& t) {
        if constexpr(is_string_type<T>) {
            if(i == 0) {
                set_message(info, t);
                return;
            }
        }
        if constexpr(isa<T, int>) {
            info.extra_diagnostics.push_back(make_extra_diagnostic(args_strings.data[i], t));
        } else {
            info.extra_diagnostics.push_back({ args_strings.data[i], generate_stringification(t) });
        }
    }

    template<typename... Args>
    LIBASSERT_ATTR_COLD
    void process_args(assertion_info& info, sv_span args_strings, Args&... args) {
        size_t i = 0;
        (process_arg(info, i++, args_strings, args), ...);
        (void)args_strings;
    }
}
LIBASSERT_END_NAMESPACE

/*
 * Actual top-level assertion processing
 */

 LIBASSERT_BEGIN_NAMESPACE
 namespace detail {
    LIBASSERT_EXPORT void fail(const assertion_info& info);

    template<typename A, typename B, typename C, typename... Args>
    LIBASSERT_ATTR_COLD LIBASSERT_ATTR_NOINLINE
    // TODO: Re-evaluate forwarding here.
    void process_assert_fail(
        expression_decomposer<A, B, C>& decomposer,
        const assert_static_parameters* params,
        // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
        Args&&... args
    ) {
        const size_t sizeof_extra_diagnostics = sizeof...(args) - 1; // - 1 for pretty function signature
        LIBASSERT_PRIMITIVE_DEBUG_ASSERT(sizeof...(args) <= params->args_strings.size);
        assertion_info info(params, detail::generate_trace(), sizeof_extra_diagnostics);
        // process_args fills in the message, extra_diagnostics, and pretty_function
        process_args(info, params->args_strings, args...);
        // generate binary diagnostics
        if constexpr(is_nothing<C>) {
            static_assert(is_nothing<B> && !is_nothing<A>);
            if constexpr(isa<A, bool>) {
                (void)decomposer; // suppress warning in msvc
            } else {
                info.binary_diagnostics = generate_binary_diagnostic(
                    decomposer.a,
                    true,
                    params->expr_str,
                    "true",
                    "=="
                );
            }
        } else {
            auto [left_expression, right_expression] = decompose_expression(params->expr_str, C::op_string);
            info.binary_diagnostics = generate_binary_diagnostic(
                decomposer.a,
                decomposer.b,
                left_expression,
                right_expression,
                C::op_string
            );
        }
        // send off
        fail(info);
    }

    template<typename... Args>
    LIBASSERT_ATTR_COLD [[noreturn]] LIBASSERT_ATTR_NOINLINE
    // TODO: Re-evaluate forwarding here.
    void process_panic(
        const assert_static_parameters* params,
        // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
        Args&&... args
    ) {
        const size_t sizeof_extra_diagnostics = sizeof...(args) - 1; // - 1 for pretty function signature
        LIBASSERT_PRIMITIVE_DEBUG_ASSERT(sizeof...(args) <= params->args_strings.size);
        assertion_info info(params, detail::generate_trace(), sizeof_extra_diagnostics);
        // process_args fills in the message, extra_diagnostics, and pretty_function
        process_args(info, params->args_strings, args...);
        // send off
        fail(info);
        LIBASSERT_PRIMITIVE_PANIC("PANIC/UNREACHABLE failure handler returned");
    }

    template<typename T>
    struct assert_value_wrapper {
        T value;
    };

    template<
        bool ret_lhs, bool value_is_lval_ref,
        typename T, typename A, typename B, typename C
    >
    constexpr auto get_expression_return_value(T& value, expression_decomposer<A, B, C>& decomposer) {
        if constexpr(ret_lhs) {
            if constexpr(std::is_lvalue_reference_v<A>) {
                return assert_value_wrapper<A>{decomposer.take_lhs()};
            } else {
                return assert_value_wrapper<A>{std::move(decomposer.take_lhs())};
            }
        } else {
            if constexpr(value_is_lval_ref) {
                return assert_value_wrapper<T&>{value};
            } else {
                return assert_value_wrapper<T>{std::move(value)};
            }
        }
    }
}
LIBASSERT_END_NAMESPACE

#if LIBASSERT_IS_MSVC
 #pragma warning(pop)
#endif

#endif
