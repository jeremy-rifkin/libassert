#include "assert.hpp"

#include <iostream>

int main() {
    bool success = true;
    auto test = [&success](const std::string& type, const std::string& expected) {
        auto pretty = asserts::detail::prettify_type(type);
        if(pretty != expected) {
            std::cout<<"Error:"<<std::endl;
            std::cout<<"Expected:"<<expected<<std::endl;
            std::cout<<"Result:  "<<pretty<<std::endl;
            success = false;
        }
    };
    test(
        R"(class std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::vector<int,class std::allocator<int> >,struct std::less<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > >,class std::allocator<struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class std::vector<int,class std::allocator<int> > > > >)",
        R"(std::map<std::string, std::vector<int>, std::less<std::string>>)"
    );
    return !success;
}
