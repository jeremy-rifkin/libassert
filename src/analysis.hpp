#ifndef ANALYSIS_HPP
#define ANALYSIS_HPP

#include <string>
#include <string_view>
#include <vector>

#include <assert/assert.hpp>

namespace libassert::detail {
    struct highlight_block {
        std::string_view color;
        std::string content;
    };

    LIBASSERT_ATTR_COLD LIBASSERT_EXPORT /* FIXME */
    std::string highlight(const std::string& expression);

    LIBASSERT_ATTR_COLD
    std::vector<highlight_block> highlight_blocks(const std::string& expression);

    LIBASSERT_ATTR_COLD std::string trim_suffix(const std::string& expression);
}

#endif
