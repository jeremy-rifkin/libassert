demo: demo.o foo.o
	g++ -std=c++17 -g demo.o foo.o -o demo.exe -Wall -Wextra
include/assert.hpp.gch: include/assert.hpp
	g++ -std=c++17 -g include/assert.hpp -o include/assert.hpp.gch -Wall -Wextra -D_0_ASSERT_DEMO
demo.o: demo.cpp include/assert.hpp.gch
	g++ -std=c++17 -g demo.cpp -c -o demo.o -Iinclude -D_0_ASSERT_DEMO -Wall -Wextra
foo.o: foo.cpp include/assert.hpp.gch
	g++ -std=c++17 -g foo.cpp  -c -o foo.o -Iinclude -D_0_ASSERT_DEMO -Wall -Wextra

tests/assert.hpp.gch: include/assert.hpp
	g++ -std=c++17 -g include/assert.hpp -o tests/assert.hpp.gch -Wall -Wextra -D_0_ASSERT_DEMO -D_0_DEBUG_ASSERT_DISAMBIGUATION
tests/disambiguation: tests/disambiguation.cpp tests/assert.hpp.gch
	g++ -Itests tests/disambiguation.cpp -o tests/disambiguation.exe -g -std=c++17 -D_0_ASSERT_DEMO -D_0_DEBUG_ASSERT_DISAMBIGUATION
tests/literals: tests/literals.cpp tests/assert.hpp.gch
	g++ -Itests tests/literals.cpp -o tests/literals.exe -g -std=c++17 -D_0_ASSERT_DEMO -D_0_DEBUG_ASSERT_DISAMBIGUATION
tests/tokens_and_highlighting: tests/tokens_and_highlighting.cpp tests/assert.hpp.gch
	g++ -Itests tests/tokens_and_highlighting.cpp -o tests/tokens_and_highlighting.exe -g -std=c++17 -D_0_ASSERT_DEMO -D_0_DEBUG_ASSERT_DISAMBIGUATION

.PHONY: tests clean everything

tests: tests/disambiguation tests/literals tests/tokens_and_highlighting

everything: demo tests

clean:
	rm demo.o foo.o tests/disambiguation tests/literals tests/tokens_and_highlighting include/assert.hpp.gch
