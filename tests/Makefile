# Defaults, can be overriden with invocation
COMPILER = g++
TARGET = debug # release or debug

BIN = bin
SRC_DIRS := demo integration unit

SRCS := $(shell find $(SRC_DIRS) -name *.cpp)
OBJS := $(SRCS:%=$(BIN)/o/%.o)
DEPS := $(OBJS:.o=.d)

MKDIR_P ?= mkdir -p

.PHONY: clean _all

ifeq ($(OS),Windows_NT)
    STATIC_LIB = ../$(BIN)/assert.lib
    SHARED_LIB = ../$(BIN)/assert.dll
    DEMO = demo.exe
else
    STATIC_LIB = ../$(BIN)/libassert.a
    SHARED_LIB = ../$(BIN)/libassert.so
    DEMO = demo
endif

WFLAGS := -Wall -Wextra -Wvla
DFLAGS := -MMD -MP
CPPFLAGS := -std=c++17 -I../include -DASSERT_FAIL=custom_fail
LDFLAGS := -L../bin -lassert

HPP = ../include/assert.hpp

# target flags
ifeq ($(TARGET), debug)
	CPPFLAGS += -O0 -g
	ifneq ($(OS),Windows_NT)
		CPPFLAGS += -fsanitize=address
		LDFLAGS += -fsanitize=address
	endif
else ifeq ($(TARGET), release)
	CPPFLAGS += -O3 -funroll-loops -ffast-math -flto -march=native -DNDEBUG
	LDFLAGS += -flto -s
endif

CPPFLAGS += $(WFLAGS) $(DFLAGS)

MKDIR_P ?= mkdir -p

CPP = $(COMPILER)
LD = $(COMPILER)

_all: $(BIN)/$(DEMO) $(BIN)/disambiguation $(BIN)/literals $(BIN)/tokens_and_highlighting

$(BIN)/$(DEMO): $(BIN)/o/demo/demo.cpp.o $(BIN)/o/demo/bar.cpp.o $(BIN)/o/demo/foo.cpp.o $(BIN)/o/demo/baz/demo.cpp.o
	$(LD) $^ -o $@ $(LDFLAGS)

$(BIN)/disambiguation: $(BIN)/o/unit/disambiguation.cpp.o
	$(LD) $^ -o $@ $(LDFLAGS)

$(BIN)/literals: $(BIN)/o/unit/literals.cpp.o
	$(LD) $^ -o $@ $(LDFLAGS)

$(BIN)/tokens_and_highlighting: $(BIN)/o/unit/tokens_and_highlighting.cpp.o
	$(LD) $^ -o $@ $(LDFLAGS)

# c++ source
$(BIN)/o/%.cpp.o: %.cpp $(HPP)
	$(MKDIR_P) $(dir $@)
	$(CPP) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) -r $(BIN)

-include $(DEPS)
