#ifndef PRINTING_HPP
#define PRINTING_HPP

#include <vector>

#include "analysis.hpp"
#include "utils.hpp"

#include <libassert/assert.hpp>

namespace libassert::detail {
    struct column_t {
        size_t width;
        std::vector<highlight_block> blocks;
        bool right_align = false;
        LIBASSERT_ATTR_COLD column_t(size_t _width, std::vector<highlight_block> _blocks, bool _right_align = false)
            : width(_width), blocks(std::move(_blocks)), right_align(_right_align) {}
        LIBASSERT_ATTR_COLD column_t(const column_t&) = default;
        LIBASSERT_ATTR_COLD column_t(column_t&&) = default;
        LIBASSERT_ATTR_COLD ~column_t() = default;
        LIBASSERT_ATTR_COLD column_t& operator=(const column_t&) = default;
        LIBASSERT_ATTR_COLD column_t& operator=(column_t&&) = default;
    };

    LIBASSERT_ATTR_COLD
    std::string wrapped_print(const std::vector<column_t>& columns, const color_scheme& scheme);
}

#endif
