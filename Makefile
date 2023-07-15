# Defaults, can be overriden with invocation
COMPILER = g++
# release or debug
TARGET = release
STD = c++17

BIN = bin

MKDIR_P ?= mkdir -p

.PHONY: _all clean

ifeq ($(OS),Windows_NT)
    GFLAG = -g
else
    GFLAG = -g -gdwarf-4
endif

ifneq ($(COMPILER),msvc)
    # GCC / Clang
    CPP = $(COMPILER)
    LD = $(COMPILER)
    override WFLAGS += -Wall -Wextra -Wvla -Wshadow -Werror -Wundef
    override FLAGS += -std=$(STD) $(GFLAG) -Iinclude
    override LDFLAGS += -Wl,--whole-archive -Wl,--no-whole-archive
    ifeq ($(TARGET), debug)
        override FLAGS += $(GFLAG)
    else
        override FLAGS += -O2
    endif
    ifeq ($(OS),Windows_NT)
        STATIC_LIB = $(BIN)/assert.lib
        SHARED_LIB = $(BIN)/assert.dll
        override LDFLAGS += -ldbghelp
    else
        STATIC_LIB = $(BIN)/libassert.a
        SHARED_LIB = $(BIN)/libassert.so
        override LDFLAGS += -ldl
        PIC = -fPIC
        ifeq ($(TARGET), debug)
            override FLAGS += -fsanitize=address
            override LDFLAGS += -fsanitize=address
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
    SHELL = powershell
    override WFLAGS += /W4 /WX /wd4668
    override FLAGS += /std:$(STD) /EHsc
    override LDFLAGS += /WHOLEARCHIVE # /PDB /OPT:ICF /OPT:REF
    ifeq ($(TARGET), debug)
        override FLAGS += /Z7
        override LDFLAGS += /DEBUG
    else
        override FLAGS += /O1
    endif
    STATIC_LIB = $(BIN)/assert.lib
    SHARED_LIB = $(BIN)/assert.dll

    _all: $(STATIC_LIB) $(SHARED_LIB)

    $(BIN)/src/assert.obj: src/assert.cpp include/assert.hpp
		-$(MKDIR_P) $(BIN)/src
		$(CPP) /c /I include /Fo$@ $(WFLAGS) $(FLAGS) $<
    $(STATIC_LIB): $(BIN)/src/assert.obj
		-$(MKDIR_P) bin
		lib $^ /OUT:$@
    $(SHARED_LIB): $(BIN)/src/assert.obj
		-$(MKDIR_P) bin
		$(LD) /DLL $^ dbghelp.lib /OUT:$@ $(LDFLAGS)
endif

clean:
	rm -rf $(BIN)
