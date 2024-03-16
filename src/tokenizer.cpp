#include "tokenizer.hpp"

#include <array>
#include <cctype>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

#include "utils.hpp"

namespace libassert::detail {
    // http://eel.is/c++draft/lex.name#nt:identifier
    bool is_identifier_start(char c) {
        return isalpha(c) || c == '$' || c == '_';
    }
    bool is_identifier_continue(char c) {
        return isdigit(c) || is_identifier_start(c);
    }
    bool is_hex_digit(char c) {
        // codegen is ok https://godbolt.org/z/sEnGPszao
        return isdigit(c) || needle(c).is_in('a', 'b', 'c', 'd', 'e', 'f', 'A', 'B', 'C', 'D', 'E', 'F');
    }
    bool is_octal_digit(char c) {
        // codegen is ok https://godbolt.org/z/sEnGPszao
        return needle(c).is_in('0', '1', '2', '3', '4', '5', '6', '7');
    }
    bool is_simple_escape_char(char c) {
        // codegen is ok https://godbolt.org/z/sEnGPszao
        return needle(c).is_in('\'', '"', '?', '\\', 'a', 'b', 'f', 'n', 'r', 't', 'v');
    }

    // http://eel.is/c++draft/lex.operators#nt:operator-or-punctuator

    constexpr std::array punctuators_and_operators = []() constexpr {
        std::array arr = to_array<std::string_view>({
            "{",        "}",        "[",        "]",        "(",        ")",
            "<:",       ":>",       "<%",       "%>",       ";",        ":",        "...",
            "?",        "::",       ".",        ".*",       "->",       "->*",      "~",
            "!",        "+",        "-",        "*",        "/",        "%",        "^",        "&",        "|",
            "=",        "+=",       "-=",       "*=",       "/=",       "%=",       "^=",       "&=",       "|=",
            "==",       "!=",       "<",        ">",        "<=",       ">=",       "<=>",      "&&",       "||",
            "<<",       ">>",       "<<=",      ">>=",      "++",       "--",       ",",
            // "and",      "or",       "xor",      "not",      "bitand",   "bitor",    "compl",
            // "and_eq",   "or_eq",    "xor_eq",   "not_eq",
        });
        constexpr_sort(arr, [](std::string_view a, std::string_view b) { return a.size() < b.size(); });
        return arr;
    } ();

    constexpr std::array alternative_operators = []() constexpr {
        std::array arr = to_array<std::string_view>({
            "and",      "or",       "xor",      "not",      "bitand",   "bitor",    "compl",
            "and_eq",   "or_eq",    "xor_eq",   "not_eq",
        });
        constexpr_sort(arr, [](std::string_view a, std::string_view b) { return a.size() < b.size(); });
        return arr;
    } ();

    struct lexer_error {
        // lexer_error() { throw std::runtime_error("oops"); }
    };

    #define TRY(expr) do if(std::optional<lexer_error> res = (expr); res.has_value()) { return res.value(); } while(0)

    // key#nt:keyword
    // [...temp0.querySelectorAll("span.keyword, span.literal")].map(node => `"${node.innerHTML.replace(`<span class="shy"></span>`, "")}",`).join("\n")
    std::unordered_set<std::string_view> keywords {
        "alignas",
        "constinit",
        // "false",
        "public",
        // "true",
        "alignof",
        "const_cast",
        "float",
        "register",
        "try",
        "asm",
        "continue",
        "for",
        "reinterpret_cast",
        "typedef",
        "auto",
        "co_await",
        "friend",
        "requires",
        "typeid",
        "bool",
        "co_return",
        "goto",
        "return",
        "typename",
        "break",
        "co_yield",
        "if",
        "short",
        "union",
        "case",
        "decltype",
        "inline",
        "signed",
        "unsigned",
        "catch",
        "default",
        "int",
        "sizeof",
        "using",
        "char",
        "delete",
        "long",
        "static",
        "virtual",
        "char8_t",
        "do",
        "mutable",
        "static_assert",
        "void",
        "char16_t",
        "double",
        "namespace",
        "static_cast",
        "volatile",
        "char32_t",
        "dynamic_cast",
        "new",
        "struct",
        "wchar_t",
        "class",
        "else",
        "noexcept",
        "switch",
        "while",
        "concept",
        "enum",
        // "nullptr",
        "template",
        "const",
        "explicit",
        "operator",
        "this",
        "consteval",
        "export",
        "private",
        "thread_local",
        "constexpr",
        "extern",
        "protected",
        "throw"
    };

    class tokenizer {
        std::string_view source;
        std::string_view::iterator it;
        bool error = false;
    public:
        tokenizer(std::string_view source_) : source(source_), it(source.begin()) {}

        std::variant<std::vector<token_t>, lexer_error> tokenize(bool decompose_shr = false) {
            std::vector<token_t> tokens;
            while(!end()) {
                // Blanks, horizontal and vertical tabs, newlines, formfeeds, and comments (collectively, “whitespace”), as
                // described below, are ignored except as they serve to separate tokens.
                // http://eel.is/c++draft/lex.token#1.sentence-2
                if(isspace(peek())) {
                    tokens.push_back(read_whitespace());
                } else if(peek("//")) { // comments can't show up here but may as well
                    TRY(read_comment());
                } else if(peek("/*")) {
                    TRY(read_multi_line_comment());
                }
                // -----------------------------------------------------------------------------------------------------
                // There are five kinds of tokens: identifiers, keywords, literals, operators, and other separators
                // http://eel.is/c++draft/lex.token#1.sentence-1
                // check in the following order:
                // 1. literals       must come before identifiers due to R"()" and similar, must come before punctuation
                //                   due to .1
                // 2. punctuators    must come before identifiers due to and/or/not/bitand/etc
                // 3. identifiers and keywords
                // -----------------------------------------------------------------------------------------------------
                // 1. literals
                //      integer-literal          all start with digits
                //      character-literal        a prefix (u8  u  U  L) followed by ''
                //      floating-point-literal   all start with digits
                //      string-literal           a prefix (u8  u  U  L) followed by "", or R"...(...)..."
                //      boolean-literal          true/false
                //      pointer-literal          nullptr
                //      user-defined-literal     integer/float/string/char literal followed by a ud-suffix
                else if(
                    auto named_literal = peek_any(to_array<std::string_view>({"false", "true", "nullptr"}));
                    named_literal && !is_identifier_continue(peek(named_literal->size()))
                ) {
                    TRY(advance(named_literal->size()));
                    tokens.push_back({token_e::named_literal, *named_literal});
                }
                else if( // char literals
                    auto prefix = peek_any(to_array<std::string_view>({"u8", "u", "U", "L"}));
                    peek(prefix.value_or("").size()) == '\''
                ) {
                    auto begin = pos();
                    TRY(advance(prefix.value_or("").size()));
                    TRY(read_char_literal());
                    auto end = pos();
                    tokens.push_back({token_e::string, std::string_view(source.data() + begin, end - begin)});
                }
                else if( // string literals
                    // reusing same prefix from last if
                    peek(prefix.value_or("").size()) == '"'
                        || (peek(prefix.value_or("").size()) == 'R' && peek(prefix.value_or("").size() + 1) == '"')
                ) {
                    auto begin = pos();
                    TRY(advance(prefix.value_or("").size()));
                    if(peek() == 'R') {
                        TRY(read_raw_string_literal());
                    } else {
                        TRY(read_string_literal());
                    }
                    auto end = pos();
                    tokens.push_back({token_e::string, std::string_view(source.data() + begin, end - begin)});
                }
                else if(isdigit(peek()) || (peek() == '.' && isdigit(peek(1)))) { // integer, float
                    auto begin = pos();
                    TRY(read_numeric_literal());
                    auto end = pos();
                    tokens.push_back({token_e::number, std::string_view(source.data() + begin, end - begin)});
                }
                // 2. punctuation
                //     handle normal punctuation and alternative operators separately
                else if(auto punctuator = peek_any(punctuators_and_operators)) {
                    TRY(advance(punctuator->size()));
                    // handle <:: edge case https://eel.is/c++draft/lex.pptoken#3.2
                    if(punctuator == "<:" && peek() == ':' && !needle(peek(1)).is_in(':', '>')) {
                        rollback(1);
                        tokens.push_back({token_e::punctuation, "<"});
                    }
                    // handle >> decomposition for templates
                    else if(decompose_shr && punctuator == ">>") {
                        tokens.push_back({token_e::punctuation, ">"});
                        tokens.push_back({token_e::punctuation, ">"});
                    } else {
                        tokens.push_back({token_e::punctuation, *punctuator});
                    }
                }
                else if(
                    auto alternative_operator = peek_any(alternative_operators);
                    alternative_operator && !is_identifier_continue(peek(alternative_operator->size()))
                ) {
                    TRY(advance(alternative_operator->size()));
                    tokens.push_back({token_e::punctuation, *alternative_operator});
                }
                // 3. identifiers
                else if(is_identifier_start(peek())) {
                    auto begin = pos();
                    TRY(read_identifier_or_keyword());
                    auto end = pos();
                    std::string_view contents = std::string_view(source.data() + begin, end - begin);
                    tokens.push_back({
                        keywords.find(contents) == keywords.end() ? token_e::identifier : token_e::keyword,
                        contents
                    });
                } else {
                    // we don't know....
                    tokens.push_back({token_e::unknown, std::string_view(source.data(), 1)});
                    TRY(advance());
                }
                // // universal character escapes like \U0001F60A get stringified so we have to handle them
                // // http://eel.is/c++draft/lex#charset-3
                // // fortunately we don't have to worry about unicode spellings of basic character set characters
                // // http://eel.is/c++draft/lex#charset-6.sentence-1
                // else if(peek() == '\\' && needle(peek(1)).is_in('u', 'U', 'N')) {
                //     // tokens.push_back(read_universal_character_name());
                // }
            }
            if(error) {
                return lexer_error{};
            } else {
                return tokens;
            }
        }
    private:
        token_t read_whitespace() {
            auto begin = pos();
            std::size_t count = 0;
            while(isspace(peek())) {
                internal_verify(advance() == std::nullopt);
                count++;
            }
            return {token_e::whitespace, std::string_view(source.data() + begin, count)};
        }

        [[nodiscard]] std::optional<lexer_error> read_comment() {
            while(!end() && peek() != '\n') {
                TRY(advance());
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<lexer_error> read_multi_line_comment() {
            while(!end() && !(peek() == '*' && peek(1) == '/')) {
                TRY(advance());
            }
            TRY(expect('*'));
            TRY(expect('/'));
            return std::nullopt;
        }

        [[nodiscard]] std::optional<lexer_error> read_identifier_or_keyword() {
            while(!end() && is_identifier_continue(peek())) {
                TRY(advance());
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<lexer_error> read_char_literal() {
            // http://eel.is/c++draft/lex.ccon
            TRY(expect('\''));
            // make sure this isn't an invalid char literal
            if(peek() == '\'') {
                return lexer_error{};
            }
            // read one character, or an escape sequence
            if(peek() == '\\') {
                TRY(read_escape_sequence());
            } else {
                TRY(advance());
            }
            // expect end of char literal
            TRY(expect('\''));
            TRY(read_optional_udl_suffix());
            return std::nullopt;
        }

        [[nodiscard]] std::optional<lexer_error> read_string_literal() {
            // string#nt:string-literal
            TRY(expect('"'));
            while(!end() && peek() != '"') {
                if(peek() == '\\') {
                    TRY(read_escape_sequence());
                } else {
                    TRY(advance());
                }
            }
            TRY(expect('"'));
            TRY(read_optional_udl_suffix());
            return std::nullopt;
        }

        [[nodiscard]] std::optional<lexer_error> read_raw_string_literal() {
            // string#nt:string-literal
            TRY(expect('R'));
            TRY(expect('"'));
            // read d-char sequence
            // we'll be much more permissive about what is allowed as a d-char, we'll allow anything that's not a (
            auto d_begin = pos();
            while(!end() && peek() != '(') {
                TRY(advance());
            }
            auto d_char_sequence = std::string_view(source.data() + d_begin, pos() - d_begin);
            // read r-char sequence
            while(!end()) {
                if(peek() == ')' && peek(d_char_sequence, 1) && peek(1 + d_char_sequence.size()) == '"') {
                    // end of string
                    TRY(advance(1 + d_char_sequence.size()));
                    break;
                } else {
                    TRY(advance());
                }
            }
            TRY(expect('"'));
            return std::nullopt;
        }

        [[nodiscard]] std::optional<lexer_error> read_escape_sequence() {
            // http://eel.is/c++draft/lex.ccon#nt:escape-sequence
            // escape-sequence:
            //   simple-escape-sequence
            //   numeric-escape-sequence
            //   conditional-escape-sequence
            TRY(expect('\\'));
            // the only escape sequences we really need to parse are those that could contain ' or "
            if(is_simple_escape_char(peek())) {
                TRY(advance());
            } else if(is_octal_digit(peek())) {
                TRY(advance()); // read the first
                if(is_octal_digit(peek())) { // read the second
                    TRY(advance());
                }
                if(is_octal_digit(peek())) { // read the third
                    TRY(advance());
                }
            } else if(peek() == 'o') {
                TRY(read_braced_sequence());
            } else if(peek() == 'x') {
                TRY(advance());
                if(peek() == '{') {
                    TRY(read_braced_sequence());
                } else if(is_hex_digit(peek())) {
                    while(!end() && is_hex_digit(peek())) {
                        TRY(advance());
                    }
                } else {
                    return lexer_error{};
                }
            } else {
                TRY(read_universal_character_name());
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<lexer_error> read_universal_character_name() {
            // http://eel.is/c++draft/lex.charset#nt:universal-character-name
            // universal-character-name:
            //   \u hex-quad
            //   \U hex-quad hex-quad
            //   \u{ simple-hexadecimal-digit-sequence }
            //   named-universal-character
            if(peek() == 'u') {
                TRY(advance());
                if(peek() == '{') {
                    TRY(read_braced_sequence());
                } else {
                    TRY(read_hex_quad());
                }
            } else if(peek() == 'U') {
                TRY(advance());
                TRY(read_hex_quad());
                TRY(read_hex_quad());
            } else if(peek() == 'N') {
                TRY(advance());
                TRY(read_braced_sequence());
            } else {
                return lexer_error{};
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<lexer_error> read_hex_quad() {
            // http://eel.is/c++draft/lex.charset#nt:hex-quad
            // hex-quad:
            //   hexadecimal-digit hexadecimal-digit hexadecimal-digit hexadecimal-digit
            for(int i = 0; i < 4; i++) {
                if(!is_hex_digit(peek())) {
                    return lexer_error{};
                } else {
                    TRY(advance());
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<lexer_error> read_numeric_literal() {
            // since we can assume a valid token, and we'll treat it as a pp-number....
            // http://eel.is/c++draft/lex.ppnumber
            // just read [0-9a-zA-Z'\.]+, essentially, with special handling for exponents/sign
            while(!end() && (isdigit(peek()) || isalpha(peek()) || peek() == '\'' ||  peek() == '.')) {
                if(needle(peek()).is_in('e', 'E', 'p', 'P') && needle(peek(1)).is_in('-', '+')) {
                    TRY(advance(2));
                } else {
                    TRY(advance());
                }
            }
            // non-underscore udls will already be handled above, catch remaining here
            TRY(read_optional_udl_suffix());
            return std::nullopt;
        }

        [[nodiscard]] std::optional<lexer_error> read_optional_udl_suffix() {
            // http://eel.is/c++draft/lex.ext#nt:ud-suffix
            if(is_identifier_start(peek())) {
                TRY(read_identifier_or_keyword());
            }
            return std::nullopt;
        }

        // reads a {...}
        // TODO: Allow a predicate to decide what is allowed between the braces
        [[nodiscard]] std::optional<lexer_error> read_braced_sequence() {
            TRY(expect('{'));
            while(!end() && peek() != '}') {
                TRY(advance());
            }
            TRY(expect('}'));
            return std::nullopt;
        }
    private:

        [[nodiscard]] std::size_t pos() const {
            return it - source.begin();
        }

        [[nodiscard]] char peek(std::size_t count = 0) const {
            auto pos = it - source.begin();
            if(pos + count < source.size()) {
                return *(it + count);
            } else {
                return 0;
            }
        }

        [[nodiscard]] bool peek(std::string_view pattern, std::size_t offset = 0) const {
            for(std::size_t i = 0; i < pattern.size(); i++) {
                if(peek(offset + i) != pattern[i]) {
                    return false;
                }
            }
            return true;
        }

        template<std::size_t N>
        [[nodiscard]]
        std::optional<std::string_view> peek_any(const std::array<std::string_view, N>& candidates) const {
            for(auto entry : candidates) {
                if(peek(entry)) {
                    return entry;
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<lexer_error> advance() {
            if(it != source.end()) {
                it++;
                return std::nullopt;
            } else {
                return lexer_error{};
            }
        }

        [[nodiscard]] std::optional<lexer_error> advance(std::size_t count) {
            while(it != source.end() && count > 0) {
                it++;
                count--;
            }
            if(count == 0) {
                return std::nullopt;
            } else {
                return lexer_error{};
            }
        }

        void rollback(std::size_t count) {
            while(count--) {
                internal_verify(it != source.begin(), "Tokenizer rollback() failed, please report this bug");
                it--;
            }
        }

        [[nodiscard]] std::optional<lexer_error> expect(char c) {
            if(peek() != c) {
                error = true;
                return lexer_error{};
            }
            TRY(advance());
            return std::nullopt;
        }

        // returns true if the end of the input has been reached or if tokenization should otherwise stop
        [[nodiscard]] bool end() const {
            return it == source.end() || error;
        }
    };

    std::optional<std::vector<token_t>> tokenize(std::string_view source, bool decompose_shr) {
        auto res = tokenizer(source).tokenize(decompose_shr);
        if(res.index() == 0) {
            return std::move(std::get<std::vector<token_t>>(res));
        } else {
            internal_verify(res.index() == 1);
            return std::nullopt;
        }
    }
}
