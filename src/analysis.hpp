#ifndef ANALYSIS_HPP
#define ANALYSIS_HPP

#include <string>
#include <string_view>

#include <assert/assert.hpp>

namespace libassert::detail {
    struct highlight_block {
        std::string_view color;
        std::string content;
    };

    LIBASSERT_ATTR_COLD
    std::string prettify_type(std::string type);

    LIBASSERT_ATTR_COLD
    std::string highlight(const std::string& expression);

    LIBASSERT_ATTR_COLD
    std::vector<highlight_block> highlight_blocks(const std::string& expression);

    LIBASSERT_ATTR_COLD
    std::vector<highlight_block> highlight_blocks(const std::string& expression);

    LIBASSERT_ATTR_COLD literal_format get_literal_format(const std::string& expression);

    LIBASSERT_ATTR_COLD std::string trim_suffix(const std::string& expression);

    LIBASSERT_ATTR_COLD bool is_bitwise(std::string_view op);

    LIBASSERT_ATTR_COLD
    std::pair<std::string, std::string> decompose_expression(
        const std::string& expression,
        const std::string_view target_op
    );
}

#endif
