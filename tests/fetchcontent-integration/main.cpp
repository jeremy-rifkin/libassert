#include <iostream>

#include <libassert/assert.hpp>

int main() {
	ASSERT(true);
	ASSUME(true);
	DEBUG_ASSERT(true);
	std::cout << "Good to go" << std::endl;
}
