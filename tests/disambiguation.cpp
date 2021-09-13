#include "assert.hpp"

#include <iostream>

int main() {
	std::pair<std::string, std::string> tests[] = {
		{"a < 1 == 2 > ( 1 + 3 )", "=="},
		{"a < 1 == 2 > - 3 == ( 1 + 3 )", "=="}, // <- disambiguated despite ambiguity
		{"( 1 + 3 ) == a < 1 == 2 > - 3", "=="}, // <- ambiguous
		{"( 1 + 3 ) == a < 1 == 2 > ()", "=="},
		{"a<x<x<x<x<x<x<x<x<x<1>>>>>>>>>", "<"},
		{"1 == something<a == b>>2", "=="}, // <- ambiguous
		{"1 == something<a == b>>2", "<"}, // <- should be an error
		{"1 < something<a < b>>2", "<"} // <- ambiguous
	};
	using namespace assert_impl_;
	for(auto [expression, target_op] : tests) {
		std::cout<<analysis::highlight(expression)<<" target: "<<target_op<<std::endl;
		auto [l, r] = analysis::decompose_expression(expression, target_op);
		std::cout<<"Final:"<<std::endl
		         <<"left:  "<<analysis::highlight(l)<<std::endl
		         <<"right: "<<analysis::highlight(r)<<std::endl<<std::endl;
	}
}
