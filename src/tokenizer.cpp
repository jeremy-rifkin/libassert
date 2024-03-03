#include "tokenizer.hpp"

#include <array>
#include <cctype>
#include <string_view>
#include <vector>
#include <optional>
#include <unordered_set>

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
            "and",      "or",       "xor",      "not",      "bitand",   "bitor",    "compl",
            "and_eq",   "or_eq",    "xor_eq",   "not_eq",
        });
        constexpr_sort(arr, [](std::string_view a, std::string_view b) { return a.size() < b.size(); });
        return arr;
    } ();

    // constexpr std::array punctuators = []() constexpr {
    //     std::array arr = to_array<std::string_view>({
    //         "{",        "}",        "[",        "]",        "(",        ")",
    //         "<:",       ":>",       "<%",       "%>",       ";",        ":",        "...",
    //         "?",        "::",       ".",        ".*",       "->",       "->*",      ","
    //     });
    //     constexpr_sort(arr, [](std::string_view a, std::string_view b) { return a.size() < b.size(); });
    //     return arr;
    // } ();

    // constexpr std::array operators = []() constexpr {
    //     std::array arr = to_array<std::string_view>({
    //         "~",
    //         "!",        "+",        "-",        "*",        "/",        "%",        "^",        "&",        "|",
    //         "=",        "+=",       "-=",       "*=",       "/=",       "%=",       "^=",       "&=",       "|=",
    //         "==",       "!=",       "<",        ">",        "<=",       ">=",       "<=>",      "&&",       "||",
    //         "<<",       ">>",       "<<=",      ">>=",      "++",       "--",
    //         "and",      "or",       "xor",      "not",      "bitand",   "bitor",    "compl",
    //         "and_eq",   "or_eq",    "xor_eq",   "not_eq"
    //     });
    //     constexpr_sort(arr, [](std::string_view a, std::string_view b) { return a.size() < b.size(); });
    //     return arr;
    // } ();

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
    public:
        tokenizer(std::string_view source_) : source(source_), it(source.begin()) {}
        std::vector<token_t> tokenize(bool decompose_shr = false) {
            std::vector<token_t> tokens;
            while(!end()) {
                // Blanks, horizontal and vertical tabs, newlines, formfeeds, and comments (collectively, “whitespace”), as
                // described below, are ignored except as they serve to separate tokens.
                // http://eel.is/c++draft/lex.token#1.sentence-2
                if(isspace(peek())) {
                    tokens.push_back(read_whitespace());
                } else if(peek("//")) { // comments can't show up here but may as well
                    read_comment();
                } else if(peek("/*")) {
                    read_multi_line_comment();
                }
                // There are five kinds of tokens: identifiers, keywords, literals, operators, and other separators
                // http://eel.is/c++draft/lex.token#1.sentence-1
                // check in the following order:
                // 1. punctuators    must come before identifiers due to and/or/not/bitand/etc
                // 2. literals       must come before identifiers due to R"()" and stuff
                // 3. identifiers and keywords
                else if(auto punctuator = peek_any(punctuators_and_operators)) {
                    advance(punctuator->size());
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
                // 2. literals
                //      integer-literal          all start with digits
                //      character-literal        a prefix (u8  u  U  L) followed by ''
                //      floating-point-literal   all start with digits
                //      string-literal           a prefix (u8  u  U  L) followed by "", or R"...(...)..."
                //      boolean-literal          true/false
                //      pointer-literal          nullptr
                //      user-defined-literal     integer/float/string/char literal followed by a ud-suffix
                else if(auto named_literal = peek_any(to_array<std::string_view>({"false", "true", "nullptr"}))) {
                    advance(named_literal->size());
                    tokens.push_back({token_e::named_literal, *named_literal});
                }
                else if( // char literals
                    auto prefix = peek_any(to_array<std::string_view>({"u8", "u", "U", "L"}));
                    peek(prefix.value_or("").size()) == '\''
                ) {
                    auto begin = pos();
                    advance(prefix.value_or("").size());
                    read_char_literal();
                    auto end = pos();
                    tokens.push_back({token_e::string, std::string_view(source.data() + begin, end - begin)});
                }
                else if( // string literals
                    // reusing same prefix from last if
                    peek(prefix.value_or("").size()) == '"'
                        || (peek(prefix.value_or("").size()) == 'R' && peek(prefix.value_or("").size() + 1) == '"')
                ) {
                    auto begin = pos();
                    advance(prefix.value_or("").size());
                    if(peek() == 'R') {
                        read_raw_string_literal();
                    } else {
                        read_string_literal();
                    }
                    auto end = pos();
                    tokens.push_back({token_e::string, std::string_view(source.data() + begin, end - begin)});
                }
                else if(isdigit(peek())) { // integer, float, or UDL
                    auto begin = pos();
                    read_numeric_literal();
                    auto end = pos();
                    tokens.push_back({token_e::number, std::string_view(source.data() + begin, end - begin)});
                }
                // 3. identifiers
                else if(is_identifier_start(peek())) {
                    auto begin = pos();
                    read_identifier_or_keyword();
                    auto end = pos();
                    std::string_view contents = std::string_view(source.data() + begin, end - begin);
                    tokens.push_back({
                        keywords.find(contents) == keywords.end() ? token_e::identifier : token_e::keyword,
                        contents
                    });
                } else {
                    // we don't know....
                    tokens.push_back({token_e::unknown, std::string_view(source.data(), 1)});
                    advance();
                }

                // // universal character escapes like \U0001F60A get stringified so we have to handle them
                // // http://eel.is/c++draft/lex#charset-3
                // // fortunately we don't have to worry about unicode spellings of basic character set characters
                // // http://eel.is/c++draft/lex#charset-6.sentence-1
                // else if(peek() == '\\' && needle(peek(1)).is_in('u', 'U', 'N')) {
                //     // tokens.push_back(read_universal_character_name());
                // }
            }
            return tokens;
        }
    private:
        token_t read_whitespace() {
            auto begin = pos();
            std::size_t count = 0;
            while(isspace(peek())) {
                advance();
                count++;
            }
            return {token_e::whitespace, std::string_view(source.data() + begin, count)};
        }
        void read_comment() {
            while(!end() && peek() != '\n') advance();
        }
        void read_multi_line_comment() {
            while(!end() && peek() != '*' && peek(1) != '/') advance();
        }
        void read_identifier_or_keyword() {
            while(!end() && is_identifier_continue(peek())) advance();
        }
        void read_char_literal() {
            // http://eel.is/c++draft/lex.ccon
            expect('\'');
            while(peek() != '\'') {
                if(peek() == '\\') {
                    read_escape_sequence();
                } else {
                    advance();
                }
            }
            expect('\'');
        }
        void read_string_literal() {
            // string#nt:string-literal
            expect('"');
            while(peek() != '"') {
                if(peek() == '\\') {
                    read_escape_sequence();
                } else {
                    advance();
                }
            }
            expect('"');
        }
        void read_raw_string_literal() {
            // string#nt:string-literal
            expect('R');
            expect('"');
            // read d-char sequence
            // we'll be much more permissive about what is allowed as a d-char, we'll allow anything that's not a (
            auto d_begin = pos();
            while(peek() != '(') {
                advance();
            }
            auto d_char_sequence = std::string_view(source.data() + d_begin, pos() - d_begin);
            // read r-char sequence
            while(!end()) {
                if(peek() == ')' && peek(d_char_sequence, 1) && peek(1 + d_char_sequence.size()) == '"') {
                    // end of string
                    advance(1 + d_char_sequence.size() + 1);
                } else {
                    advance();
                }
            }
            expect('"');
        }
        void read_escape_sequence() {
            // http://eel.is/c++draft/lex.ccon#nt:escape-sequence
            // escape-sequence:
            //   simple-escape-sequence
            //   numeric-escape-sequence
            //   conditional-escape-sequence
            expect('\\');
            // the only escape sequences we really need to parse are those that could contain ' or "
            if(peek() == 'N' && peek(1) == '{') {
                advance();
                read_braced_sequence();
            } else {
                // read it as though it's a simple-escape-sequence
                advance();
            }
        }
        void read_numeric_literal() {
            // since we can assume a valid token, and we'll treat it as a pp-number....
            // http://eel.is/c++draft/lex.ppnumber
            // just read [0-9a-zA-Z'\.]+, essentially, with special handling for exponents/sign
            while(isdigit(peek()) || isalpha(peek()) || peek() == '\'' ||  peek() == '.') {
                if(needle(peek()).is_in('e', 'E', 'p', 'P') && needle(peek(1)).is_in('-', '+')) {
                    advance(2);
                } else {
                    advance();
                }
            }
        }
        // reads a {...}
        void read_braced_sequence() {
            expect('{');
            while(peek() != '}') {
                advance();
            }
            expect('}');
        }
    private:
        std::size_t pos() const {
            return it - source.begin();
        }
        char peek(std::size_t count = 0) const {
            auto pos = it - source.begin();
            if(pos + count < source.size()) {
                return *(it + count);
            } else {
                return 0;
            }
        }
        bool peek(std::string_view pattern, std::size_t offset = 0) const {
            for(std::size_t i = 0; i < pattern.size(); i++) {
                if(peek(offset + i) != pattern[i]) {
                    return false;
                }
            }
            return true;
        }
        template<std::size_t N>
        std::optional<std::string_view> peek_any(const std::array<std::string_view, N>& candidates) const {
            for(auto entry : candidates) {
                if(peek(entry)) {
                    return entry;
                }
            }
            return std::nullopt;
        }
        char advance() {
            if(it != source.end()) {
                return *it++;
            } else {
                return 0;
            }
        }
        void advance(std::size_t count) {
            while(it != source.end() && count--) {
                it++;
            }
        }
        void rollback(std::size_t count) {
            while(count--) {
                internal_verify(it != source.begin(), "Tokenizer rollback() failed, please report this bug");
                it--;
            }
        }
        void expect(char c) {
            internal_verify(peek() == c, "Tokenizer expect() failed, please report this bug");
            advance();
        }
        bool end() const {
            return it == source.end();
        }
    };

    std::vector<token_t> tokenize(std::string_view source, bool decompose_shr) {
        // try {
            return tokenizer(source).tokenize(decompose_shr);
        // } catch(...) {
            // return {};
        // }
    }

}
