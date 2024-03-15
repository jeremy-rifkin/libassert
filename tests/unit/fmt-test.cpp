#include <libassert/assert-gtest.hpp>

#include <fmt/format.h>

struct S {
    int x;
};

template<>
struct fmt::formatter<S> {
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const S& s, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "s.x={}", s.x);
    }
};

TEST(LibassertFmt, FmtWorks) {
    ASSERT(libassert::stringify(S{42}) == "s.x=42");
}

struct S2 {
    int x;
};

template<>
struct fmt::formatter<S2> {
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const S2& s, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "s2.x={}", s.x);
    }
};

template<> struct libassert::stringifier<S2> {
    std::string stringify(const S2& s) {
        return fmt::format("{}", s) + "--";
    }
};

TEST(LibassertFmt, FmtPriority) {
    ASSERT(libassert::stringify(S2{42}) == "s2.x=42--");
}
