#include <libassert/assert-gtest.hpp>
#include <format>
#include <fmt/format.h>

struct S {
    int x;
};

template<>
struct std::formatter<S> {
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const S& s, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "s.x={}", s.x);
    }
};

TEST(LibassertFmt, StdFormatWorks) {
    ASSERT(libassert::stringify(S{42}) == "s.x=42");
}

struct S2 {
    int x;
};

template<>
struct std::formatter<S2> {
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const S2& s, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "s2.x={}", s.x);
    }
};

template<> struct libassert::stringifier<S2> {
    std::string stringify(const S2& s) {
        return std::format("{}", s) + "--";
    }
};

TEST(LibassertFmt, StdFormatPriority1) {
    ASSERT(libassert::stringify(S2{42}) == "s2.x=42--");
}

struct S3 {
    int x;
};

template<>
struct std::formatter<S3> {
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const S3& s, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "std::format chosen");
    }
};

template<>
struct fmt::formatter<S3> {
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const S3& s, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "fmt::format chosen");
    }
};

TEST(LibassertFmt, StdFormatPriority2) {
    ASSERT(libassert::stringify(S3{42}) == "std::format chosen");
}

struct fmtable {
    int x;
};

template <> struct std::formatter<fmtable>: formatter<string_view> {
    template<class FormatContext>
    auto format(const fmtable& f, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{{{}}}", f.x);
    }
};

static_assert(std::formattable<fmtable, char>);

TEST(LibassertFmt, StdFormatContainers) {
    fmtable f{2};
    ASSERT(libassert::detail::generate_stringification(f) == "{2}");

    std::vector<fmtable> fvec{{{2}, {3}}};
    ASSERT(libassert::detail::generate_stringification(fvec) == "std::vector<fmtable>: [{2}, {3}]");
}
