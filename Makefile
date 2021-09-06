demo: demo.cpp foo.cpp include/assert.hpp
	g++ -std=c++17 -g demo.cpp foo.cpp -Iinclude -o demo.exe -DASSERT_DEMO -Wall -Wextra
