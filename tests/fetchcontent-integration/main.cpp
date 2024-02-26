#include <libassert/assert.hpp>

#include <iostream>

int main() {
    ASSERT(true);
    ASSUME(true);
    DEBUG_ASSERT(true);
    std::cout<<"Good to go"<<std::endl;
}
