#C = clang++
C = g++-10

demo: demo.o foo.o
	$(C) -std=c++17 -g demo.o foo.o -o demo.exe -Wall -Wextra -ldl #-rdynamic #-ldbghelp
demo.o: demo.cpp include/assert.hpp
	$(C) -std=c++17 -g demo.cpp -c -o demo.o -Iinclude -D_0_ASSERT_DEMO -Wall -Wextra
foo.o: foo.cpp include/assert.hpp
	$(C) -std=c++17 -g foo.cpp  -c -o foo.o -Iinclude -D_0_ASSERT_DEMO -Wall -Wextra

tests/disambiguation: tests/disambiguation.cpp include/assert.hpp
	$(C) -Itests tests/disambiguation.cpp -o tests/disambiguation.exe -Iinclude -g -std=c++17 -Wall -Wextra -D_0_ASSERT_DEMO -D_0_DEBUG_ASSERT_DISAMBIGUATION
tests/literals: tests/literals.cpp include/assert.hpp
	$(C) -Itests tests/literals.cpp -o tests/literals.exe -Iinclude -g -std=c++17 -Wall -Wextra -D_0_ASSERT_DEMO -D_0_DEBUG_ASSERT_DISAMBIGUATION
tests/tokens_and_highlighting: tests/tokens_and_highlighting.cpp include/assert.hpp
	$(C) -Itests tests/tokens_and_highlighting.cpp -o tests/tokens_and_highlighting.exe -Iinclude -g -std=c++17 -Wall -Wextra -D_0_ASSERT_DEMO -D_0_DEBUG_ASSERT_DISAMBIGUATION

.PHONY: tests clean everything

tests: tests/disambiguation tests/literals tests/tokens_and_highlighting

everything: demo tests

clean:
	rm demo.o foo.o tests/disambiguation tests/literals tests/tokens_and_highlighting include/assert.hpp.gch
