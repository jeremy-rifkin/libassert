#include "assert.hpp"

#include <fstream>
#include <iostream>

int main() {
	std::ifstream file("tests/test_program.cpp"); // just a test program that doesn't have preprocessor directives, which we don't tokenize
	std::ostringstream buf;
	buf<<file.rdbuf();
	std::cout<<assert_detail::highlight(buf.str());
}
