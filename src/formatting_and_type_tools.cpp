#include <cstdarg>
#include <cstdio>

#include <libassert/assert.hpp>

namespace libassert::detail {
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

    LIBASSERT_ATTR_COLD [[nodiscard]]
    constexpr std::string_view substring_bounded_by(
        std::string_view sig,
        std::string_view l,
        std::string_view r
    ) noexcept {
        auto i = sig.find(l) + l.length();
        return sig.substr(i, sig.rfind(r) - i);
    }

    LIBASSERT_EXPORT std::string_view type_name_core(std::string_view signature) noexcept {
        // Cases to handle:
        // gcc:   constexpr std::string_view ns::type_name() [with T = int; std::string_view = std::basic_string_view<char>]
        // clang: std::string_view ns::type_name() [T = int]
        // msvc:  class std::basic_string_view<char,struct std::char_traits<char> > __cdecl ns::type_name<int>(void)
        #if LIBASSERT_IS_CLANG
         return substring_bounded_by(signature, "[T = ", "]");
        #elif LIBASSERT_IS_GCC
         return substring_bounded_by(signature, "[with T = ", "; std::string_view = ");
        #elif LIBASSERT_IS_MSVC
         return substring_bounded_by(signature, "type_name<", ">(void)");
        #else
         static_assert(false, "unsupported compiler");
        #endif
    }
}


