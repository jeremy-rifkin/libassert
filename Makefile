#C = clang++
C = g++

WFLAGS = -Wall -Wextra -Werror=return-type
FLAGS = -std=c++17 -g -Iinclude
ifeq ($(OS),Windows_NT)
	FLAGS += -ldbghelp
else
	FLAGS += -ldl
endif

demo.exe: demo.o foo.o
	$(C) demo.o foo.o -o demo.exe $(WFLAGS) $(FLAGS)
demo.o: demo.cpp include/assert.hpp
	$(C) demo.cpp -c -o demo.o $(WFLAGS) $(FLAGS) -D_0_ASSERT_DEMO
foo.o: foo.cpp include/assert.hpp
	$(C) foo.cpp  -c -o foo.o  $(WFLAGS) $(FLAGS) -D_0_ASSERT_DEMO

tests/disambiguation: tests/disambiguation.cpp include/assert.hpp
	$(C) -Itests tests/disambiguation.cpp -o tests/disambiguation.exe -Iinclude -g -std=c++17 $(WFLAGS) -D_0_ASSERT_DEMO -D_0_DEBUG_ASSERT_DISAMBIGUATION
tests/literals: tests/literals.cpp include/assert.hpp
	$(C) -Itests tests/literals.cpp -o tests/literals.exe -Iinclude -g -std=c++17 $(WFLAGS) -D_0_ASSERT_DEMO -D_0_DEBUG_ASSERT_DISAMBIGUATION
tests/tokens_and_highlighting: tests/tokens_and_highlighting.cpp include/assert.hpp
	$(C) -Itests tests/tokens_and_highlighting.cpp -o tests/tokens_and_highlighting.exe -Iinclude -g -std=c++17 $(WFLAGS) -D_0_ASSERT_DEMO -D_0_DEBUG_ASSERT_DISAMBIGUATION

.PHONY: tests clean everything

tests: tests/disambiguation tests/literals tests/tokens_and_highlighting

everything: demo tests

clean:
	rm demo.exe demo.o foo.o tests/disambiguation tests/literals tests/tokens_and_highlighting include/assert.hpp.gch demo.pdb demo.ilk
