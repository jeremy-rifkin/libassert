demo: demo.o foo.o
	g++ -std=c++17 -g demo.o foo.o -o demo.exe -Wall -Wextra -ftime-report
demo.o: demo.cpp include/assert.hpp
	g++ -std=c++17 -g demo.cpp -c -o demo.o -Iinclude -DASSERT_DEMO -Wall -Wextra -ftime-report
foo.o: foo.cpp include/assert.hpp
	g++ -std=c++17 -g foo.cpp  -c -o foo.o -Iinclude -DASSERT_DEMO -Wall -Wextra -ftime-report
