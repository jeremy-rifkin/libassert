#ifndef ANALYSIS_HPP
#define ANALYSIS_HPP

#include <string>
#include <string_view>
#include <vector>

#include <libassert/assert.hpp>

namespace libassert::detail {
    struct highlight_block {
        std::string_view color;
        std::string content;
        highlight_block(std::string_view color_, std::string_view content_) : color(color_), content(content_) {}
    };

    LIBASSERT_ATTR_COLD LIBASSERT_EXPORT /* FIXME */
    std::string highlight(std::string_view expression, const color_scheme& scheme);

    LIBASSERT_ATTR_COLD
    std::vector<highlight_block> highlight_blocks(std::string_view expression, const color_scheme& scheme);

    LIBASSERT_ATTR_COLD literal_format get_literal_format(std::string_view expression);

    LIBASSERT_ATTR_COLD std::string_view trim_suffix(std::string_view expression);

    LIBASSERT_ATTR_COLD bool is_bitwise(std::string_view op);
}

#endif
