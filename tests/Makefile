# Defaults, can be overriden with invocation
COMPILER = g++
# release or debug
TARGET = debug

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

_all: tests demo

demo: $(BIN)/demo$(E)

tests: $(BIN)/disambiguation$(E) $(BIN)/literals$(E) $(BIN)/tokens_and_highlighting$(E) $(BIN)/integration$(E)

ifneq ($(COMPILER),msvc)
    CPP = $(COMPILER)
    LD = $(COMPILER)
    WFLAGS := -Wall -Wextra -Wvla
    DFLAGS := -MMD -MP
    CPPFLAGS := -std=c++17 -I../include -DASSERT_FAIL=custom_fail
    LDFLAGS := -L../bin -lassert
    ifeq ($(TARGET),debug)
        CPPFLAGS += -O0 -g
        ifneq ($(OS),Windows_NT)
            CPPFLAGS += -fsanitize=address
            LDFLAGS += -fsanitize=address
        endif
    else
        CPPFLAGS += -O3 -funroll-loops -ffast-math -flto -march=native -DNDEBUG
        LDFLAGS += -flto -s
    endif
    CPPFLAGS += $(WFLAGS) $(DFLAGS)

    $(BIN)/demo$(E): $(BIN)/o/demo/demo.cpp.o $(BIN)/o/demo/bar.cpp.o $(BIN)/o/demo/foo.cpp.o $(BIN)/o/demo/baz/demo.cpp.o
		$(LD) $^ -o $@ $(LDFLAGS)

    $(BIN)/disambiguation$(E): $(BIN)/o/unit/disambiguation.cpp.o
		$(LD) $^ -o $@ $(LDFLAGS)

    $(BIN)/literals$(E): $(BIN)/o/unit/literals.cpp.o
		$(LD) $^ -o $@ $(LDFLAGS)

    $(BIN)/tokens_and_highlighting$(E): $(BIN)/o/unit/tokens_and_highlighting.cpp.o
		$(LD) $^ -o $@ $(LDFLAGS)

    $(BIN)/integration$(E): $(BIN)/o/integration/integration.cpp.o $(BIN)/o/integration/a.cpp.o $(BIN)/o/integration/x/a.cpp.o
		$(LD) $^ -o $@ $(LDFLAGS)

    $(BIN)/o/%.cpp.o: %.cpp $(HPP)
		$(MKDIR_P) $(dir $@)
		echo "$(TARGET)"
		$(CPP) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
else
    CPP = cl
    LD = link
    SHELL = powershell
    WFLAGS := /W4
    CPPFLAGS := /std:c++17 /EHsc /I ../include /DASSERT_FAIL=custom_fail /Z7
    LDFLAGS := /LIBPATH:../bin assert.lib dbghelp.lib /DEBUG
    ifeq ($(TARGET), debug)
        CPPFLAGS +=
    else
        CPPFLAGS += /O2 -DNDEBUG
        LDFLAGS +=
    endif
    CPPFLAGS += $(WFLAGS) $(DFLAGS)

    $(BIN)/demo$(E): $(BIN)/o/demo/demo.cpp.o $(BIN)/o/demo/bar.cpp.o $(BIN)/o/demo/foo.cpp.o $(BIN)/o/demo/baz/demo.cpp.o
		$(LD) $^ /OUT:$@ $(LDFLAGS)

    $(BIN)/disambiguation$(E): $(BIN)/o/unit/disambiguation.cpp.o
		$(LD) $^ /OUT:$@ $(LDFLAGS)

    $(BIN)/literals$(E): $(BIN)/o/unit/literals.cpp.o
		$(LD) $^ /OUT:$@ $(LDFLAGS)

    $(BIN)/tokens_and_highlighting$(E): $(BIN)/o/unit/tokens_and_highlighting.cpp.o
		$(LD) $^ /OUT:$@ $(LDFLAGS)

    $(BIN)/integration$(E): $(BIN)/o/integration/integration.cpp.o $(BIN)/o/integration/a.cpp.o $(BIN)/o/integration/x/a.cpp.o
		$(LD) $^ /OUT:$@ $(LDFLAGS)

    $(BIN)/o/%.cpp.o: %.cpp $(HPP)
		-$(MKDIR_P) $(dir $@)
		$(CPP) $(CPPFLAGS) $(CXXFLAGS) /c $< /Fo$@
endif

clean:
	$(RM) -r $(BIN)

-include $(DEPS)
