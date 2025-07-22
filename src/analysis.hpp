#ifndef ANALYSIS_HPP
#define ANALYSIS_HPP

#include <string>
#include <string_view>
#include <vector>

#include <libassert/assert.hpp>

LIBASSERT_BEGIN_NAMESPACE
namespace detail {
    struct highlight_block {
        std::string_view color; // text color
        std::string_view highlight; // used for the diff highlight
        std::string content;
        highlight_block(std::string_view color_, std::string_view content_) : color(color_), content(content_) {}
        highlight_block(
            std::string_view color_,
            std::string_view highlight_,
            std::string_view content_
        ) : color(color_), highlight(highlight_), content(content_) {}
    };

    LIBASSERT_EXPORT /* FIXME */
    std::string highlight(std::string_view expression, const color_scheme& scheme);

    std::vector<highlight_block> highlight_blocks(std::string_view expression, const color_scheme& scheme);

    std::string combine_blocks(const std::vector<highlight_block>& blocks, const color_scheme& scheme);

    std::size_t length(const std::vector<highlight_block>& blocks);

    std::optional<std::vector<highlight_block>> diff(
        std::vector<highlight_block> a,
        std::vector<highlight_block> b,
        const color_scheme& scheme
    );

    literal_format get_literal_format(std::string_view expression);

    std::string_view trim_suffix(std::string_view expression);

    bool is_bitwise(std::string_view op);
}
LIBASSERT_END_NAMESPACE

#endif
