#include <algorithm>
#include <cstdio>
#include <cctype>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <stdexcept>
#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <assert/assert.hpp>

#include "common.hpp"
#include "utils.hpp"
#include "analysis.hpp"

namespace libassert::detail {
    /*
     * C++ syntax analysis logic
     */

    LIBASSERT_ATTR_COLD
    std::string union_regexes(std::initializer_list<std::string_view> regexes) {
        std::string composite;
        for(const std::string_view str : regexes) {
            if(!composite.empty()) {
                composite += "|";
            }
            composite += "(?:" + std::string(str) + ")";
        }
        return composite;
    }

    LIBASSERT_ATTR_COLD
    std::string prettify_type(std::string type) {
        // > > -> >> replacement
        // could put in analysis:: but the replacement is basic and this is more convenient for
        // using in the stringifier too
        replace_all_dynamic(type, "> >", ">>");
        // "," -> ", " and " ," -> ", "
        static const std::regex comma_re(R"(\s*,\s*)");
        replace_all(type, comma_re, ", ");
        // class C -> C for msvc
        static const std::regex class_re(R"(\b(class|struct)\s+)");
        replace_all(type, class_re, "");
        // rules to replace std::basic_string -> std::string and std::basic_string_view -> std::string_view
        // rule to replace ", std::allocator<whatever>"
        static const std::pair<std::regex, std::string_view> basic_string = {
            std::regex(R"(std(::[a-zA-Z0-9_]+)?::basic_string<char)"), "std::string"
        };
        replace_all_template(type, basic_string);
        static const std::pair<std::regex, std::string_view> basic_string_view = {
            std::regex(R"(std(::[a-zA-Z0-9_]+)?::basic_string_view<char)"), "std::string_view"
        };
        replace_all_template(type, basic_string_view);
        static const std::pair<std::regex, std::string_view> allocator = {
            std::regex(R"(,\s*std(::[a-zA-Z0-9_]+)?::allocator<)"), ""
        };
        replace_all_template(type, allocator);
        static const std::pair<std::regex, std::string_view> default_delete = {
            std::regex(R"(,\s*std(::[a-zA-Z0-9_]+)?::default_delete<)"), ""
        };
        replace_all_template(type, default_delete);
        // replace std::__cxx11 -> std:: for gcc dual abi
        // https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_dual_abi.html
        replace_all_dynamic(type, "std::__cxx11::", "std::");
        return type;
    }

    class analysis {
        enum class token_e {
            keyword,
            punctuation,
            number,
            string,
            named_literal,
            identifier,
            whitespace
        };

        struct token_t {
            token_e token_type;
            std::string str;
        };

    public:
        // Analysis singleton, lazy-initialize all the regex nonsense
        // 8 BSS bytes and <512 bytes heap bytes not a problem
        static std::unique_ptr<analysis> analysis_singleton;
        static std::mutex singleton_mutex;
        static analysis& get() {
            const std::unique_lock lock(singleton_mutex);
            if(analysis_singleton == nullptr) {
                analysis_singleton = std::unique_ptr<analysis>(new analysis);
            }
            return *analysis_singleton;
        }

        // could all be const but I don't want to try to pack everything into an init list
        std::vector<std::pair<token_e, std::regex>> rules; // could be std::array but I don't want to hard-code a size
        std::regex escapes_re;
        std::unordered_map<std::string_view, int> precedence;
        std::unordered_map<std::string_view, std::string_view> braces = {
            // template angle brackets excluded from this analysis
            { "(", ")" }, { "{", "}" }, { "[", "]" }, { "<:", ":>" }, { "<%", "%>" }
        };
        std::unordered_map<std::string_view, std::string_view> digraph_map = {
            { "<:", "[" }, { "<%", "{" }, { ":>", "]" }, { "%>", "}" }
        };
        // punctuators to highlight
        std::unordered_set<std::string_view> highlight_ops = {
            "~", "!", "+", "-", "*", "/", "%", "^", "&", "|", "=", "+=", "-=", "*=", "/=", "%=",
            "^=", "&=", "|=", "==", "!=", "<", ">", "<=", ">=", "<=>", "&&", "||", "<<", ">>",
            "<<=", ">>=", "++", "--", "and", "or", "xor", "not", "bitand", "bitor", "compl",
            "and_eq", "or_eq", "xor_eq", "not_eq"
        };
        // all operators
        std::unordered_set<std::string_view> operators = {
            ":", "...", "?", "::", ".", ".*", "->", "->*", "~", "!", "+", "-", "*", "/", "%", "^",
            "&", "|", "=", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "==", "!=", "<", ">",
            "<=", ">=", "<=>", "&&", "||", "<<", ">>", "<<=", ">>=", "++", "--", ",", "and", "or",
            "xor", "not", "bitand", "bitor", "compl", "and_eq", "or_eq", "xor_eq", "not_eq"
        };
        std::unordered_map<std::string_view, std::string_view> alternative_operators_map = {
            {"and", "&&"}, {"or", "||"}, {"xor", "^"}, {"not", "!"}, {"bitand", "&"},
            {"bitor", "|"}, {"compl", "~"}, {"and_eq", "&="}, {"or_eq", "|="}, {"xor_eq", "^="},
            {"not_eq", "!="}
        };
        std::unordered_set<std::string_view> bitwise_operators = {
            "^", "&", "|", "^=", "&=", "|=", "xor", "bitand", "bitor", "and_eq", "or_eq", "xor_eq"
        };
        std::vector<std::pair<std::regex, literal_format>> literal_formats;

    private:
        LIBASSERT_ATTR_COLD
        analysis() {
            // https://eel.is/c++draft/gram.lex
            // generate regular expressions
            std::string keywords[] = { "alignas", "constinit", "public", "alignof", "const_cast",
                "float", "register", "try", "asm", "continue", "for", "reinterpret_cast", "typedef",
                "auto", "co_await", "friend", "requires", "typeid", "bool", "co_return", "goto",
                "return", "typename", "break", "co_yield", "if", "short", "union", "case",
                "decltype", "inline", "signed", "unsigned", "catch", "default", "int", "sizeof",
                "using", "char", "delete", "long", "static", "virtual", "char8_t", "do", "mutable",
                "static_assert", "void", "char16_t", "double", "namespace", "static_cast",
                "volatile", "char32_t", "dynamic_cast", "new", "struct", "wchar_t", "class", "else",
                "noexcept", "switch", "while", "concept", "enum", "template", "const", "explicit",
                "operator", "this", "consteval", "export", "private", "thread_local", "constexpr",
                "extern", "protected", "throw" };
            std::string punctuators[] = { "{", "}", "[", "]", "(", ")", "<:", ":>", "<%",
                "%>", ";", ":", "...", "?", "::", ".", ".*", "->", "->*", "~", "!", "+", "-", "*",
                "/", "%", "^", "&", "|", "=", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "==",
                "!=", "<", ">", "<=", ">=", "<=>", "&&", "||", "<<", ">>", "<<=", ">>=", "++", "--",
                ",", "and", "or", "xor", "not", "bitand", "bitor", "compl", "and_eq", "or_eq",
                "xor_eq", "not_eq", "#" }; // # appears in some lambda signatures
            // Sort longest -> shortest (secondarily A->Z)
            const auto cmp = [](const std::string_view a, const std::string_view b) {
                if(a.length() > b.length()) { return true; }
                else if(a.length() == b.length()) { return a < b; }
                else { return false; }
            };
            std::sort(std::begin(keywords), std::end(keywords), cmp);
            std::sort(std::begin(punctuators), std::end(punctuators), cmp);
            // Escape special characters and add wordbreaks
            const std::regex special_chars { R"([-[\]{}()*+?.,\^$|#\s])" };
            std::transform(std::begin(punctuators), std::end(punctuators), std::begin(punctuators),
                [&special_chars](const std::string& str) {
                    return std::regex_replace(str + (isalpha(str[0]) ? "\\b" : ""), special_chars, "\\$&");
                });
            // https://eel.is/c++draft/lex.pptoken#3.2
            *std::find(std::begin(punctuators), std::end(punctuators), "<:") += "(?!:[^:>])";
            // regular expressions
            std::string keywords_re    = "(?:" + join(keywords, "|") + ")\\b";
            std::string punctuators_re = join(punctuators, "|");
            // numeric literals
            const std::string optional_integer_suffix = "(?:[Uu](?:LL?|ll?|Z|z)?|(?:LL?|ll?|Z|z)[Uu]?)?";
            const std::string int_binary  = "0[Bb][01](?:'?[01])*" + optional_integer_suffix;
            // slightly modified from grammar so 0 is lexed as a decimal literal instead of octal
            const std::string int_octal   = "0(?:'?[0-7])+" + optional_integer_suffix;
            const std::string int_decimal = "(?:0|[1-9](?:'?\\d)*)" + optional_integer_suffix;
            const std::string int_hex        = "0[Xx](?!')(?:'?[\\da-fA-F])+" + optional_integer_suffix;
            const std::string digit_sequence = "\\d(?:'?\\d)*";
            const std::string fractional_constant = stringf(
                "(?:(?:%s)?\\.%s|%s\\.)",
                digit_sequence.c_str(),
                digit_sequence.c_str(),
                digit_sequence.c_str()
            );
            const std::string exponent_part = "(?:[Ee][\\+-]?" + digit_sequence + ")";
            const std::string suffix = "[FfLl]";
            const std::string float_decimal = stringf(
                "(?:%s%s?|%s%s)%s?",
                fractional_constant.c_str(),
                exponent_part.c_str(),
                digit_sequence.c_str(),
                exponent_part.c_str(),
                suffix.c_str()
            );
            const std::string hex_digit_sequence = "[\\da-fA-F](?:'?[\\da-fA-F])*";
            const std::string hex_frac_const = stringf(
                "(?:(?:%s)?\\.%s|%s\\.)",
                hex_digit_sequence.c_str(),
                hex_digit_sequence.c_str(),
                hex_digit_sequence.c_str()
            );
            const std::string binary_exp = "[Pp][\\+-]?" + digit_sequence;
            const std::string float_hex = stringf(
                "0[Xx](?:%s|%s)%s%s?",
                hex_frac_const.c_str(),
                hex_digit_sequence.c_str(),
                binary_exp.c_str(),
                suffix.c_str()
            );
            // char and string literals
            const std::string escapes = R"(\\[0-7]{1,3}|\\x[\da-fA-F]+|\\.)";
            const std::string char_literal = R"((?:u8|[UuL])?'(?:)" + escapes + R"(|[^\n'])*')";
            const std::string string_literal = R"((?:u8|[UuL])?"(?:)" + escapes + R"(|[^\n"])*")";
                                 // \2 because the first capture is the match without the rest of the file
            const std::string raw_string_literal = R"((?:u8|[UuL])?R"([^ ()\\t\r\v\n]*)\((?:(?!\)\2\").)*\)\2")";
            escapes_re = std::regex(escapes);
            // final rule set
            // rules must be sequenced as a topological sort adhering to:
            // keyword > identifier
            // number > punctuation (for .1f)
            // float > int (for 1.5 and hex floats)
            // hex int > decimal int
            // named_literal > identifier
            // string > identifier (for R"(foobar)")
            // punctuation > identifier (for "not" and other alternative operators)
            const std::pair<token_e, std::string> rules_raw[] = {
                { token_e::keyword    , keywords_re },
                { token_e::number     , union_regexes({
                    float_decimal,
                    float_hex,
                    int_binary,
                    int_octal,
                    int_hex,
                    int_decimal
                }) },
                { token_e::punctuation, punctuators_re },
                // nullopt added deliberately
                { token_e::named_literal, "true|false|nullptr|nullopt" },
                { token_e::string     , union_regexes({
                    char_literal,
                    raw_string_literal,
                    string_literal
                }) },
                { token_e::identifier , R"((?!\d+)(?:[\da-zA-Z_\$]|\\u[\da-fA-F]{4}|\\U[\da-fA-F]{8})+)" },
                { token_e::whitespace , R"(\s+)" }
            };
            rules.resize(std::size(rules_raw));
            for(size_t i = 0; i < std::size(rules_raw); i++) {
                                // [^] instead of . because . does not match newlines
                const std::string str = stringf("^(%s)[^]*", rules_raw[i].second.c_str());
                #ifdef _0_DEBUG_ASSERT_LEXER_RULES
                 fprintf(stderr, "%s : %s\n", rules_raw[i].first.c_str(), str.c_str());
                #endif
                rules[i] = { rules_raw[i].first, std::regex(str) };
            }
            // setup literal format rules
            literal_formats = {
                { std::regex(int_binary),    literal_format::integer_binary },
                { std::regex(int_octal),     literal_format::integer_octal },
                { std::regex(int_decimal),   literal_format::default_format },
                { std::regex(int_hex),       literal_format::integer_hex },
                { std::regex(float_decimal), literal_format::default_format },
                { std::regex(float_hex),     literal_format::float_hex },
                { std::regex(char_literal),  literal_format::default_format }
            };
            // generate precedence table
            // bottom few rows of the precedence table:
            const std::unordered_map<int, std::vector<std::string_view>> precedences = {
                { -1, { "<<", ">>" } },
                { -2, { "<=>" } },
                { -3, { "<", "<=", ">=", ">" } },
                { -4, { "==", "!=" } },
                { -5, { "&" } },
                { -6, { "^" } },
                { -7, { "|" } },
                { -8, { "&&" } },
                { -9, { "||" } },
                            // Note: associativity logic currently relies on these having precedence -10
                { -10, { "?", ":", "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=" } },
                { -11, { "," } }
            };
            std::unordered_map<std::string_view, int> table;
            for(const auto& [ p, ops ] : precedences) {
                for(const auto& op : ops) {
                    table.insert({op, p});
                }
            }
            precedence = table;
        }

    public:
        LIBASSERT_ATTR_COLD
        std::string_view normalize_op(const std::string_view op) {
            // Operators need to be normalized to support alternative operators like and and bitand
            // Normalization instead of just adding to the precedence table because target operators
            // will always be the normalized operator even when the alternative operator is used.
            if(auto it = alternative_operators_map.find(op); it != alternative_operators_map.end()) {
                return it->second;
            } else {
                return op;
            }
        }

        LIBASSERT_ATTR_COLD
        std::string_view normalize_brace(const std::string_view brace) {
            // Operators need to be normalized to support alternative operators like and and bitand
            // Normalization instead of just adding to the precedence table because target operators
            // will always be the normalized operator even when the alternative operator is used.
            if(auto it = digraph_map.find(brace); it != digraph_map.end()) {
                return it->second;
            } else {
                return brace;
            }
        }

        LIBASSERT_ATTR_COLD
        std::vector<token_t> tokenize(const std::string& expression, bool decompose_shr = false) {
            std::vector<token_t> tokens;
            size_t i = 0;
            while(i < expression.length()) {
                std::smatch match;
                bool at_least_one_matched = false;
                for(const auto& [ type, re ] : rules) {
                    // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
                    if(std::regex_match(std::begin(expression) + i, std::end(expression), match, re)) {
                        #ifdef _0_DEBUG_ASSERT_TOKENIZATION
                         fprintf(stderr, "%s\n", match[1].str().c_str());
                         fflush(stdout);
                        #endif
                        if(decompose_shr && match[1].str() == ">>") { // Do >> decomposition now for templates
                            tokens.push_back({ type, ">" });
                            tokens.push_back({ type, ">" });
                        } else {
                            tokens.push_back({ type, match[1].str() });
                        }
                        i += match[1].length();
                        at_least_one_matched = true;
                        break;
                    }
                }
                if(!at_least_one_matched) {
                    throw std::logic_error("error: invalid token");
                }
            }
            return tokens;
        }

        LIBASSERT_ATTR_COLD
        std::vector<highlight_block> highlight_string(const std::string& str) const {
            std::vector<highlight_block> output;
            std::smatch match;
            std::size_t i = 0;
            // NOLINTNEXTLINE(bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions)
            while(std::regex_search(str.cbegin() + i, str.cend(), match, escapes_re)) {
                // add string part
                // TODO: I don't know why this assert was added, I might have done it in dev on a whim. Re-evaluate.
                // LIBASSERT_PRIMITIVE_ASSERT(match.position() > 0);
                if(match.position() > 0) {
                    output.push_back({GREEN, str.substr(i, match.position())});
                }
                output.push_back({BLUE, str.substr(i + match.position(), match.length())});
                i += match.position() + match.length();
            }
            if(i < str.length()) {
                output.push_back({GREEN, str.substr(i)});
            }
            return output;
        }

        LIBASSERT_ATTR_COLD
        // TODO: Refactor
        // NOLINTNEXTLINE(readability-function-cognitive-complexity)
        std::vector<highlight_block> highlight(const std::string& expression) try {
            const auto tokens = tokenize(expression);
            std::vector<highlight_block> output;
            for(size_t i = 0; i < tokens.size(); i++) {
                const auto& token = tokens[i];
                // Peek next non-whitespace token, return empty whitespace token if end is reached
                const auto peek = [i, &tokens](size_t j = 1) {
                    for(size_t k = 1; j > 0 && i + k < tokens.size(); k++) {
                        if(tokens[i + k].token_type != token_e::whitespace) {
                            if(--j == 0) {
                                return tokens[i + k];
                            }
                        }
                    }
                    LIBASSERT_PRIMITIVE_ASSERT(j != 0);
                    return token_t { token_e::whitespace, "" };
                };
                switch(token.token_type) {
                    case token_e::keyword:
                        output.push_back({PURPL, token.str});
                        break;
                    case token_e::punctuation:
                        if(highlight_ops.count(token.str)) {
                            output.push_back({PURPL, token.str});
                        } else {
                            output.push_back({"", token.str});
                        }
                        break;
                    case token_e::named_literal:
                        output.push_back({ORANGE, token.str});
                        break;
                    case token_e::number:
                        output.push_back({CYAN, token.str});
                        break;
                    case token_e::string:
                        {
                            auto string_tokens = highlight_string(token.str);
                            output.insert(output.end(), string_tokens.begin(), string_tokens.end());
                        }
                        break;
                    case token_e::identifier:
                        if(peek().str == "(") {
                            output.push_back({BLUE, token.str});
                        } else if(peek().str == "::") {
                            output.push_back({YELLOW, token.str});
                        } else {
                            output.push_back({BLUE, token.str});
                        }
                        break;
                    case token_e::whitespace:
                        output.push_back({"", token.str});
                        break;
                }
            }
            return output;
        } catch(...) {
            return {{"", expression}};
        }

        LIBASSERT_ATTR_COLD
        literal_format get_literal_format(const std::string& expression) {
            for(auto& [ re, type ] : literal_formats) {
                if(std::regex_match(expression, re)) {
                    return type;
                }
            }
            return literal_format::default_format; // not a literal // TODO
        }

        LIBASSERT_ATTR_COLD
        static token_t find_last_non_ws(const std::vector<token_t>& tokens, size_t i) {
            // returns empty token_e::whitespace on failure
            while(i--) {
                if(tokens[i].token_type != token_e::whitespace) {
                    return tokens[i];
                }
            }
            return {token_e::whitespace, ""};
        }

        LIBASSERT_ATTR_COLD
        static std::string_view get_real_op(const std::vector<token_t>& tokens, const size_t i) {
            // re-coalesce >> if necessary
            const bool is_shr = tokens[i].str == ">" && i < tokens.size() - 1 && tokens[i + 1].str == ">";
            return is_shr ? std::string_view(">>") : std::string_view(tokens[i].str);
        }

        // In this function we are essentially exploring all possible parse trees for an expression
        // an making an attempt to disambiguate as much as we can. It's potentially O(2^t) (?) with
        // t being the number of possible templates in the expression, but t is anticipated to
        // always be small.
        // Returns true if parse tree traversal was a success, false if depth was exceeded
        static constexpr int max_depth = 10;
        // TODO
        // NOLINTNEXTLINE(readability-function-cognitive-complexity)
        LIBASSERT_ATTR_COLD bool pseudoparse(
            const std::vector<token_t>& tokens,
            const std::string_view target_op,
            size_t i,
            int current_lowest_precedence,
            int template_depth,
            int middle_index, // where the split currently is, current op = tokens[middle_index]
            int depth,
            std::set<int>& output
        ) {
            #ifdef _0_DEBUG_ASSERT_DISAMBIGUATION
            (void)fprintf(stderr, "*");
            #endif
            if(depth > max_depth) {
                (void)fprintf(stderr, "Max depth exceeded\n");
                return false;
            }
            // precedence table is binary, unary operators have highest precedence
            // we can figure out unary / binary easy enough
            enum {
                expecting_operator,
                expecting_term
            } state = expecting_term;
            for(; i < tokens.size(); i++) {
                const token_t& token = tokens[i];
                // scan forward to matching brace
                // can assume braces are balanced
                const auto scan_forward = [this, &i, &tokens](
                    const std::string_view open,
                    const std::string_view close)
                {
                    bool empty = true;
                    int count = 0;
                    while(++i < tokens.size()) {
                        if(normalize_brace(tokens[i].str) == normalize_brace(open)) {
                            count++;
                        } else if(normalize_brace(tokens[i].str) == normalize_brace(close)) {
                            if(count-- == 0) {
                                break;
                            }
                        } else if(tokens[i].token_type != token_e::whitespace) {
                            empty = false;
                        }
                    }
                    if(i == tokens.size() && count != -1) {
                        LIBASSERT_PRIMITIVE_ASSERT(false, "ill-formed expression input");
                    }
                    return empty;
                };
                switch(token.token_type) {
                    case token_e::punctuation:
                        if(operators.count(token.str)) {
                            if(state == expecting_term) {
                                // must be unary, continue
                            } else {
                                // template can only open with a < token, no need to check << or <<=
                                // also must be preceeded by an identifier
                                if(token.str == "<" && find_last_non_ws(tokens, i).token_type == token_e::identifier) {
                                    // branch 1: this is a template opening
                                    const bool success = pseudoparse(
                                        tokens,
                                        target_op,
                                        i + 1,
                                        current_lowest_precedence,
                                        template_depth + 1,
                                        middle_index, depth + 1,
                                        output
                                    );
                                    if(!success) { // early exit if we have to discard
                                        return false;
                                    }
                                    // branch 2: this is a binary operator // fallthrough
                                } else if(token.str == "<" && normalize_brace(find_last_non_ws(tokens, i).str) == "]") {
                                    // this must be a template parameter list, part of a generic lambda
                                    const bool empty = scan_forward("<", ">");
                                    LIBASSERT_PRIMITIVE_ASSERT(!empty);
                                    state = expecting_operator;
                                    continue;
                                }
                                if(template_depth > 0 && token.str == ">") {
                                    // No branch here: This must be a close. C++ standard
                                    // Disambiguates by treating > always as a template parameter
                                    // list close and >> is broken down.
                                    // >= and >>= don't need to be broken down and we don't need to
                                    // worry about re-tokenizing beyond just the simple breakdown.
                                    // I.e. we don't need to worry about x<2>>>1 which is tokenized
                                    // as x < 2 >> > 1 but perhaps being intended as x < 2 > >> 1.
                                    // Standard has saved us from this complexity.
                                    // Note: >> breakdown moved to initial tokenization so we can
                                    // take the token vector by reference.
                                    template_depth--;
                                    state = expecting_operator;
                                    continue;
                                }
                                // binary
                                if(template_depth == 0) { // ignore precedence in template parameter list
                                    // re-coalesce >> if necessary
                                    const std::string_view op = normalize_op(get_real_op(tokens, i));
                                    if(
                                        precedence.count(op)
                                        && (
                                            precedence.at(op) < current_lowest_precedence
                                            || (
                                                precedence.at(op) == current_lowest_precedence
                                                && precedence.at(op) != -10
                                            )
                                        )
                                    ) {
                                        middle_index = (int)i;
                                        current_lowest_precedence = precedence.at(op);
                                    }
                                    if(op == ">>") {
                                        i++;
                                    }
                                }
                                state = expecting_term;
                            }
                        } else if(braces.count(token.str)) {
                            // We can assume the given expression is valid.
                            // Braces must be balanced.
                            // Scan forward until finding matching brace, don't need to take into
                            // account other types of braces.
                            const std::string_view open = token.str;
                            const std::string_view close = braces.at(token.str);
                            const bool empty = scan_forward(open, close);
                            // Handle () and {} when they aren't a call/initializer
                            // [] may appear in lambdas when state == expecting_term
                            // [](){ ... }() is parsed fine because state == expecting_operator
                            // after the captures list. Not concerned with template parameters at
                            // the moment.
                            if(state == expecting_term && empty && normalize_brace(open) != "[") {
                                return true; // this is a failed parse tree
                            }
                            state = expecting_operator;
                        } else {
                            LIBASSERT_PRIMITIVE_ASSERT(false, "unhandled punctuation?");
                        }
                        break;
                    case token_e::keyword:
                    case token_e::named_literal:
                    case token_e::number:
                    case token_e::string:
                    case token_e::identifier:
                        state = expecting_operator;
                    case token_e::whitespace:
                        break;
                }
            }
            if(
                middle_index != -1
                && normalize_op(get_real_op(tokens, middle_index)) == target_op
                && template_depth == 0
                && state == expecting_operator
            ) {
                output.insert(middle_index);
            } else {
                // failed parse tree, ignore
            }
            return true;
        }

        LIBASSERT_ATTR_COLD
        std::pair<std::string, std::string> decompose_expression(
            const std::string& expression,
            const std::string_view target_op
        ) {
            // While automatic decomposition allows something like `assert(foo(n) == bar<n> + n);`
            // treated as `assert_eq(foo(n), bar<n> + n);` we only get the full expression's string
            // representation.
            // Here we attempt to parse basic info for the raw string representation. Just enough to
            // decomposition into left and right parts for diagnostic.
            // Template parameters make C++ grammar ambiguous without type information. That being
            // said, many expressions can be disambiguated.
            // This code will make guesses about the grammar, essentially doing a traversal of all
            // possibly parse trees and looking for ones that could work. This is O(2^t) with the
            // where t is the number of potential templates. Usually t is small but this should
            // perhaps be limited.
            // Will return {"left", "right"} if unable to decompose unambiguously.
            // Some cases to consider
            //   tgt  expr
            //   ==   a < 1 == 2 > ( 1 + 3 )
            //   ==   a < 1 == 2 > - 3 == ( 1 + 3 )
            //   ==   ( 1 + 3 ) == a < 1 == 2 > - 3 // <- ambiguous
            //   ==   ( 1 + 3 ) == a < 1 == 2 > ()
            //   <    a<x<x<x<x<x<x<x<x<x<1>>>>>>>>>
            //   ==   1 == something<a == b>>2 // <- ambiguous
            //   <    1 == something<a == b>>2 // <- should be an error
            //   <    1 < something<a < b>>2 // <- ambiguous
            // If there is only one candidate from the parse trees considered, expression has been
            // disambiguated. If there are no candidates that's an error. If there is more than one
            // candidate the expression may be ambiguous, but, not necessarily. For example,
            // a < 1 == 2 > - 3 == ( 1 + 3 ) is split the same regardless of whether a < 1 == 2 > is
            // treated as a template or not:
            //   left:  a < 1 == 2 > - 3
            //   right: ( 1 + 3 )
            //   ---
            //   left:  a < 1 == 2 > - 3
            //   right: ( 1 + 3 )
            //   ---
            // Because we're just splitting an expression in half, instead of storing tokens for
            // both sides we can just store an index of the split.
            // Note: We don't need to worry about expressions where there is only a single term.
            // This will only be called on decomposable expressions.
            // Note: The >> to > > token breakdown needed in template parameter lists is done in the
            // initial tokenization so we can pass the token vector by reference and avoid copying
            // for every recursive path (O(t^2)). This does not create an issue for syntax
            // highlighting as long as >> and > are highlighted the same.
            const auto tokens = tokenize(expression, true);
            // We're only looking for the split, we can just store a set of split indices. No need
            // to store a vector<pair<vector<token_t>, vector<token_t>>>
            std::set<int> candidates;
            const bool success = pseudoparse(tokens, target_op, 0, 0, 0, -1, 0, candidates);
            #ifdef _0_DEBUG_ASSERT_DISAMBIGUATION
             fprintf(stderr, "\n%d %d\n", (int)candidates.size(), success);
             for(size_t m : candidates) {
                 std::vector<std::string> left_strings;
                 std::vector<std::string> right_strings;
                 for(size_t i = 0; i < m; i++) left_strings.push_back(tokens[i].str);
                 for(size_t i = m + 1; i < tokens.size(); i++) right_strings.push_back(tokens[i].str);
                 fprintf(stderr, "left:  %s\n", libassert::detail::highlight(std::string(trim(join(left_strings, "")))).c_str());
                 fprintf(stderr, "right: %s\n", libassert::detail::highlight(std::string(trim(join(right_strings, "")))).c_str());
                 fprintf(stderr, "target_op: %s\n", target_op.data()); // should be null terminated
                 fprintf(stderr, "---\n");
             }
            #endif
            if(success && candidates.size() == 1) {
                std::vector<std::string> left_strings;
                std::vector<std::string> right_strings;
                const size_t m = *candidates.begin();
                for(size_t i = 0; i < m; i++) {
                    left_strings.push_back(tokens[i].str);
                }
                // >> is decomposed and requires special handling (m will be the index of the first > token)
                for(size_t i = m + 1 + (target_op == ">>" ? 1 : 0); i < tokens.size(); i++) {
                    right_strings.push_back(tokens[i].str);
                }
                return {
                    std::string(trim(join(left_strings, ""))),
                    std::string(trim(join(right_strings, "")))
                };
            } else {
                return { "left", "right" };
            }
        }
    };

    std::unique_ptr<analysis> analysis::analysis_singleton;
    std::mutex analysis::singleton_mutex;

    LIBASSERT_ATTR_COLD
    std::string highlight(const std::string& expression) {
        #ifdef NCOLOR
        return expression;
        #else
        auto blocks = analysis::get().highlight(expression);
        std::string str;
        for(auto& block : blocks) {
            str += block.color;
            str += block.content;
            if(!block.color.empty()) {
                str += RESET;
            }
        }
        return str;
        #endif
    }

    LIBASSERT_ATTR_COLD
    std::vector<highlight_block> highlight_blocks(const std::string& expression) {
        #ifdef NCOLOR
        return expression;
        #else
        return analysis::get().highlight(expression);
        #endif
    }

    LIBASSERT_ATTR_COLD literal_format get_literal_format(const std::string& expression) {
        return analysis::get().get_literal_format(expression);
    }

    LIBASSERT_ATTR_COLD std::string trim_suffix(const std::string& expression) {
        return expression.substr(0, expression.find_last_not_of("FfUuLlZz") + 1);
    }

    LIBASSERT_ATTR_COLD bool is_bitwise(std::string_view op) {
        return analysis::get().bitwise_operators.count(op);
    }

    LIBASSERT_ATTR_COLD
    std::pair<std::string, std::string> decompose_expression(
        const std::string& expression,
        const std::string_view target_op
    ) {
        return analysis::get().decompose_expression(expression, target_op);
    }
}
