#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <libassert/assert.hpp>

#include "../../src/analysis.hpp"

int main() {
    std::ifstream file("tests/test_program.cpp"); // just a test program that doesn't have preprocessor directives, which we don't tokenize
    std::ostringstream buf;
    buf<<file.rdbuf();
    std::cout<<libassert::detail::highlight(buf.str(), libassert::color_scheme::ansi_rgb);
}
