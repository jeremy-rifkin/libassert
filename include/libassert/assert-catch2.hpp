#ifndef LIBASSERT_CATCH2_HPP
#define LIBASSERT_CATCH2_HPP

#define LIBASSERT_PREFIX_ASSERTIONS
#include <libassert/assert.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_version_macros.hpp>

#if defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL != 0
 #error "Libassert integration does not work with MSVC's non-conformant preprocessor. /Zc:preprocessor must be used."
#endif
// TODO: CHECK/REQUIRE?
#define ASSERT(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); SUCCEED(); } catch(std::exception& e) { FAIL(e.what()); } } while(false)

namespace libassert::detail {
    // catch line wrapping can't handle ansi sequences before 3.6 https://github.com/catchorg/Catch2/issues/2833
    inline constexpr bool use_color = CATCH_VERSION_MAJOR > 3 || (CATCH_VERSION_MAJOR == 3 && CATCH_VERSION_MINOR >= 6);

    inline void catch2_failure_handler(const assertion_info& info) {
        if(use_color) {
            enable_virtual_terminal_processing_if_needed();
        }
        auto scheme = use_color ? color_scheme::ansi_rgb : color_scheme::blank;
        std::string message = std::string(info.action()) + " at " + info.location() + ":";
        if(info.message) {
            message += " " + *info.message;
        }
        message += "\n";
        message += info.statement(scheme)
                + info.print_binary_diagnostics(CATCH_CONFIG_CONSOLE_WIDTH, scheme)
                + info.print_extra_diagnostics(CATCH_CONFIG_CONSOLE_WIDTH, scheme);
        throw std::runtime_error(std::move(message));
    }

    inline auto pre_main = [] () {
        set_failure_handler(catch2_failure_handler);
        return 1;
    } ();
}

#endif
