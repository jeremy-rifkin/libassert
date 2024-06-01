#include "platform.hpp"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>

#include "common.hpp"
#include "utils.hpp"

#if IS_WINDOWS
 #include <windows.h>
 #include <io.h>
 #undef min // fucking windows headers, man
 #undef max
#elif IS_LINUX
 #include <charconv>
 #include <fcntl.h>
 #include <sys/ioctl.h>
 #include <unistd.h>
#elif IS_APPLE
 #include <sys/ioctl.h>
 #include <sys/sysctl.h>
 #include <sys/types.h>
 #include <unistd.h>
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

    #if IS_LINUX
    class file_closer {
        int fd;
    public:
        file_closer(int fd_) : fd(fd_) {}
        file_closer(const file_closer&) = delete;
        file_closer(file_closer&&) = delete;
        file_closer& operator=(const file_closer&) = delete;
        file_closer& operator=(file_closer&&) = delete;
        ~file_closer() {
            if(close(fd) == -1) {
                perror("Libassert: Something went wrong when closing a file descriptor.");
            }
        }
    };
    #endif

	LIBASSERT_ATTR_COLD bool is_debugger_present_internal() noexcept {
        #if IS_WINDOWS
         return IsDebuggerPresent();
        #elif IS_LINUX
         // https://www.man7.org/linux/man-pages/man5/proc.5.html
         // We're looking at the top of /proc/self/status until we find "TracerPid:"
         // Sample:
         //  Name:   cat
         //  Umask:  0022
         //  State:  R (running)
         //  Tgid:   106085
         //  Ngid:   0
         //  Pid:    106085
         //  PPid:   104045
         //  TracerPid:      0
         // The name is truncated at 16 characters, so this whole thing should be under 256 chars
         constexpr std::size_t read_goal = 256;
         int fd = open("/proc/self/status", O_RDONLY);
         if(fd == -1) {
             // something went wrong
             return false;
         }
         file_closer closer(fd);
         char buffer[read_goal];
         auto size = read(fd, buffer, read_goal);
         if(size == -1) {
             // something went wrong
             return false;
         }
         std::string_view status{buffer, std::size_t(size)};
         constexpr std::string_view key = "TracerPid:";
         auto pos = status.find(key);
         if(pos == std::string_view::npos) {
             return false;
         }
         auto pid_start = status.find_first_not_of(detail::whitespace_chars, pos + key.size());
         if(pid_start == std::string_view::npos) {
             return false;
         }
         auto pid_end = status.find_first_of(detail::whitespace_chars, pid_start);
         if(pid_end == std::string_view::npos) {
             return false;
         }
         // since we found a whitespace character after the pid we know the read wasn't somehow weirdly truncated
         auto tracer_pid = status.substr(pid_start, pid_end - pid_start);
         int value;
         if(std::from_chars(tracer_pid.data(), tracer_pid.data() + tracer_pid.size(), value).ec == std::errc{}) {
             return value != 0;
         } else {
             return false;
         }
         return false;
        #else
         // https://developer.apple.com/library/archive/qa/qa1361/_index.html
         int mib[4] {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
         struct kinfo_proc info;
         info.kp_proc.p_flag = 0;
         size_t size = sizeof(info);
         int res = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
         if(res != 0) {
             // something went wrong, assume false
             return false;
         }
         return info.kp_proc.p_flag & P_TRACED;
        #endif
    }

    std::atomic<debugger_check_mode> check_mode = debugger_check_mode::check_once;
    std::mutex is_debugger_present_mutex;
    std::optional<bool> cached_is_debugger_present;

    LIBASSERT_ATTR_COLD
    bool is_debugger_present() noexcept {
        if(check_mode.load() == debugger_check_mode::check_every_time) {
            return is_debugger_present_internal();
        } else {
            std::unique_lock lock(is_debugger_present_mutex);
            if(cached_is_debugger_present) {
                return *cached_is_debugger_present;
            } else {
                cached_is_debugger_present = is_debugger_present_internal();
                return *cached_is_debugger_present;
            }
        }
    }

    LIBASSERT_ATTR_COLD LIBASSERT_EXPORT
    void set_debugger_check_mode(debugger_check_mode mode) noexcept {
        check_mode = mode;
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
        return strerror(e);
    }
}
