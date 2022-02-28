# Defaults, can be overriden with invocation
COMPILER = g++
TARGET = release # release or debug

BIN = bin

MKDIR_P ?= mkdir -p

.PHONY: _all clean

ifneq ($(COMPILER),msvc)
    # GCC / Clang
    WFLAGS = -Wall -Wextra -Werror=return-type
    FLAGS = -std=c++17 -g -Iinclude
    ifneq ($(TARGET), debug)
        FLAGS += -O2
    endif
    LDFLAGS = -Wl,--whole-archive -Wl,--no-whole-archive
    CPP = $(COMPILER)
    LD = $(COMPILER)
    ifeq ($(OS),Windows_NT)
        STATIC_LIB = $(BIN)/assert.lib
        SHARED_LIB = $(BIN)/assert.dll
        LDFLAGS += -ldbghelp
    else
        STATIC_LIB = $(BIN)/libassert.a
        SHARED_LIB = $(BIN)/libassert.so
        LDFLAGS += -ldl
        PIC = -fPIC
        ifeq ($(TARGET), debug)
            FLAGS += -fsanitize=address
            LDFLAGS += -fsanitize=address
        endif
    endif

    _all: $(STATIC_LIB) $(SHARED_LIB)

    $(BIN)/src/assert.o: src/assert.cpp include/assert.hpp
		$(MKDIR_P) $(BIN)/src
		$(CPP) $< -c -o $@ $(WFLAGS) $(FLAGS) $(PIC)
    $(STATIC_LIB): $(BIN)/src/assert.o
		$(MKDIR_P) bin
		ar rcs $@ $^
    $(SHARED_LIB): $(BIN)/src/assert.o
		$(MKDIR_P) bin
		$(CPP) -shared $^ -o $@ $(WFLAGS) $(FLAGS) $(LDFLAGS)
else
    # MSVC
    # Note: Will need to manually run vcvarsall.bat x86_amd64
    CPP = cl
    LD = link
    WFLAGS = /W3
    FLAGS = /std:c++17 /EHsc
    ifneq ($(TARGET), debug)
        FLAGS += /O1
    endif
    LDFLAGS = /WHOLEARCHIVE # /PDB /OPT:ICF /OPT:REF
    STATIC_LIB = $(BIN)/assert.lib
    SHARED_LIB = $(BIN)/assert.dll

    _all: $(STATIC_LIB) $(SHARED_LIB)

    $(BIN)/src/assert.obj: src/assert.cpp include/assert.hpp
		$(MKDIR_P) $(BIN)/src
		cmd /c "$(CPP) /c /I include /Fo$@ $(WFLAGS) $(FLAGS) $<"
    $(STATIC_LIB): $(BIN)/src/assert.obj
		$(MKDIR_P) bin
		cmd /c "lib $^ /OUT:$@"
    $(SHARED_LIB): $(BIN)/src/assert.obj
		$(MKDIR_P) bin
		cmd /c "$(LD) /DLL $^ dbghelp.lib /OUT:$@ $(LDFLAGS)"
endif

clean:
	rm -rf $(BIN)
