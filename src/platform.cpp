#include "platform.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include "common.hpp"
#include "utils.hpp"

#if IS_WINDOWS
 #include <windows.h>
 #include <io.h>
 #undef min // fucking windows headers, man
 #undef max
#elif defined(__linux) || defined(__APPLE__) || defined(__unix__)
 #include <sys/ioctl.h>
 #include <unistd.h>
 // NOLINTNEXTLINE(misc-include-cleaner)
 #include <climits> // MAX_PATH
#else
 #error "Libassert doesn't recognize this system, please open an issue at https://github.com/jeremy-rifkin/libassert"
#endif

#include <libassert/assert.hpp>

// All platform-specific/system code lives here

namespace libassert {
    // https://stackoverflow.com/questions/23369503/get-size-of-terminal-window-rows-columns
    LIBASSERT_ATTR_COLD int terminal_width(int fd) {
        if(fd < 0) {
            return 0;
        }
        #if IS_WINDOWS
         DWORD windows_handle = detail::needle(fd).lookup(
             STDIN_FILENO, STD_INPUT_HANDLE,
             STDOUT_FILENO, STD_OUTPUT_HANDLE,
             STDERR_FILENO, STD_ERROR_HANDLE
         );
         CONSOLE_SCREEN_BUFFER_INFO csbi;
         HANDLE h = GetStdHandle(windows_handle);
         if(h == INVALID_HANDLE_VALUE) { return 0; }
         if(!GetConsoleScreenBufferInfo(h, &csbi)) { return 0; }
         return csbi.srWindow.Right - csbi.srWindow.Left + 1;
        #else
         struct winsize w;
         // NOLINTNEXTLINE(misc-include-cleaner)
         if(ioctl(fd, TIOCGWINSZ, &w) == -1) { return 0; }
         return w.ws_col;
        #endif
    }

    LIBASSERT_ATTR_COLD LIBASSERT_EXPORT void enable_virtual_terminal_processing_if_needed() {
        // enable colors / ansi processing if necessary
        #if IS_WINDOWS
         // https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences#example-of-enabling-virtual-terminal-processing
         #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
          constexpr DWORD ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x4;
         #endif
         HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
         DWORD dwMode = 0;
         if(hOut == INVALID_HANDLE_VALUE) return;
         if(!GetConsoleMode(hOut, &dwMode)) return;
         if(dwMode != (dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
         if(!SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) return;
        #endif
    }

    LIBASSERT_ATTR_COLD bool isatty(int fd) {
        #if IS_WINDOWS
         return _isatty(fd);
        #else
         return ::isatty(fd);
        #endif
    }
}

namespace libassert::detail {
    std::mutex strerror_mutex;

    LIBASSERT_ATTR_COLD std::string strerror_wrapper(int e) {
        std::unique_lock lock(strerror_mutex);
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        return strerror(e);
    }
}
