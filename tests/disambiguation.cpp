#include "assert.hpp"

#include <iostream>

#define ESC "\033["
#define RED ESC "1;31m"
#define GREEN ESC "1;32m"
#define BLUE ESC "1;34m"
#define CYAN ESC "1;36m"
#define PURPL ESC "1;35m"
#define DARK ESC "1;30m"
#define RESET ESC "0m"

int main() {
	std::tuple<std::string, std::string_view, bool> tests[] = {
		{"a < 1 == 2 > ( 1 + 3 )", "==", true},
		{"a < 1 == 2 > - 3 == ( 1 + 3 )", "==", true}, // <- disambiguated despite ambiguity
		{"( 1 + 3 ) == a < 1 == 2 > - 3", "==", false}, // <- ambiguous
		{"( 1 + 3 ) == a < 1 == 2 > ()", "==", true},
		{"( 1 + 3 ) not_eq a < 1 not_eq 2 > ()", "!=", true},
		{"a<x<x<x<x<x<x<x<x<x<1>>>>>>>>>", "<", true},
		{"a<x<x<x<x<x<x<x<x<x<x<1>>>>>>>>>>", "<", false}, // <- max depth exceeded
		{"1 == something<a == b>>2", "==", false}, // <- ambiguous
		{"1 == something<a == b>>2", "<", false}, // <- should be an error
		{"1 < something<a < b>>2", "<", false}, // <- ambiguous
		{"1 < something<a < b>> - 2", "<", false}, // <- ambiguous
		{"18446744073709551606ULL == -10", "==", true}
	};
	using namespace assert_detail;
	for(auto [expression, target_op, should_disambiguate] : tests) {
		std::cout<<analysis::highlight(expression)<<" target: "<<target_op<<std::endl;
		auto [l, r] = analysis::decompose_expression(expression, target_op);
		std::cout<<"Final:"<<std::endl
		         <<"left:  "<<analysis::highlight(l)<<std::endl
		         <<"right: "<<analysis::highlight(r)<<std::endl<<std::endl;
		bool disambiguated = !(l == "left" && r == "right");
		if(should_disambiguate == disambiguated) {
			std::cout<<GREEN "Passed" RESET<<std::endl;
		} else {
			std::cout<<RED "Failed" RESET<<std::endl;
		}
	}
}
