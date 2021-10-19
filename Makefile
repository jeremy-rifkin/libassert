C = clang++
#C = g++

WFLAGS = -Wall -Wextra -Werror=return-type
FLAGS = -std=c++17 -g -Iinclude -D_0_ASSERT_DEMO -ftime-trace # -DASSERT_INTERNAL_DEBUG
LIBFLAGS = $(FLAGS) -O2
PIC =
ifeq ($(OS),Windows_NT)
	DEMO = demo.exe
	STATIC_LIB = assert.lib
	SHARED_LIB = assert.dll
	LDFLAGS = -ldbghelp
else
	DEMO = demo
	STATIC_LIB = libassert.a
	SHARED_LIB = libassert.so
	LDFLAGS = -ldl
	PIC = -fPIC
endif

.PHONY: _all

_all: $(STATIC_LIB) $(SHARED_LIB)

$(DEMO): demo.o foo.o bar.o $(SHARED_LIB)
	$(C) demo.o foo.o bar.o -L. -lassert -o $(DEMO) $(WFLAGS) $(FLAGS)
demo.o: demo.cpp include/assert.hpp
	$(C) demo.cpp -c -o demo.o $(WFLAGS) $(FLAGS)
foo.o: foo.cpp include/assert.hpp
	$(C) foo.cpp  -c -o foo.o  $(WFLAGS) $(FLAGS)
bar.o: bar.cpp include/assert.hpp
	$(C) bar.cpp  -c -o bar.o  $(WFLAGS) $(FLAGS)
src/assert.o: src/assert.cpp include/assert.hpp
	# optimizing here improves link substantially, and this only needs to be built once
	$(C) src/assert.cpp -c -o src/assert.o $(WFLAGS) $(LIBFLAGS)
$(STATIC_LIB): src/assert.o
	ar rcs $(STATIC_LIB) src/assert.o
$(SHARED_LIB): src/assert.cpp
	$(C) -shared src/assert.cpp -o $(SHARED_LIB) $(WFLAGS) $(LIBFLAGS) $(PIC) -Wl,--whole-archive -Wl,--no-whole-archive $(LDFLAGS)

tests/disambiguation: tests/disambiguation.cpp $(SHARED_LIB)
	$(C) -Itests tests/disambiguation.cpp -o tests/disambiguation.exe $(FLAGS) $(WFLAGS) -D_0_DEBUG_ASSERT_DISAMBIGUATION
tests/literals: tests/literals.cpp $(SHARED_LIB)
	$(C) -Itests tests/literals.cpp -o tests/literals.exe -Iinclude -g -std=c++17 -L. -lassert $(WFLAGS) -D_0_DEBUG_ASSERT_DISAMBIGUATION
tests/tokens_and_highlighting: tests/tokens_and_highlighting.cpp $(SHARED_LIB)
	$(C) -Itests tests/tokens_and_highlighting.cpp -o tests/tokens_and_highlighting.exe -Iinclude -g -std=c++17 -L. -lassert $(WFLAGS) -D_0_DEBUG_ASSERT_DISAMBIGUATION

.PHONY: tests clean

tests: tests/disambiguation tests/literals tests/tokens_and_highlighting

clean:
	rm $(DEMO) *.so *.dll *.lib *.a demo.o foo.o bar.o src/assert.o tests/disambiguation tests/literals tests/tokens_and_highlighting tests/*.pdb tests/*.ilk demo.pdb demo.ilk
