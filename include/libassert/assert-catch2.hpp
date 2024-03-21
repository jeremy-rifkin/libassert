#ifndef ASSERT_CATCH2_HPP
#define ASSERT_CATCH2_HPP

#define LIBASSERT_PREFIX_ASSERTIONS
#include <libassert/assert.hpp>

#include <catch2/catch_test_macros.hpp>

#if defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL != 0
 #error "Libassert integration does not work with MSVC's non-conformant preprocessor. /Zc:preprocessor must be used."
#endif
// TODO: CHECK/REQUIRE?
#define ASSERT(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); SUCCEED(); } catch(std::exception& e) { FAIL(e.what()); } } while(false)

namespace libassert::detail {
    inline void catch2_failure_handler(const assertion_info& info) {
        std::string message = std::string(info.action()) + " at " + info.location() + ":";
        if(info.message) {
            message += " " + *info.message;
        }
        message += "\n";
        message += info.statement(color_scheme::blank)
                + info.print_binary_diagnostics(0, color_scheme::blank)
                + info.print_extra_diagnostics(0, color_scheme::blank);
        // catch line wrapping has issues with ansi sequences https://github.com/catchorg/Catch2/issues/2833
        throw std::runtime_error(std::move(message));
    }

    inline auto pre_main = [] () {
        set_failure_handler(catch2_failure_handler);
        return 1;
    } ();
}

#endif
