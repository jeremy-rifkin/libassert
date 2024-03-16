#include <gtest/gtest.h>

#define LIBASSERT_PREFIX_ASSERTIONS
#include <libassert/assert.hpp>

#include "tokenizer.hpp"

using namespace libassert::detail;

#if defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL != 0
 #error "Libassert integration does not work with MSVC's non-conformant preprocessor. /Zc:preprocessor must be used."
#endif
#define ASSERT(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); ASSERT_TRUE(true); } catch(std::exception& e) { ASSERT_TRUE(false) << e.what(); } } while(false)
#define EXPECT(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); ASSERT_TRUE(true); } catch(std::exception& e) { EXPECT_TRUE(false) << e.what(); } } while(false)

void failure_handler(const libassert::assertion_info& info) {
    std::string message = "";
    throw std::runtime_error(info.header());
}

auto pre_main = [] () {
    libassert::set_failure_handler(failure_handler);
    return 1;
} ();

template<> struct libassert::stringifier<token_t> {
    std::string stringify(const token_t& token) {
        return libassert::stringify(token.type) + " \"" + std::string(token.str) + "\"";
    }
};

template<typename T>
void check_vector(const std::optional<std::vector<T>>& output_opt, const std::optional<std::vector<T>>& expected_opt) {
    if(output_opt.has_value() && expected_opt.has_value()) {
        const auto& [output, expected] = std::make_pair(*output_opt, *expected_opt);
        for(std::size_t i = 0; i < std::max(output.size(), expected.size()); i++) {
            if(i >= output.size()) {
                ASSERT(false, "expected item not in vector", expected[i], i, expected, output);
            } else if(i >= expected.size()) {
                ASSERT(false, "unexpected item in vector", output[i], i, expected, output);
            } else {
                ASSERT(output[i] == expected[i], i, expected, output);
            }
        }
    } else {
        ASSERT(output_opt.has_value() == expected_opt.has_value(), output_opt, expected_opt);
    }
}

template<typename T>
void check_vector(const std::optional<std::vector<T>>& output, const std::vector<T>& expected) {
    check_vector(output, std::optional{expected});
}

template<typename T>
void check_vector(const std::optional<std::vector<T>>& output, std::nullopt_t expected) {
    check_vector<T>(output, std::optional<std::vector<T>>{});
}

TEST(LexerTests, Operators) {
    std::array punctuators = to_array<std::string_view>({
        "{",        "}",        "[",        "]",        "(",        ")",
        "<:",       ":>",       "<%",       "%>",       ";",        ":",        "...",
        "?",        "::",       ".",        ".*",       "->",       "->*",      "~",
        "!",        "+",        "-",        "*",        "/",        "%",        "^",        "&",        "|",
        "=",        "+=",       "-=",       "*=",       "/=",       "%=",       "^=",       "&=",       "|=",
        "==",       "!=",       "<",        ">",        "<=",       ">=",       "<=>",      "&&",       "||",
        "<<",       ">>",       "<<=",      ">>=",      "++",       "--",       ",",
        "and",      "or",       "xor",      "not",      "bitand",   "bitor",    "compl",
        "and_eq",   "or_eq",    "xor_eq",   "not_eq",
    });
    std::string str = join(punctuators, " ");
    auto vec = tokenize(str);
    std::vector<token_t> expected;
    for(auto& punctuator : punctuators) {
        expected.push_back(token_t(token_e::punctuation, punctuator));
        if(&punctuator - &*punctuators.begin() != punctuators.size() - 1) {
            expected.push_back(token_t(token_e::whitespace, " "));
        }
    }
    check_vector(vec, expected);
}

TEST(LexerTests, OperatorsNoSpaces) {
    std::array punctuators = to_array<std::string_view>({
        "{",        "}",        "[",        "]",        "(",        ")",
        "<:",       ":>",       "<%",       "%>",       ";",        ":",        "...",
        "?",        "::",       ".",        ".*",       "->",       "->*",      "~",
        "!",        "+",        "-",        "*",        "/",        "%",        "^",        "&",        "|",
        "~", // to prevent |=
        "=",        "+=",       "-=",       "*=",       "/=",       "%=",       "^=",       "&=",       "|=",
        "==",       "!=",       "<",        ">",        "<=", "~",  ">=",       "<=>",      "&&",       "||",
        "<<",       ">>",       "<<=",      ">>=",      "++",       "--",       ","
    });
    std::string str = join(punctuators, "");
    auto vec = tokenize(str);
    std::vector<token_t> expected;
    for(auto& punctuator : punctuators) {
        expected.push_back(token_t(token_e::punctuation, punctuator));
    }
    check_vector(vec, expected);
}

TEST(LexerTests, AlternativeOperators) {
    std::array punctuators = to_array<std::string_view>({
        "and",      "or",       "xor",      "not",      "bitand",   "bitor",    "compl",
        "and_eq",   "or_eq",    "xor_eq",   "not_eq",
    });
    auto id = join(punctuators, "");
    std::string str = id + " and<";
    auto vec = tokenize(str);
    std::vector<token_t> expected = {
        token_t(token_e::identifier, id),
        token_t(token_e::whitespace, " "),
        token_t(token_e::punctuation, "and"),
        token_t(token_e::punctuation, "<"),
    };
    check_vector(vec, expected);
}

TEST(LexerTests, AlternativeTokenEdgeCase) {
    std::string str = "<:<::std>:>";
    auto vec = tokenize(str);
    std::vector<token_t> expected = {
        token_t(token_e::punctuation, "<:"),
        token_t(token_e::punctuation, "<"),
        token_t(token_e::punctuation, "::"),
        token_t(token_e::identifier, "std"),
        token_t(token_e::punctuation, ">"),
        token_t(token_e::punctuation, ":>"),
    };
    check_vector(vec, expected);
}

TEST(LexerTests, ShrDecomposition) {
    std::string str = "1 >> 2";
    auto vec = tokenize(str, true);
    std::vector<token_t> expected = {
        token_t(token_e::number, "1"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::punctuation, ">"),
        token_t(token_e::punctuation, ">"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "2")
    };
    check_vector(vec, expected);
}

TEST(LexerTests, SingleLineComments) {
    std::string str = "foobar // 123";
    auto vec = tokenize(str);
    std::vector<token_t> expected = {
        token_t(token_e::identifier, "foobar"),
        token_t(token_e::whitespace, " ")
    };
    check_vector(vec, expected);
}

TEST(LexerTests, MultiLineComments) {
    std::string str = "1 /* foobar */ 2";
    auto vec = tokenize(str);
    std::vector<token_t> expected = {
        token_t(token_e::number, "1"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::whitespace, " "), // TODO: This might be sub-ideal
        token_t(token_e::number, "2")
    };
    check_vector(vec, expected);
}

TEST(LexerTests, NamedLiterals) {
    std::string str = "false true nullptr falsetrue false1 nullptr-";
    auto vec = tokenize(str);
    std::vector<token_t> expected = {
        token_t(token_e::named_literal, "false"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::named_literal, "true"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::named_literal, "nullptr"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::identifier, "falsetrue"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::identifier, "false1"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::named_literal, "nullptr"),
        token_t(token_e::punctuation, "-"),
    };
    check_vector(vec, expected);
}

TEST(LexerTests, CharLiterals) {
    std::string str = R"('f''f'12 '\n' u8'\u1212''\N{foo'bar}' L'W''\'')";
    auto vec = tokenize(str);
    std::vector<token_t> expected = {
        token_t(token_e::string, "'f'"),
        token_t(token_e::string, "'f'"),
        token_t(token_e::number, "12"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::string, "'\\n'"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::string, "u8'\\u1212'"),
        token_t(token_e::string, "'\\N{foo'bar}'"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::string, "L'W'"),
        token_t(token_e::string, "'\\''"),
    };
    check_vector(vec, expected);
    check_vector(tokenize("'foo'"), std::nullopt);
}

TEST(LexerTests, StringLiterals) {
    std::string str = R"("f""f"12 "\n" u8"\u1212""foobar""\N{foo"bar}" L"W""foo\"foo")";
    auto vec = tokenize(str);
    std::vector<token_t> expected = {
        token_t(token_e::string, "\"f\""),
        token_t(token_e::string, "\"f\""),
        token_t(token_e::number, "12"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::string, "\"\\n\""),
        token_t(token_e::whitespace, " "),
        token_t(token_e::string, "u8\"\\u1212\""),
        token_t(token_e::string, "\"foobar\""),
        token_t(token_e::string, "\"\\N{foo\"bar}\""),
        token_t(token_e::whitespace, " "),
        token_t(token_e::string, "L\"W\""),
        token_t(token_e::string, "\"foo\\\"foo\""),
    };
    check_vector(vec, expected);
}

TEST(LexerTests, Numbers) {
    std::string str = R"TT(100 20 066 080 0x4fefe 0b101001010 .12 1. 1.f .12f 1e1 1e+2 1.e-2 0x1.1p+10)TT";
    auto vec = tokenize(str);
    std::vector<token_t> expected = {
        token_t(token_e::number, "100"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "20"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "066"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "080"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "0x4fefe"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "0b101001010"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, ".12"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "1."),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "1.f"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, ".12f"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "1e1"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "1e+2"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "1.e-2"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "0x1.1p+10"),
    };
    check_vector(vec, expected);
}

TEST(LexerTests, UDLs) {
    std::string str = R"TT('1'sv "12"sv 20uint 1._f 0x1.1p+10_foo 1+foo)TT";
    auto vec = tokenize(str);
    std::vector<token_t> expected = {
        token_t(token_e::string, "'1'sv"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::string, "\"12\"sv"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "20uint"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "1._f"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "0x1.1p+10_foo"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::number, "1"),
        token_t(token_e::punctuation, "+"),
        token_t(token_e::identifier, "foo"),
    };
    check_vector(vec, expected);
}

TEST(LexerTests, IdentifiersAndKeywords) {
    std::string str = R"TT(12f f12 foo_bar200.0 break for() foo() this.foo this->foo char)TT";
    auto vec = tokenize(str);
    std::vector<token_t> expected = {
        token_t(token_e::number, "12f"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::identifier, "f12"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::identifier, "foo_bar200"),
        token_t(token_e::number, ".0"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::keyword, "break"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::keyword, "for"),
        token_t(token_e::punctuation, "("),
        token_t(token_e::punctuation, ")"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::identifier, "foo"),
        token_t(token_e::punctuation, "("),
        token_t(token_e::punctuation, ")"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::keyword, "this"),
        token_t(token_e::punctuation, "."),
        token_t(token_e::identifier, "foo"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::keyword, "this"),
        token_t(token_e::punctuation, "->"),
        token_t(token_e::identifier, "foo"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::keyword, "char"),
    };
    check_vector(vec, expected);
}

TEST(LexerTests, InvalidInputIsRejected) {
    auto vec1 = tokenize(R"TT(Error: Didn't return the correct result)TT");
    check_vector(vec1, std::nullopt);
    auto vec2 = tokenize(R"TT(Error: Didn't return the correct result, or didn't return the right result)TT");
    check_vector(vec2, std::nullopt);
}

TEST(LexerTests, Regression1) {
    auto vec = tokenize(R"TT(std::optional<std::vector<token_t>>: nullopt)TT");
    std::vector<token_t> expected = {
        token_t(token_e::identifier, "std"),
        token_t(token_e::punctuation, "::"),
        token_t(token_e::identifier, "optional"),
        token_t(token_e::punctuation, "<"),
        token_t(token_e::identifier, "std"),
        token_t(token_e::punctuation, "::"),
        token_t(token_e::identifier, "vector"),
        token_t(token_e::punctuation, "<"),
        token_t(token_e::identifier, "token_t"),
        token_t(token_e::punctuation, ">>"),
        token_t(token_e::punctuation, ":"),
        token_t(token_e::whitespace, " "),
        token_t(token_e::identifier, "nullopt"),
    };
    check_vector(vec, expected);
}
