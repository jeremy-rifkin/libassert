# Defaults, can be overriden with invocation
COMPILER = g++
# release or debug
TARGET = debug
STD=c++17

BIN = bin
SRC_DIRS := demo integration unit

SRCS := $(shell find $(SRC_DIRS) -name *.cpp)
OBJS := $(SRCS:%=$(BIN)/o/%.o)
DEPS := $(OBJS:.o=.d)

MKDIR_P ?= mkdir -p

.PHONY: clean _all demo tests

ifeq ($(OS),Windows_NT)
    STATIC_LIB = ../$(BIN)/assert.lib
    SHARED_LIB = ../$(BIN)/assert.dll
    E = .exe
else
    STATIC_LIB = ../$(BIN)/libassert.a
    SHARED_LIB = ../$(BIN)/libassert.so
    E =
endif

HPP = ../include/assert.hpp

_all: tests demo basic_test integration

demo: $(BIN)/demo$(E)

basic_test: $(BIN)/basic_test$(E)

tests: $(BIN)/disambiguation$(E) $(BIN)/literals$(E) $(BIN)/tokens_and_highlighting$(E) $(BIN)/type_handling$(E) $(BIN)/basic_test$(E) $(BIN)/integration$(E)

integration: $(BIN)/integration$(E)

ifneq ($(COMPILER),msvc)
    CPP = $(COMPILER)
    LD = $(COMPILER)
    override WFLAGS += -Wall -Wextra -Wvla -Wshadow -Werror
    override DFLAGS += -MMD -MP
    override CPPFLAGS += -std=$(STD) -I../include
    override GENERAL_FLAGS += -DASSERT_USE_MAGIC_ENUM -DASSERT_FAIL=custom_fail -DASSERT_LOWERCASE
    override LDFLAGS += -g -L../bin -lassert
    ifeq ($(TARGET),debug)
        override CPPFLAGS += -O0 -g
        ifneq ($(OS),Windows_NT)
            override CPPFLAGS += -fsanitize=address
            override LDFLAGS += -fsanitize=address
        endif
    else
        override CPPFLAGS += -O3 -funroll-loops -ffast-math -flto -march=native -DNDEBUG
        override LDFLAGS += -flto -s
    endif
    ifeq ($(OS),Windows_NT)
        override LDFLAGS += -ldbghelp
    endif
    override CPPFLAGS += $(WFLAGS) $(DFLAGS)

    $(BIN)/demo$(E): $(BIN)/o/demo/demo.cpp.o $(BIN)/o/demo/bar.cpp.o $(BIN)/o/demo/foo.cpp.o $(BIN)/o/demo/baz/demo.cpp.o
		$(LD) $^ -o $@ $(LDFLAGS)

    $(BIN)/disambiguation$(E): $(BIN)/o/unit/disambiguation.cpp.o
		$(LD) $^ -o $@ $(LDFLAGS)

    $(BIN)/literals$(E): $(BIN)/o/unit/literals.cpp.o
		$(LD) $^ -o $@ $(LDFLAGS)

    $(BIN)/tokens_and_highlighting$(E): $(BIN)/o/unit/tokens_and_highlighting.cpp.o
		$(LD) $^ -o $@ $(LDFLAGS)

    $(BIN)/type_handling$(E): $(BIN)/o/unit/type_handling.cpp.o
		$(LD) $^ -o $@ $(LDFLAGS)

    $(BIN)/basic_test$(E): $(BIN)/o/unit/basic_test.cpp.o
		$(LD) $^ -o $@ $(LDFLAGS)

    $(BIN)/integration$(E): $(BIN)/o/integration/integration.cpp.o $(BIN)/o/integration/a.cpp.o $(BIN)/o/integration/x/a.cpp.o
		$(LD) $^ -o $@ $(LDFLAGS)

    $(BIN)/o/unit/basic_test.cpp.o: unit/basic_test.cpp $(HPP)
		$(MKDIR_P) $(dir $@)
		echo "$(TARGET)"
		$(CPP) $(CPPFLAGS) -c $< -o $@

    $(BIN)/o/%.cpp.o: %.cpp $(HPP)
		$(MKDIR_P) $(dir $@)
		echo "$(TARGET)"
		$(CPP) $(CPPFLAGS) $(GENERAL_FLAGS) -c $< -o $@
else
    CPP = cl
    LD = link
    SHELL = powershell
    override WFLAGS += /W4 /WX
    override CPPFLAGS += /std:$(STD) /EHsc /I ../include /Z7
    override GENERAL_FLAGS += /DASSERT_USE_MAGIC_ENUM /DASSERT_FAIL=custom_fail /DASSERT_LOWERCASE
    override LDFLAGS += /LIBPATH:../bin assert.lib dbghelp.lib /DEBUG
    ifeq ($(TARGET), debug)
        override CPPFLAGS +=
    else
        override CPPFLAGS += /O2 -DNDEBUG
        override LDFLAGS +=
    endif
    override CPPFLAGS += $(WFLAGS) $(DFLAGS)

    $(BIN)/demo$(E): $(BIN)/o/demo/demo.cpp.o $(BIN)/o/demo/bar.cpp.o $(BIN)/o/demo/foo.cpp.o $(BIN)/o/demo/baz/demo.cpp.o
		$(LD) $^ /OUT:$@ $(LDFLAGS)

    $(BIN)/disambiguation$(E): $(BIN)/o/unit/disambiguation.cpp.o
		$(LD) $^ /OUT:$@ $(LDFLAGS)

    $(BIN)/literals$(E): $(BIN)/o/unit/literals.cpp.o
		$(LD) $^ /OUT:$@ $(LDFLAGS)

    $(BIN)/tokens_and_highlighting$(E): $(BIN)/o/unit/tokens_and_highlighting.cpp.o
		$(LD) $^ /OUT:$@ $(LDFLAGS)

    $(BIN)/type_handling$(E): $(BIN)/o/unit/type_handling.cpp.o
		$(LD) $^ /OUT:$@ $(LDFLAGS)

    $(BIN)/basic_test$(E): $(BIN)/o/unit/basic_test.cpp.o
		$(LD) $^ /OUT:$@ $(LDFLAGS)

    $(BIN)/integration$(E): $(BIN)/o/integration/integration.cpp.o $(BIN)/o/integration/a.cpp.o $(BIN)/o/integration/x/a.cpp.o
		$(LD) $^ /OUT:$@ $(LDFLAGS)

    $(BIN)/o/unit/basic_test.cpp.o: unit/basic_test.cpp $(HPP)
		-$(MKDIR_P) $(dir $@)
		$(CPP) $(CPPFLAGS) /c $< /Fo$@

    $(BIN)/o/%.cpp.o: %.cpp $(HPP)
		-$(MKDIR_P) $(dir $@)
		$(CPP) $(CPPFLAGS) $(GENERAL_FLAGS) /c $< /Fo$@
endif

clean:
	$(RM) -r $(BIN)

-include $(DEPS)
