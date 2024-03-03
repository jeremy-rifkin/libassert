#ifndef TOKENIZER_HPP
#define TOKENIZER_HPP

#include <string_view>
#include <vector>

namespace libassert::detail {
    enum class token_e {
        keyword,
        punctuation,
        number,
        string,
        named_literal,
        identifier,
        whitespace,
        unknown
    };

    struct token_t {
        token_e type;
        std::string_view str;
        token_t(token_e type_, std::string_view str_) : type(type_), str(str_) {}
    };

    // lifetime notes: token_t's store string_views to data with at least the same lifetime as the source string_view's
    // data
    std::vector<token_t> tokenize(std::string_view source, bool decompose_shr = false);
}

#endif
