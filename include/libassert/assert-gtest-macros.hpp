#ifndef LIBASSERT_GTEST_MACROS_HPP
#define LIBASSERT_GTEST_MACROS_HPP

#define LIBASSERT_PREFIX_ASSERTIONS
#include <libassert/assert-macros.hpp>

#include <stdexcept>

#if defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL
 #error "Libassert integration does not work with MSVC's non-conformant preprocessor. /Zc:preprocessor must be used."
#endif
#define ASSERT(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); SUCCEED(); } catch(std::exception& e) { FAIL() << e.what(); } } while(0)
#define EXPECT(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); SUCCEED(); } catch(std::exception& e) { ADD_FAILURE() << e.what(); } } while(0)

LIBASSERT_BEGIN_NAMESPACE
namespace detail {
    inline void gtest_failure_handler(const assertion_info& info) {
        enable_virtual_terminal_processing_if_needed(); // for terminal colors on windows
        auto width = terminal_width(stderr_fileno);
        const auto& scheme = isatty(stderr_fileno) ? get_color_scheme() : color_scheme::blank;
        std::string message = std::string(info.action()) + " at " + info.location() + ":";
        if(info.message) {
            message += " " + *info.message;
        }
        message += "\n";
        message += info.statement()
                + info.print_binary_diagnostics(width, scheme)
                + info.print_extra_diagnostics(width, scheme);
        throw std::runtime_error(std::move(message));
    }

    inline auto pre_main = [] () {
        set_failure_handler(gtest_failure_handler);
        return 1;
    } ();
}
LIBASSERT_END_NAMESPACE

#endif
