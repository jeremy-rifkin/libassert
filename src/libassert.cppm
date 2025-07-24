module;
#include <libassert/assert.hpp>

export module libassert;

LIBASSERT_BEGIN_NAMESPACE
    // assert.hpp
    export using libassert::terminal_width;
    export using libassert::enable_virtual_terminal_processing_if_needed;
    export using libassert::stdin_fileno;
    export using libassert::stdout_fileno;
    export using libassert::stderr_fileno;
    export using libassert::isatty;
    export using libassert::is_debugger_present;
    export using libassert::debugger_check_mode;
    export using libassert::set_debugger_check_mode;
    export using libassert::type_name;
    export using libassert::pretty_type_name;
    export using libassert::stringify;
    export using libassert::color_scheme;
    export using libassert::set_color_scheme;
    export using libassert::get_color_scheme;
    export using libassert::set_diff_highlighting;
    export using libassert::set_stacktrace_callback;
    export using libassert::set_separator;
    export using libassert::highlight;
    export using libassert::highlight_stringify;
    export using libassert::stacktrace;
    export using libassert::literal_format;
    export using libassert::operator|;
    export using libassert::literal_format_mode;
    export using libassert::set_literal_format_mode;
    export using libassert::set_fixed_literal_format;
    export using libassert::path_mode;
    export using libassert::set_path_mode;
    export using libassert::assert_type;
    export using libassert::default_failure_handler;
    export using libassert::handler_ptr;
    export using libassert::get_failure_handler;
    export using libassert::set_failure_handler;
    export using libassert::binary_diagnostics_descriptor;
    export using libassert::extra_diagnostic;
    export using libassert::assertion_info;
    namespace detail {
        export using libassert::detail::set_literal_format;
        export using libassert::detail::assert_static_parameters;
        export using libassert::detail::pretty_function_name_wrapper;
        export using libassert::detail::process_assert_fail;
        export using libassert::detail::process_panic;
        export using libassert::detail::get_expression_return_value;
    }
    // expression-decomposition.hpp
    namespace detail {
        export using libassert::detail::expression_decomposer;
    }
    // stringification.hpp
    export using libassert::stringifier;
    namespace detail {
        export using libassert::detail::generate_stringification;
        export using libassert::detail::stringifiable;
        export using libassert::detail::stringifiable_container;
    }
    // utilities.hpp
    export using libassert::source_location;
    namespace detail {
        export using libassert::detail::prettify_type;
    }
LIBASSERT_END_NAMESPACE
