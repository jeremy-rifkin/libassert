#include "printing.hpp"

#include "microfmt.hpp"

namespace libassert::detail {
    LIBASSERT_ATTR_COLD
    // TODO
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    std::string wrapped_print(const std::vector<column_t>& columns, const color_scheme& scheme) {
        // 2d array rows/columns
        struct line_content {
            size_t length;
            std::string content;
        };
        std::vector<std::vector<line_content>> lines;
        lines.emplace_back(columns.size());
        // populate one column at a time
        for(size_t i = 0; i < columns.size(); i++) {
            auto [width, blocks, _] = columns[i];
            size_t current_line = 0;
            for(auto& block : blocks) {
                size_t block_i = 0;
                // digest block
                while(block_i != block.content.size()) {
                    if(lines.size() == current_line) {
                        lines.emplace_back(columns.size());
                    }
                    // number of characters we can extract from the block
                    size_t extract = std::min(width - lines[current_line][i].length, block.content.size() - block_i);
                    LIBASSERT_PRIMITIVE_ASSERT(block_i + extract <= block.content.size());
                    auto substr = std::string_view(block.content).substr(block_i, extract);
                    // handle newlines
                    if(auto x = substr.find('\n'); x != std::string_view::npos) {
                        substr = substr.substr(0, x);
                        extract = x + 1; // extract newline but don't print
                    }
                    // append
                    lines[current_line][i].content += block.color;
                    lines[current_line][i].content += substr;
                    lines[current_line][i].content += block.color.empty() ? "" : scheme.reset;
                    // advance
                    block_i += extract;
                    lines[current_line][i].length += extract;
                    // new line if necessary
                    // substr.size() != extract iff newline
                    if(lines[current_line][i].length >= width || substr.size() != extract) {
                        current_line++;
                    }
                }
            }
        }
        // print
        std::string output;
        for(auto& line : lines) {
            // don't print empty columns with no content in subsequent columns and more importantly
            // don't print empty spaces they'll mess up lines after terminal resizing even more
            size_t last_col = 0;
            for(size_t i = 0; i < line.size(); i++) {
                if(!line[i].content.empty()) {
                    last_col = i;
                }
            }
            for(size_t i = 0; i <= last_col; i++) {
                auto& content = line[i];
                if(columns[i].right_align) {
                    output += microfmt::format(
                        "{<{}}{}{}",
                        i == last_col ? 0 : int(columns[i].width - content.length),
                        "",
                        content.content,
                        i == last_col ? "\n" : " "
                    );
                } else {
                    output += microfmt::format(
                        "{}{<{}}{}",
                        content.content,
                        i == last_col ? 0 : int(columns[i].width - content.length),
                        "",
                        i == last_col ? "\n" : " "
                    );
                }
            }
        }
        return output;
    }
}
