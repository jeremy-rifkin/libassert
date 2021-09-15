#include "assert.hpp"

#include <iostream>

int main() {
	std::tuple<std::string, std::string, bool> tests[] = {
		{"a < 1 == 2 > ( 1 + 3 )", "==", true},
		{"a < 1 == 2 > - 3 == ( 1 + 3 )", "==", true}, // <- disambiguated despite ambiguity
		{"( 1 + 3 ) == a < 1 == 2 > - 3", "==", false}, // <- ambiguous
		{"( 1 + 3 ) == a < 1 == 2 > ()", "==", true},
		{"a<x<x<x<x<x<x<x<x<x<1>>>>>>>>>", "<", true},
		{"1 == something<a == b>>2", "==", false}, // <- ambiguous
		{"1 == something<a == b>>2", "<", false}, // <- should be an error
		{"1 < something<a < b>>2", "<", false}, // <- ambiguous
		{"1 < something<a < b>> - 2", "<", false} // <- ambiguous
	};
	using namespace assert_impl_;
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
