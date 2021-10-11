#ifndef ASSERT_HPP
#define ASSERT_HPP

// Jeremy Rifkin 2021
// https://github.com/jeremy-rifkin/asserts

#include <bitset>
#include <iomanip>
#include <limits>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string_view>
#include <string.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

#if defined(__clang__)
 #define IS_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
 #define IS_GCC 1
#else
 #error "no"
#endif
#if defined(_WIN32)
 #define IS_WINDOWS 1
#elif defined(__linux)
 #define IS_LINUX 1
#else
 #error "no"
#endif

#if IS_WINDOWS
 #include <windows.h>
 #include <conio.h>
 #include <dbghelp.h>
 #include <io.h>
 #include <process.h>
 #ifndef STDIN_FILENO
  #define STDIN_FILENO _fileno(stdin)
  #define STDOUT_FILENO _fileno(stdout)
  #define STDERR_FILENO _fileno(stderr)
 #endif
 #undef min // fucking windows headers man
 #undef max
 #define USE_DBG_HELP_H
#else
 #include <dlfcn.h>
 #include <execinfo.h>
 #include <limits.h>
 #include <sys/ioctl.h>
 #include <sys/stat.h>
 #include <sys/types.h>
 #include <sys/wait.h>
 #include <termios.h>
 #include <unistd.h>
 #define USE_EXECINFO_H
#endif

///#include <iostream>

#ifndef NCOLOR
 #define ESC "\033["
 #define ANSIRGB(r, g, b) ESC "38;2;" #r ";" #g ";" #b "m"
 #define RED ANSIRGB(224, 107, 116)
 #define ORANGE ANSIRGB(209, 154, 102)
 #define YELLOW ANSIRGB(229, 192, 122)
 #define GREEN ANSIRGB(152, 195, 121)
 #define BLUE ANSIRGB(98, 174, 239)
 #define CYAN ANSIRGB(85, 182, 194)
 #define PURPL ANSIRGB(198, 120, 221)
 #define RESET ESC "0m"
#else
 #define RED ""
 #define ORANGE ""
 #define YELLOW ""
 #define GREEN ""
 #define BLUE ""
 #define CYAN ""
 #define PURPL ""
 #define RESET ""
#endif

namespace assert_impl_ {
	// Lightweight helper, eventually may use C++20 std::source_location if this library no longer
	// targets C++17. Right now std::source_location contains the method signature for a function
	// while __builtin_FUNCTION is just the name (hence why __PRETTY_FUNCTION__ is used later on).
	struct source_location {
		const char* const file;
		const char* const function;
		const int line;
		constexpr source_location(
			const char* file     = __builtin_FILE(),
			const char* function = __builtin_FUNCTION(),
			int line             = __builtin_LINE()
		) : file(file), function(function), line(line) {}
	};

	[[gnu::cold]]
	static void primitive_assert_impl(bool c, const char* expression, const char* message = nullptr, source_location location = {}) {
		if(!c) {
			if(message == nullptr) {
				fprintf(stderr, "Assertion failed at %s:%d: %s\n", location.file, location.line, location.function);
			} else {
				fprintf(stderr, "Assertion failed at %s:%d: %s: %s\n", location.file, location.line, location.function, message);
			}
			fprintf(stderr, "    assert(%s);\n", expression);
			abort();
		}
	}

	#define primitive_assert(c, ...) primitive_assert_impl(c, #c, ##__VA_ARGS__)

	// string utilities
	template<typename... T>
	[[gnu::cold]]
	std::string stringf(T... args) {
		int length = snprintf(0, 0, args...);
		if(length < 0) primitive_assert(false, "Invalid arguments to stringf");
		std::string str(length, 0);
		snprintf(str.data(), length + 1, args...);
		return str;
	}

	[[gnu::cold]] [[maybe_unused]]
	static std::vector<std::string> split(std::string_view s, std::string_view delim) {
		std::vector<std::string> vec;
		size_t old_pos = 0;
		size_t pos = 0;
		std::string token;
		while((pos = s.find(delim, old_pos)) != std::string::npos) {
				token = s.substr(old_pos, pos - old_pos);
				vec.push_back(token);
				old_pos = pos + delim.length();
		}
		vec.push_back(std::string(s.substr(old_pos)));
		return vec;
	}

	template<typename C>
	[[gnu::cold]]
	static std::string join(const C& container, const std::string_view delim) {
		auto iter = std::begin(container);
		auto end = std::end(container);
		std::string str;
		if(std::distance(iter, end) > 0) {
			str += *iter;
			while(++iter != end) {
				str += delim;
				str += *iter;
			}
		}
		return str;
	}

	constexpr const char * const ws = " \t\n\r\f\v";

	[[gnu::cold]]
	static std::string_view trim(const std::string_view s) {
		size_t l = s.find_first_not_of(ws);
		size_t r = s.find_last_not_of(ws) + 1;
		return s.substr(l, r - l);
	}

	template<size_t N>
	[[gnu::cold]]
	std::optional<std::array<std::string, N>> match(const std::string s, const std::regex& r) {
		std::smatch match;
		if(std::regex_match(s, match, r)) {
			std::array<std::string, N> arr;
			for(size_t i = 0; i < N; i++) {
				arr[i] = match[i + 1].str();
			}
			return arr;
		} else {
			return {};
		}
	}

	[[gnu::cold]] [[maybe_unused]]
	static void enable_virtual_terminal_processing_if_needed() {
		// enable colors / ansi processing if necessary
		#ifdef _WIN32
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

	#ifdef _WIN32
	typedef int pid_t;
	#endif
	[[gnu::cold]] [[maybe_unused]] static pid_t getpid() {
		#ifdef _WIN32
		return _getpid();
		#else
		return ::getpid();
		#endif
	}

	[[gnu::cold]] [[maybe_unused]] static void wait_for_keypress() {
		#ifdef _WIN32
		_getch();
		#else
		struct termios tty_attr;
        tcgetattr(0, &tty_attr);
		struct termios tty_attr_original = tty_attr;
        tty_attr.c_lflag &= ~(ICANON | ECHO);
        tty_attr.c_cc[VTIME] = 0;
        tty_attr.c_cc[VMIN] = 1;
        tcsetattr(0, TCSAFLUSH, &tty_attr); // formerly used TCSANOW
		char c;
		read(STDIN_FILENO, &c, 1);
        tcsetattr(0, TCSAFLUSH, &tty_attr_original);
		#endif
	}

	[[gnu::cold]] [[maybe_unused]] static bool isatty(int fd) {
		#ifdef _WIN32
		return _isatty(fd);
		#else
		return ::isatty(fd);
		#endif
	}

	// https://stackoverflow.com/questions/23369503/get-size-of-terminal-window-rows-columns
	[[gnu::cold]] [[maybe_unused]] static int terminal_width() {
		#ifdef _WIN32
		 CONSOLE_SCREEN_BUFFER_INFO csbi;
		 HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
		 if(h == INVALID_HANDLE_VALUE) return 0;
		 if(!GetConsoleScreenBufferInfo(h, &csbi)) return 0;
		 return csbi.srWindow.Right - csbi.srWindow.Left + 1;
		#else
		 struct winsize w;
    	 if(ioctl(STDERR_FILENO, TIOCGWINSZ, &w) == -1) return 0;
		 return w.ws_col;
		#endif
	}

	[[gnu::cold]] [[maybe_unused]] static std::string get_executable_path() {
		#ifdef _WIN32
		 char buffer[MAX_PATH + 1];
		 int s = GetModuleFileNameA(NULL, buffer, sizeof(buffer));
		 primitive_assert(s != 0);
		 return buffer;
		#else
		 char buffer[PATH_MAX + 1];
		 ssize_t s = readlink("/proc/self/exe", buffer, PATH_MAX);
		 primitive_assert(s != -1);
		 buffer[s] = 0;
		 return buffer;
		#endif
	}

	struct stacktrace_entry {
		std::string filename;
		std::string signature;
		int line = 0;
		bool operator==(const stacktrace_entry& other) const {
			return line == other.line && signature == other.signature && filename == other.filename;
		}
		bool operator!=(const stacktrace_entry& other) const {
			return !operator==(other);
		}
	};
	
	// ... -> print_stacktrace -> get_stacktrace
	// get 100, skip at least 2
	constexpr size_t n_frames = 100;
	constexpr size_t n_skip   = 2;

	// based on https://blog.aaronballman.com/2011/04/generating-a-stack-crawl/
	// TODO: I'd like to find a way to get a full signature for symbols, at the moment this only
	// produces name and template parameters, not other parameters. I do not know if this is
	// possible, I haven't found any library or example that can extract this info.
	#ifdef USE_DBG_HELP_H
	[[gnu::cold]] [[maybe_unused]]
	static std::string resolve_addresses(const std::string& addresses, const std::string& executable) {
		// TODO: Popen is a hack. Implement properly with CreateProcess and pipes later.
		FILE* p = popen(("addr2line -e " + executable + " -fC " + addresses).c_str(), "r");
		std::string output;
		constexpr int buffer_size = 4096;
		char buffer[buffer_size];
		size_t count = 0;
		while((count = fread(buffer, 1, buffer_size, p)) > 0) {
			output.insert(output.end(), buffer, buffer + count);
		}
		return output;
	}

	[[maybe_unused]] static std::optional<std::vector<stacktrace_entry>> get_stacktrace() {
		SymSetOptions(
			  SYMOPT_DEFERRED_LOADS
			| SYMOPT_INCLUDE_32BIT_MODULES
		//	| SYMOPT_NO_PUBLICS
		//	| SYMOPT_UNDNAME
		    | SYMOPT_AUTO_PUBLICS
			| SYMOPT_LOAD_LINES
		//	| SYMOPT_CASE_INSENSITIVE
			);
		HANDLE hProcess = GetCurrentProcess();
		if (!SymInitialize(hProcess, NULL, TRUE)) return {};
		PVOID addrs[n_frames] = { 0 };
		int frames = CaptureStackBackTrace(n_skip, n_frames, addrs, NULL);
		std::vector<stacktrace_entry> trace;
		trace.reserve(frames);
		#if IS_GCC
		std::string executable = get_executable_path();
		std::vector<std::pair<PVOID, size_t>> deferred;
		#endif
		for(int i = 0; i < frames; i++) {
			char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
			SYMBOL_INFO* info = (SYMBOL_INFO*)buffer;
			info->SizeOfStruct = sizeof(SYMBOL_INFO);
			info->MaxNameLen = MAX_SYM_NAME;
			DWORD64 displacement;
			DWORD   displacement2;
			IMAGEHLP_LINE64 line;
			bool got_line = SymGetLineFromAddr64(hProcess, (DWORD64)addrs[i], &displacement2, &line);
			if(SymFromAddr(hProcess, (DWORD64)addrs[i], &displacement, info)) {
				//printf("%s:%d %s", line.FileName, info->Name, (int)line.LineNumber);
				//primitive_assert(SymSetContext(hProcess, ))
				if(got_line) {
					trace.push_back({line.FileName, info->Name, (int)line.LineNumber});
				} else {
					trace.push_back({"", info->Name, 0});
				}
			} else {
				trace.push_back({"", "", 0});
				#if IS_GCC
				deferred.push_back({addrs[i], trace.size() - 1});
				#endif
			}
		}
		SymCleanup(hProcess);
		#if IS_GCC
		// If we're compiling with gcc on windows, the debug symbols will be embedded in the
		// executable and retrievable with addr2line (this is provided by mingw).
		// The executable will not be PIE. TODO: I don't know if PIE gcc on windows is a
		// case that we need to worry about, can cross that bridge later.
		// Currently not doing any file grouping like done in the linux version. Assuming
		// the only unresolvable symbols are from this binary.
		// Always two lines per entry?
		// https://kernel.googlesource.com/pub/scm/linux/kernel/git/hjl/binutils/+/hjl/secondary/binutils/addr2line.c
		///auto x = resolve_addresses(addresses, file);
		///puts(x.c_str());
		///auto output = split(trim(x), "\n");
		std::string addresses = "";
		for(auto& [address, _] : deferred) addresses += stringf("%#tx ", address);
		auto output = split(trim(resolve_addresses(addresses, executable)), "\n");
		primitive_assert(output.size() == 2 * deferred.size());
		for(size_t i = 0; i < deferred.size(); i++) {
			// line info is one of the following:
			// path:line (descriminator number)
			// path:line
			// path:?
			// ??:?
			// Regex modified from the linux version to eat the C:\\ at the beginning
			static std::regex location_re(R"(^((?:\w:\\)?[^:]*):?([\d\?]*)(?: \(discriminator \d+\))?$)");
			auto m = match<2>(output[i * 2 + 1], location_re);
			primitive_assert(m.has_value());
			auto [path, line] = *m;
			if(path != "??") {
				trace[deferred[i].second].filename = path;
			}
			trace[deferred[i].second].line = line == "?" ? 0 : std::stoi(line);
			// signature shouldn't require processing
			// signature done after path so path can take signature, if needed
			trace[deferred[i].second].signature = output[i * 2];
		}
		#endif
		return trace;
	}
	#endif
	#ifdef USE_EXECINFO_H
	[[gnu::cold]] [[maybe_unused]] static bool has_addr2line() {
		// Detects if addr2line exists by trying to invoke addr2line --help
		constexpr int magic = 42;
		pid_t pid = fork();
		if(pid == -1) return false; // error? TODO: Diagnostic
		if(pid == 0) { // child
			close(STDOUT_FILENO);
			execlp("addr2line", "addr2line", "--help", nullptr);
			exit(magic); // TODO: Diagnostic?
		}
		int status;
		waitpid(pid, &status, 0); // -1 && errno == EINTR handles interrupt
		return WEXITSTATUS(status) == 0;
	}

	// returns 1 for little endian, 2 for big endien, matches elf
	[[gnu::cold]] [[maybe_unused]] static int endianness() {
		int n = 1;
		if(*(char*)&n == 1) {
			return 1; // little
		} else {
			return 2; // big
		}
	}

	// https://en.wikipedia.org/wiki/Executable_and_Linkable_Format#File_header
	// file header is the same between 32 and 64-bit until offset 0x18
	struct [[gnu::packed]] partial_elf_file_header {
		char magic[4];
		uint8_t e_class;
		uint8_t e_data; // endianness: 1 for little, 2 for big
		uint8_t e_version;
		uint8_t e_osabi;
		uint8_t e_abiver;
		char e_unused[7];
		uint16_t e_type;
	};
	static_assert(sizeof(partial_elf_file_header) == 0x12);

	enum elf_obj_types {
		ET_EXEC = 0x02,
		ET_DYN = 0x03
	};

	[[gnu::cold]] [[maybe_unused]] static uint16_t get_executable_e_type(const std::string& path) {
		FILE* f = fopen(path.c_str(), "rb");
		primitive_assert(f != NULL);
		partial_elf_file_header h;
		size_t s = fread(&h, sizeof(partial_elf_file_header), 1, f);
		if(s != 1) primitive_assert(false, "error while reading file");
		if(h.e_data != endianness()) {
			h.e_type = (h.e_type & 0x00ff) << 8 | (h.e_type & 0xff00) >> 8;
		}
		return h.e_type;
	}

	[[gnu::cold]] [[maybe_unused]]
	static bool paths_refer_to_same(const std::string& path_a, const std::string& path_b) {
		struct stat a, b;
		stat(path_a.c_str(), &a);
		stat(path_b.c_str(), &b);
		static_assert(std::is_integral_v<dev_t>);
		static_assert(std::is_integral_v<ino_t>);
		return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
	}

	struct frame { // TODO: re-evaluate members here
		std::string obj_path;
		std::string symbol;
		void* obj_base = nullptr;
		void* raw_address = nullptr; // raw_address - obj_base -> addr2line
	};

	[[gnu::cold]] [[maybe_unused]]
	static std::vector<frame> backtrace_frames(void * const * array, size_t size, size_t skip) {
		// reference: https://github.com/bminor/glibc/blob/master/debug/backtracesyms.c
		std::vector<frame> frames;
		for(size_t i = skip; i < size; i++) {
			Dl_info info;
			frame frame;
			frame.raw_address = array[i];
			if(dladdr(array[i], &info)) {
				primitive_assert(info.dli_fname != nullptr);
				// dli_sname and dli_saddr are only present with -rdynamic, sname will be included
				// but we don't really need dli_saddr
				frame.obj_path = info.dli_fname;
				frame.obj_base = info.dli_fbase;
				frame.symbol = info.dli_sname ?: "?";
			}
			frames.push_back(frame);
		}
		primitive_assert(frames.size() == size - skip);
		return frames;
	}
	
	struct pipe_t {
		union {
			struct {
				int read_end;
				int write_end;
			};
			int data[2];
		};
	};
	static_assert(sizeof(pipe_t) == 2 * sizeof(int));

	[[gnu::cold]] [[maybe_unused]]
	static std::string resolve_addresses(const std::string& addresses, const std::string& executable) {
		pipe_t output_pipe;
		pipe_t input_pipe;
		pipe(output_pipe.data);
		pipe(input_pipe.data);
		pid_t pid = fork();
		if(pid == -1) return ""; // error? TODO: Diagnostic
		if(pid == 0) { // child
			dup2(output_pipe.write_end, STDOUT_FILENO);
			dup2(input_pipe.read_end, STDIN_FILENO);
			close(output_pipe.read_end);
			close(output_pipe.write_end);
			close(input_pipe.read_end);
			close(input_pipe.write_end);
			execlp("addr2line", "addr2line", "-e", executable.c_str(), "-f", "-C", nullptr);
			exit(1); // TODO: Diagnostic?
		}
		primitive_assert(write(input_pipe.write_end, addresses.data(), addresses.size()) != -1);
		close(input_pipe.read_end);
		close(input_pipe.write_end);
		close(output_pipe.write_end);
		std::string output;
		constexpr int buffer_size = 4096;
		char buffer[buffer_size];
		size_t count = 0;
		while((count = read(output_pipe.read_end, buffer, buffer_size)) > 0) {
			output.insert(output.end(), buffer, buffer + count);
		}
		// TODO: check status from addr2line?
		waitpid(pid, NULL, 0);
		return output;
	}

	[[gnu::cold]] [[maybe_unused]]
	static std::optional<std::vector<stacktrace_entry>> get_stacktrace() {
		void* bt[n_frames];
		int bt_size = backtrace(bt, n_frames);
		///char** x = backtrace_symbols(bt, bt_size);
		///for(int i = 0; i < bt_size; i++) {
		///	puts(x[i]);
		///}
		std::vector<frame> frames = backtrace_frames(bt, bt_size, n_skip);
		///std::cout<<"path symbol obj_base symbol_address raw_address"<<std::endl;
		///for(auto& frame : frames) {
		///	std::cout<<frame.obj_path<<" "<<frame.symbol<<" "<<frame.obj_base<<" "<<frame.raw_address<<std::endl;
		///}
		std::vector<stacktrace_entry> trace(frames.size());
		if(has_addr2line()) {
			// filename -> { addresses string, target vector }
			std::unordered_map<std::string, std::pair<std::string, std::vector<stacktrace_entry*>>> entries;
			std::string binary_path = get_executable_path();
			bool is_pie = get_executable_e_type(binary_path) == ET_DYN;
			std::optional<std::string> dladdr_name_of_executable;
			for(size_t i = 0; i < frames.size(); i++) {
				auto& entry = frames[i];
				primitive_assert(entry.raw_address >= entry.obj_base);
				// There is a bug with dladdr for non-PIE objects
				if(!dladdr_name_of_executable.has_value()) {
					if(paths_refer_to_same(entry.obj_path, binary_path)) {
						dladdr_name_of_executable = entry.obj_path;
					}
				}
				uintptr_t addr;
				// if this symbol is in the executable and the executable is not pie use raw address
				// otherwise compute offset
				if(dladdr_name_of_executable.has_value()
				&& dladdr_name_of_executable.value() == entry.obj_path
				&& !is_pie) {
					addr = (uintptr_t)entry.raw_address;
				} else {
					addr = (uintptr_t)entry.raw_address - (uintptr_t)entry.obj_base;
				}
				///printf("%s :: %p\n", entry.obj_path.c_str(), addr);
				if(entries.count(entry.obj_path) == 0) entries.insert({entry.obj_path, {"", {}}});
				entries.at(entry.obj_path).first += stringf("%#tx\n", addr);
				entries.at(entry.obj_path).second.push_back(&trace[i]);
				trace[i].filename = entry.obj_path;
			}
			// perform translations
			for(auto& [file, pair] : entries) {
				auto& [addresses, target] = pair;
				// Always two lines per entry?
				// https://kernel.googlesource.com/pub/scm/linux/kernel/git/hjl/binutils/+/hjl/secondary/binutils/addr2line.c
				///auto x = resolve_addresses(addresses, file);
				///puts(x.c_str());
				///auto output = split(trim(x), "\n");
				auto output = split(trim(resolve_addresses(addresses, file)), "\n");
				primitive_assert(output.size() == 2 * target.size());
				//printf("%d\n", output.size());
				for(size_t i = 0; i < target.size(); i++) {
					// line info is one of the following:
					// path:line (descriminator number)
					// path:line
					// path:?
					// ??:?
					static std::regex location_re(R"(^([^:]*):?([\d\?]*)(?: \(discriminator \d+\))?$)");
					auto m = match<2>(output[i * 2 + 1], location_re);
					primitive_assert(m.has_value());
					auto [path, line] = *m;
					if(path != "??") {
						target[i]->filename = path;
					}
					target[i]->line = line == "?" ? 0 : std::stoi(line);
					// signature shouldn't require processing
					// signature done after path so path can take signature, if needed
					target[i]->signature = output[i * 2];
				}
			}
		}
		return trace;
	}
	#endif

	[[gnu::cold]] [[maybe_unused]]
	static std::string escape_string(const std::string_view str, char quote) {
		std::string escaped;
		escaped += quote;
		for(unsigned char c : str) {
			if(c == '\\') escaped += "\\\\";
			else if(c == '\t') escaped += "\\t";
			else if(c == '\r') escaped += "\\r";
			else if(c == '\n') escaped += "\\n";
			else if(c == quote) escaped += "\\" + std::to_string(quote);
			else if(c >= 32 && c <= 126) escaped += c; // printable
			else {
				constexpr const char * const hexdig = "0123456789abcdef";
				escaped += std::string("\\x") + hexdig[c >> 4] + hexdig[c & 0xF];
			}
		}
		escaped += quote;
		return escaped;
	}

	[[gnu::cold]]
	static std::string indent(const std::string_view str, size_t depth, char c=' ', bool ignore_first=false) {
		size_t i = 0, j;
		std::string output;
		while((j = str.find('\n', i)) != std::string::npos) {
			if(i != 0 || !ignore_first) output.insert(output.end(), depth, c);
			output.insert(output.end(), str.begin() + i, str.begin() + j + 1);
			i = j + 1;
		}
		if(i != 0 || !ignore_first) output.insert(output.end(), depth, c);
		output.insert(output.end(), str.begin() + i, str.end());
		return output;
	}

	struct nothing {};

	template<typename T> constexpr bool is_nothing = std::is_same_v<T, nothing>;

	// Hack to get around static_assert(false); being evaluated before any instantiation, even under
	// an if-constexpr branch
	template<typename T> constexpr bool always_false = false;

	template<typename T> using strip = std::remove_cv_t<std::remove_reference_t<T>>;

	// Is integral but not boolean
	template<typename T> constexpr bool is_integral_notb = std::is_integral_v<strip<T>> && !std::is_same_v<strip<T>, bool>;

	// Lots of boilerplate
	// Using int comparison functions here to support proper signed comparisons. Need to make sure
	// assert(map.count(1) == 2) doesn't produce a warning. It wouldn't under normal circumstances
	// but it would in this library due to the parameters being forwarded down a long chain.
	// And we want to provide as much robustness as possible anyways.
	// Copied and pasted from https://en.cppreference.com/w/cpp/utility/intcmp
	// Not using std:: versions because library is targetting C++17
	template<typename T, typename U>
	constexpr bool cmp_equal(T t, U u) {
		using UT = std::make_unsigned_t<T>;
		using UU = std::make_unsigned_t<U>;
		if constexpr (std::is_signed_v<T> == std::is_signed_v<U>)
			return t == u;
		else if constexpr (std::is_signed_v<T>)
			return t < 0 ? false : UT(t) == u;
		else
			return u < 0 ? false : t == UU(u);
	}
	
	template<typename T, typename U>
	constexpr bool cmp_not_equal(T t, U u) {
		return !cmp_equal(t, u);
	}
	
	template<typename T, typename U>
	constexpr bool cmp_less(T t, U u) {
		using UT = std::make_unsigned_t<T>;
		using UU = std::make_unsigned_t<U>;
		if constexpr (std::is_signed_v<T> == std::is_signed_v<U>)
			return t < u;
		else if constexpr (std::is_signed_v<T>)
			return t < 0 ? true : UT(t) < u;
		else
			return u < 0 ? false : t < UU(u);
	}
	
	template<typename T, typename U>
	constexpr bool cmp_greater(T t, U u) {
		return cmp_less(u, t);
	}
	
	template<typename T, typename U>
	constexpr bool cmp_less_equal(T t, U u) {
		return !cmp_greater(t, u);
	}
	
	template<typename T, typename U>
	constexpr bool cmp_greater_equal(T t, U u) {
		return !cmp_less(t, u);
	}

	// Lots of boilerplate
	// std:: implementations don't allow two separate types for lhs/rhs
	// Note: is this macro potentially bad when it comes to debugging(?)
	// TODO: put all of this in a namespace?
	#define gen_op_boilerplate(name, op, ...) struct name { \
		static constexpr std::string_view op_string = #op; \
		template<typename A, typename B> \
		constexpr auto operator()(A&& lhs, B&& rhs) const { \
			__VA_OPT__(if constexpr (is_integral_notb<A> && is_integral_notb<B>) return __VA_ARGS__(lhs, rhs); else) /* no need to forward ints */ \
			return std::forward<A>(lhs) op std::forward<B>(rhs); \
		} \
	}
	gen_op_boilerplate(shift_left, <<);
	gen_op_boilerplate(shift_right, >>);
	gen_op_boilerplate(equal_to, ==, cmp_equal); // todo: rename -> equal?
	gen_op_boilerplate(not_equal_to, !=, cmp_not_equal);
	gen_op_boilerplate(greater, >, cmp_greater);
	gen_op_boilerplate(less, <, cmp_less);
	gen_op_boilerplate(greater_equal, >=, cmp_greater_equal);
	gen_op_boilerplate(less_equal, <=, cmp_less_equal);
	gen_op_boilerplate(bit_and, &);
	gen_op_boilerplate(bit_xor, ^);
	gen_op_boilerplate(bit_or, |);
	gen_op_boilerplate(logical_and, &&);
	gen_op_boilerplate(logical_or, ||);
	gen_op_boilerplate(assign, =);
	gen_op_boilerplate(add_assign, +=);
	gen_op_boilerplate(minus_assign, -=);
	gen_op_boilerplate(mul_assign, *=);
	gen_op_boilerplate(div_assign, /=);
	gen_op_boilerplate(mod_assign, %=);
	gen_op_boilerplate(shift_left_assign, <<=);
	gen_op_boilerplate(shift_right_assign, >>=);
	gen_op_boilerplate(bit_and_assign, &=);
	gen_op_boilerplate(bit_xor_assign, ^=);
	gen_op_boilerplate(bit_or_assign, |=);
	#undef gen_op_boilerplate

	// I learned this automatic expression decomposition trick from lest:
	// https://github.com/martinmoene/lest/blob/master/include/lest/lest.hpp#L829-L853
	//
	// I have improved upon the trick by supporting more operators and adding perfect forwarding.
	//
	// Some cases to test and consider:
	//
	// Expression   Parsed as
	// Basic:
	// false        << false
	// a == b       (<< a) == b
	//
	// Equal precedence following:
	// a << b       (<< a) << b
	// a << b << c  ((<< a) << b) << c
	// a << b + c   (<< a) << (b + c)
	// a << b < c   ((<< a) << b) < c  // edge case
	//
	// Higher precedence following:
	// a + b        << (a + b)
	// a + b + c    << ((a + b) + c)
	// a + b * c    << (a + (b * c))
	// a + b < c    (<< (a + b)) < c
	//
	// Lower precedence following:
	// a < b        (<< a) < b
	// a < b < c    ((<< a) < b) < c
	// a < b + c    (<< a) < (b + c)
	// a < b == c   ((<< a) < b) == c // edge case

	template<typename A = nothing, typename B = nothing, typename C = nothing>
	struct expression_decomposer {
		A a;
		B b;
		explicit expression_decomposer() = default;
		template<typename U>
		explicit expression_decomposer(U&& a) : a(std::forward<U>(a)) {}
		template<typename U, typename V>
		explicit expression_decomposer(U&& a, V&& b) : a(std::forward<U>(a)), b(std::forward<V>(b)) {}
		// Note: get_value or operator bool() should only be invoked once
		auto get_value() {
			if constexpr(is_nothing<C>) {
				static_assert(is_nothing<B> && !is_nothing<A>);
				return std::forward<A>(a);
			} else {
				return C()(std::forward<A>(a), std::forward<B>(b));
			}
		}
		operator bool() { return get_value(); }
		// Need overloads for operators with precedence <= bitshift
		// TODO: spaceship operator?
		// Note: Could decompose more than just comparison and boolean operators, but it would take
		// a lot of work and I don't think it's beneficial for this library.
		template<typename O> auto operator<<(O&& operand) {
			using Q = std::conditional_t<std::is_rvalue_reference_v<O>, std::remove_reference_t<O>, O>;
			if constexpr (is_nothing<A>) {
				static_assert(is_nothing<B> && is_nothing<C>);
				return expression_decomposer<Q, nothing, nothing>(std::forward<O>(operand));
			} else if constexpr (is_nothing<B>) {
				static_assert(is_nothing<C>);
				return expression_decomposer<A, Q, shift_left>(std::forward<A>(a), std::forward<O>(operand));
			} else {
				static_assert(!is_nothing<C>);
				return expression_decomposer<decltype(get_value()), O, shift_left>(std::forward<A>(get_value()), std::forward<O>(operand));
			}
		}
		#define gen_op_boilerplate(functor, op) template<typename O> auto operator op(O&& operand) { \
			static_assert(!is_nothing<A>); \
			using Q = std::conditional_t<std::is_rvalue_reference_v<O>, std::remove_reference_t<O>, O>; \
			if constexpr (is_nothing<B>) { \
				static_assert(is_nothing<C>); \
				return expression_decomposer<A, Q, functor>(std::forward<A>(a), std::forward<O>(operand)); \
			} else { \
				static_assert(!is_nothing<C>); \
				return expression_decomposer<decltype(get_value()), Q, functor>(std::forward<A>(get_value()), std::forward<O>(operand)); \
			} \
		}
		gen_op_boilerplate(shift_right, >>);
		gen_op_boilerplate(equal_to, ==);
		gen_op_boilerplate(not_equal_to, !=);
		gen_op_boilerplate(greater, >);
		gen_op_boilerplate(less, <);
		gen_op_boilerplate(greater_equal, >=);
		gen_op_boilerplate(less_equal, <=);
		gen_op_boilerplate(bit_and, &);
		gen_op_boilerplate(bit_xor, ^);
		gen_op_boilerplate(bit_or, |);
		gen_op_boilerplate(logical_and, &&);
		gen_op_boilerplate(logical_or, ||);
		gen_op_boilerplate(assign, =);
		gen_op_boilerplate(add_assign, +=);
		gen_op_boilerplate(minus_assign, -=);
		gen_op_boilerplate(mul_assign, *=);
		gen_op_boilerplate(div_assign, /=);
		gen_op_boilerplate(mod_assign, %=);
		gen_op_boilerplate(shift_left_assign, <<=);
		gen_op_boilerplate(shift_right_assign, >>=);
		gen_op_boilerplate(bit_and_assign, &=);
		gen_op_boilerplate(bit_xor_assign, ^=);
		gen_op_boilerplate(bit_or_assign, |=);
		#undef gen_op_boilerplate
	};

	// for ternary support
	template<typename U>
	expression_decomposer(U&&) -> expression_decomposer<std::conditional_t<std::is_rvalue_reference_v<U>, std::remove_reference_t<U>, U>>;

	enum class literal_format {
		dec,
		hex,
		octal,
		binary,
		none
	};

	template<typename T>
	[[gnu::cold]]
	constexpr std::string_view type_name() {
		// clang: std::string_view ns::type_name() [T = int]
		// gcc:   constexpr std::string_view ns::type_name() [with T = int; std::string_view = std::basic_string_view<char>]
		// msvc:  const char *__cdecl ns::type_name<int>(void)
		auto substring_bounded_by = [](std::string_view sig, std::string_view l, std::string_view r) {
			primitive_assert(sig.find(l) != std::string_view::npos);
			primitive_assert(sig.rfind(r) != std::string_view::npos);
			primitive_assert(sig.find(l) < sig.rfind(r));
			auto i = sig.find(l) + l.length();
			return sig.substr(i, sig.rfind(r) - i);
		};
		#if defined(__clang__)
			return substring_bounded_by(__PRETTY_FUNCTION__, "[T = ", "]");
		#elif defined(__GNUC__) || defined(__GNUG__)
			return substring_bounded_by(__PRETTY_FUNCTION__, "[with T = ", "; std::string_view = ");
		#elif defined(_MSC_VER)
			return substring_bounded_by(__FUNCSIG__, "type_name<", ">(void)");
		#else
			// TODO: ?
			return __PRETTY_FUNCTION__;
		#endif
	}

	// https://stackoverflow.com/questions/28309164/checking-for-existence-of-an-overloaded-member-function
	template<typename T> class has_stream_overload {
		template<typename C,
				 typename = decltype(std::declval<std::ostringstream>() << std::declval<C>())>
		static std::true_type test(int);
		template<typename C>
		static std::false_type test(...);
	public:
		static constexpr bool value = decltype(test<T>(0))::value;
	};

	template<typename T> constexpr bool has_stream_overload_v = has_stream_overload<T>::value;

	template<typename T>
	[[gnu::cold]]
	std::string stringify(const T& t, [[maybe_unused]] literal_format fmt = literal_format::none) {
		// bool and char need to be before std::is_integral
		if constexpr (std::is_same_v<strip<T>, std::string>
					  || std::is_same_v<strip<T>, std::string_view>
					//|| std::is_same_v<strip<T>, char*>
					//|| std::is_same_v<strip<T>, const char*>
					  || std::is_same_v<std::remove_cv_t<std::decay_t<T>>, char*> // <- covers literals (i.e. const char(&)[N]) too
					) {
			if constexpr(std::is_pointer_v<T>) {
				if(t == nullptr) {
					return "nullptr";
				}
			}
			// TODO: re-evaluate this...? What if just comparing two buffer pointers?
			return escape_string(t, '"'); // string view may need explicit construction?
		} else if constexpr (std::is_same_v<strip<T>, char>) {
			return escape_string({&t, 1}, '\'');
		} else if constexpr (std::is_same_v<strip<T>, bool>) {
			return t ? "true" : "false"; // streams/boolalpha not needed for this
		} else if constexpr (std::is_same_v<strip<T>, std::nullptr_t>) {
			return "nullptr";
		} else if constexpr (std::is_pointer_v<strip<T>> || std::is_same_v<strip<T>, uintptr_t>) {
			if(uintptr_t(t) == uintptr_t(nullptr)) { // weird nullptr shenanigans, only prints "nullptr" for nullptr_t
				return "nullptr";
			} else {
				std::ostringstream oss;
				// Manually format the pointer - ostream::operator<<(void*) falls back to %p which
				// is implementation-defined. MSVC prints pointers without the leading "0x" which
				// messes up the highlighter.
				oss<<std::showbase<<std::hex<<uintptr_t(t);
				return std::move(oss).str();
			}
		} else if constexpr (std::is_integral_v<T>) {
			std::ostringstream oss;
			switch(fmt) {
				case literal_format::dec:
					break;
				case literal_format::hex:
					oss<<std::showbase<<std::hex;
					break;
				case literal_format::octal:
					oss<<std::showbase<<std::oct;
					break;
				case literal_format::binary:
					oss<<"0b"<<std::bitset<sizeof(t) * 8>(t);
					goto r;
				default:
					primitive_assert(false, "unexpected literal format requested for printing");
			}
			oss<<t;
			r: return std::move(oss).str();
		} else if constexpr (std::is_floating_point_v<T>) {
			std::ostringstream oss;
			switch(fmt) {
				case literal_format::dec:
					break;
				case literal_format::hex:
					// apparently std::hexfloat automatically prepends "0x" while std::hex does not
					oss<<std::hexfloat;
					break;
				case literal_format::octal:
				case literal_format::binary:
					return "";
				default:
					primitive_assert(false, "unexpected literal format requested for printing");
			}
			oss<<std::setprecision(std::numeric_limits<T>::max_digits10)<<t;
			return std::move(oss).str();
		} else {
			if constexpr (has_stream_overload_v<T>) {
				std::ostringstream oss;
				oss<<t;
				return std::move(oss).str();
			} else {
				return stringf("<instance of %s>", std::string(type_name<T>()).c_str());
			}
		}
	}

	[[gnu::cold]]
	static std::string union_regexes(std::initializer_list<std::string_view> regexes) {
		std::string composite;
		for(const std::string_view str : regexes) {
			if(composite != "") composite += "|";
			composite += "(?:" + std::string(str) + ")";
		}
		return composite;
	}
	
	// file-local singleton, at most 8 BSS bytes per TU in debug mode is no problem
	// <1KB heap bytes per TU at runtime but in practice should only happen in one TU
	class analysis;
	static analysis* analysis_singleton;

	class analysis {
		static analysis& get() {
			if(analysis_singleton == nullptr) {
				analysis_singleton = new analysis();
			}
			return *analysis_singleton;
		}

	public:
		enum class token_e {
			keyword,
			punctuation,
			number,
			string,
			named_literal,
			identifier,
			whitespace
		};

		struct token_t {
			token_e token_type;
			std::string str;
		};

		struct highlight_block {
			std::string_view color;
			std::string content;
		};

	private:
		// could all be const but I don't want to try to pack everything into an init list
		std::vector<std::pair<token_e, std::regex>> rules; // could be std::array but I don't want to hard-code a size
		std::string int_binary;
		std::string int_octal;
		std::string int_decimal;
		std::string int_hex;
		std::string float_decimal;
		std::string float_hex;
		std::regex escapes_re;
		std::unordered_map<std::string_view, int> precedence;
		std::unordered_map<std::string_view, std::string_view> braces = {
			// template angle brackets excluded from this analysis
			{ "(", ")" }, { "{", "}" }, { "[", "]" }, { "<:", ":>" }, { "<%", "%>" }
		};
		std::unordered_map<std::string_view, std::string_view> digraph_map = {
			{ "<:", "[" }, { "<%", "{" }, { ":>", "]" }, { "%>", "}" }
		};
		// punctuators to highlight
		std::unordered_set<std::string_view> highlight_ops = {
			"~", "!", "+", "-", "*", "/", "%", "^", "&", "|", "=", "+=", "-=", "*=", "/=", "%=",
			"^=", "&=", "|=", "==", "!=", "<", ">", "<=", ">=", "<=>", "&&", "||", "<<", ">>",
			"<<=", ">>=", "++", "--", "and", "or", "xor", "not", "bitand", "bitor", "compl",
			"and_eq", "or_eq", "xor_eq", "not_eq"
		};
		// all operators
		std::unordered_set<std::string_view> operators = {
			":", "...", "?", "::", ".", ".*", "->", "->*", "~", "!", "+", "-", "*", "/", "%", "^",
			"&", "|", "=", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "==", "!=", "<", ">",
			"<=", ">=", "<=>", "&&", "||", "<<", ">>", "<<=", ">>=", "++", "--", ",", "and", "or",
			"xor", "not", "bitand", "bitor", "compl", "and_eq", "or_eq", "xor_eq", "not_eq"
		};
		std::unordered_map<std::string_view, std::string_view> alternative_operators_map = {
			{"and", "&&"}, {"or", "||"}, {"xor", "^"}, {"not", "!"}, {"bitand", "&"},
			{"bitor", "|"}, {"compl", "~"}, {"and_eq", "&="}, {"or_eq", "|="}, {"xor_eq", "^="},
			{"not_eq", "!="}
		};
		std::unordered_set<std::string_view> bitwise_operators = {
			"^", "&", "|", "^=", "&=", "|=", "xor", "bitand", "bitor", "and_eq", "or_eq", "xor_eq"
		};
		std::vector<std::pair<std::regex, literal_format>> literal_formats;

		[[gnu::cold]]
		std::string_view normalize_op(const std::string_view op) {
			// Operators need to be normalized to support alternative operators like and and bitand
			// Normalization instead of just adding to the precedence table because target operators
			// will always be the normalized operator even when the alternative operator is used.
			if(alternative_operators_map.count(op)) return alternative_operators_map.at(op);
			else return op;
		}

		[[gnu::cold]]
		std::string_view normalize_brace(const std::string_view brace) {
			// Operators need to be normalized to support alternative operators like and and bitand
			// Normalization instead of just adding to the precedence table because target operators
			// will always be the normalized operator even when the alternative operator is used.
			if(digraph_map.count(brace)) return digraph_map.at(brace);
			else return brace;
		}

		[[gnu::cold]]
		analysis() {
			// https://eel.is/c++draft/gram.lex
			// generate regular expressions
			std::string keywords[] = { "alignas", "constinit", "public", "alignof", "const_cast",
				"float", "register", "try", "asm", "continue", "for", "reinterpret_cast", "typedef",
				"auto", "co_await", "friend", "requires", "typeid", "bool", "co_return", "goto",
				"return", "typename", "break", "co_yield", "if", "short", "union", "case",
				"decltype", "inline", "signed", "unsigned", "catch", "default", "int", "sizeof",
				"using", "char", "delete", "long", "static", "virtual", "char8_t", "do", "mutable",
				"static_assert", "void", "char16_t", "double", "namespace", "static_cast",
				"volatile", "char32_t", "dynamic_cast", "new", "struct", "wchar_t", "class", "else",
				"noexcept", "switch", "while", "concept", "enum", "template", "const", "explicit",
				"operator", "this", "consteval", "export", "private", "thread_local", "constexpr",
				"extern", "protected", "throw" };
			std::string punctuators[] = { "{", "}", "[", "]", "(", ")", "<:", ":>", "<%",
				"%>", ";", ":", "...", "?", "::", ".", ".*", "->", "->*", "~", "!", "+", "-", "*",
				"/", "%", "^", "&", "|", "=", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "==",
				"!=", "<", ">", "<=", ">=", "<=>", "&&", "||", "<<", ">>", "<<=", ">>=", "++", "--",
				",", "and", "or", "xor", "not", "bitand", "bitor", "compl", "and_eq", "or_eq",
				"xor_eq", "not_eq", "#" }; // # appears in some lambda signatures
			// Sort longest -> shortest (secondarily A->Z)
			const auto cmp = [](const std::string_view a, const std::string_view b) {
				if(a.length() > b.length()) return true;
				else if(a.length() == b.length()) return a < b;
				else return false;
			};
			std::sort(std::begin(keywords), std::end(keywords), cmp);
			std::sort(std::begin(punctuators), std::end(punctuators), cmp);
			// Escape special characters and add wordbreaks
			std::transform(std::begin(punctuators), std::end(punctuators), std::begin(punctuators), [](const std::string& str) {
				const std::regex special_chars { R"([-[\]{}()*+?.,\^$|#\s])" };
				return std::regex_replace(str + (isalpha(str[0]) ? "\\b" : ""), special_chars, "\\$&");
			});
			// https://eel.is/c++draft/lex.pptoken#3.2
			*std::find(std::begin(punctuators), std::end(punctuators), "<:") += "(?!:[^:>])";
			// regular expressions
			std::string keywords_re    = "(?:" + join(keywords, "|") + ")\\b";
			std::string punctuators_re = join(punctuators, "|");
			// numeric literals
			std::string optional_integer_suffix = "(?:[Uu](?:LL?|ll?|Z|z)?|(?:LL?|ll?|Z|z)[Uu]?)?";
			int_binary  = "0[Bb][01](?:'?[01])*" + optional_integer_suffix;
			// slightly modified from grammar so 0 is lexed as a decimal literal instead of octal
			int_octal   = "0(?:'?[0-7])+" + optional_integer_suffix;
			int_decimal = "(?:0|[1-9](?:'?\\d)*)" + optional_integer_suffix;
			int_hex	    = "0[Xx](?!')(?:'?[\\da-fA-F])+" + optional_integer_suffix;
			std::string digit_sequence = "\\d(?:'?\\d)*";
			std::string fractional_constant = stringf("(?:(?:%s)?\\.%s|%s\\.)", digit_sequence.c_str(), digit_sequence.c_str(), digit_sequence.c_str());
			std::string exponent_part = "(?:[Ee][\\+-]?" + digit_sequence + ")";
			std::string suffix = "[FfLl]";
			float_decimal = stringf("(?:%s%s?|%s%s)%s?",
								fractional_constant.c_str(), exponent_part.c_str(),
								digit_sequence.c_str(), exponent_part.c_str(), suffix.c_str());
			std::string hex_digit_sequence = "[\\da-fA-F](?:'?[\\da-fA-F])*";
			std::string hex_frac_const = stringf("(?:(?:%s)?\\.%s|%s\\.)", hex_digit_sequence.c_str(), hex_digit_sequence.c_str(), hex_digit_sequence.c_str());
			std::string binary_exp = "[Pp][\\+-]?" + digit_sequence;
			float_hex = stringf("0[Xx](?:%s|%s)%s%s?",
						hex_frac_const.c_str(), hex_digit_sequence.c_str(), binary_exp.c_str(), suffix.c_str());
			// char and string literals
			std::string escapes = R"(\\[0-7]{1,3}|\\x[\da-fA-F]+|\\.)";
			std::string char_literal = R"((?:u8|[UuL])?'(?:)" + escapes + R"(|[^\n'])*')";
			std::string string_literal = R"((?:u8|[UuL])?"(?:)" + escapes + R"(|[^\n"])*")";
			std::string raw_string_literal = R"((?:u8|[UuL])?R"([^ ()\\t\r\v\n]*)\((?:(?!\)\2\").)*\)\2")"; // \2 because the first capture is the match without the rest of the file
			escapes_re = std::regex(escapes);
			// final rule set
			// regex is greedy, sequencing rules are as follow:
			// keyword > identifier
			// number > punctuation (for .1f)
			// float > int (for 1.5 and hex floats)
			// hex int > decimal int
			// named_literal > identifier
			// string > identifier (for R"(foobar)")
			// punctuation > identifier (for "not" and other alternative operators)
			std::pair<token_e, std::string> rules_raw[] = {
				{ token_e::keyword    , keywords_re },
				{ token_e::number     , union_regexes({
					float_decimal,
					float_hex,
					int_binary,
					int_octal,
					int_hex,
					int_decimal
				}) },
				{ token_e::punctuation, punctuators_re },
				{ token_e::named_literal, "true|false|nullptr" },
				{ token_e::string     , union_regexes({
					char_literal,
					raw_string_literal,
					string_literal
				}) },
				{ token_e::identifier , R"((?!\d+)(?:[\da-zA-Z_\$]|\\u[\da-fA-F]{4}|\\U[\da-fA-F]{8})+)" },
				{ token_e::whitespace , R"(\s+)" }
			};
			rules.resize(std::size(rules_raw));
			for(size_t i = 0; i < std::size(rules_raw); i++) {
				std::string str = stringf("^(%s)[^]*", rules_raw[i].second.c_str()); // [^] instead of . because . does not match newlines
				#ifdef _0_DEBUG_ASSERT_LEXER_RULES
				fprintf(stderr, "%s : %s\n", rules_raw[i].first.c_str(), str.c_str());
				#endif
				rules[i] = { rules_raw[i].first, std::regex(str) };
			}
			// setup literal format rules
			literal_formats = {
				{ std::regex(int_binary),    literal_format::binary },
				{ std::regex(int_octal),     literal_format::octal },
				{ std::regex(int_decimal),   literal_format::dec },
				{ std::regex(int_hex),       literal_format::hex },
				{ std::regex(float_decimal), literal_format::dec },
				{ std::regex(float_hex),     literal_format::hex }
			};
			// generate precedence table
			// bottom few rows of the precedence table:
			const std::unordered_map<int, std::vector<std::string_view>> precedences = {
				{ -1, { "<<", ">>" } },
				{ -2, { "<", "<=", ">=", ">" } },
				{ -3, { "==", "!=" } },
				{ -4, { "&" } },
				{ -5, { "^" } },
				{ -6, { "|" } },
				{ -7, { "&&" } },
				{ -8, { "||" } },
				{ -9, { "?", ":", "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=" } }, // Note: associativity logic currently relies on these having precedence -9
				{ -10, { "," } }
			};
			std::unordered_map<std::string_view, int> table;
			for(const auto& [ p, ops ] : precedences) {
				for(const auto& op : ops) {
					table.insert({op, p});
				}
			}
			precedence = table;
		}

		[[gnu::cold]]
		std::vector<token_t> _tokenize(const std::string& expression, bool decompose_shr=false) {
			std::vector<token_t> tokens;
			size_t i = 0;
			while(i < expression.length()) {
				std::smatch match;
				bool at_least_one_matched = false;
				for(const auto& [ type, re ] : rules) {
					if(std::regex_match(std::begin(expression) + i, std::end(expression), match, re)) {
						#ifdef _0_DEBUG_ASSERT_TOKENIZATION
						fprintf(stderr, "%s\n", match[1].str().c_str());
						fflush(stdout);
						#endif
						if(decompose_shr && match[1].str() == ">>") { // Do >> decomposition now for templates
							tokens.push_back({ type, ">" });
							tokens.push_back({ type, ">" });
						} else {
							tokens.push_back({ type, match[1].str() });
						}
						i += match[1].length();
						at_least_one_matched = true;
						break;
					}
				}
				if(!at_least_one_matched) {
					// TODO: silent fail option / attempt to recover?
					throw "error: invalid token?";
				}
			}
			return tokens;
		}

		[[gnu::cold]]
		std::vector<highlight_block> _highlight(const std::string& expression) try {
			// TODO: improve highlighting, will require more situational awareness
			const auto tokens = _tokenize(expression);
			std::vector<highlight_block> output;
			for(size_t i = 0; i < tokens.size(); i++) {
				const auto& token = tokens[i];
				// Peek next non-whitespace token, return empty whitespace token if end is reached
				const auto peek = [i, &tokens](size_t j = 1) {
					for(size_t k = 1; j > 0 && i + k < tokens.size(); k++) {
						if(tokens[i + k].token_type != token_e::whitespace) {
							if(--j == 0) {
								return tokens[i + k];
							}
						}
					}
					primitive_assert(j != 0);
					return token_t { token_e::whitespace, "" };
				};
				switch(token.token_type) {
					case token_e::keyword:
						output.push_back({PURPL, token.str});
						break;
					case token_e::punctuation:
						if(highlight_ops.count(token.str)) {
							output.push_back({PURPL, token.str});
						} else {
							output.push_back({"", token.str});
						}
						break;
					case token_e::named_literal:
						output.push_back({ORANGE, token.str});
						break;
					case token_e::number:
						output.push_back({CYAN, token.str});
						break;
					case token_e::string:
						output.push_back({GREEN, std::regex_replace(token.str, escapes_re, BLUE "$&" GREEN)});
						break;
					case token_e::identifier:
						if(peek().str == "(") {
							output.push_back({BLUE, token.str});
						} else if(peek().str == "::") {
							output.push_back({YELLOW, token.str});
						} else {
							output.push_back({RED, token.str});
						}
						break;
					case token_e::whitespace:
						output.push_back({"", token.str});
						break;
				}
			}
			return output;
		} catch(...) {
			// yes, yes, never use exceptions for control flow, but performance really does not matter here!
			return {{"", expression}};
		}

		[[gnu::cold]]
		literal_format _get_literal_format(const std::string& expression) {
			for(auto& [ re, type ] : literal_formats) {
				if(std::regex_match(expression, re)) {
					return type;
				}
			}
			return literal_format::none; // not a literal
		}

		[[gnu::cold]]
		bool _has_precedence_below_or_equal(const std::string& expression, const std::string_view op) {
			// Scans over top-level of expression and looks for any operators with precedence <=
			// op's precedence.
			// Anything between parentheses can be excluded but no attempt is made to figure out
			// template parameter lists.
			// We can figure out unary / binary easy enough TODO: we don't actually need to worry about this though
			// TODO: There is redundant tokenization here
			// Note: Templates make C++ expressions ambiguous without type info. This function looks
			// for any occurrance of op that cannot be ruled out. False positives are ok, most stuff
			// can be ruled out though.
			const auto tokens = _tokenize(expression);
			// precedence table is binary, unary operators have highest precedence
			// we can figure out unary / binary easy enough
			enum {
				expecting_operator,
				expecting_term
			} state = expecting_term;
			for(std::size_t i = 0; i < tokens.size(); i++) {
				const token_t& token = tokens[i];
				switch(token.token_type) {
					case token_e::punctuation:
						if(operators.count(token.str)) {
							if(state == expecting_term) {
								// must be unary, continue
							} else {
								// binary
								if(precedence.count(normalize_op(token.str))
								&& precedence.count(op)
								&& precedence.at(normalize_op(token.str)) <= precedence.at(op)) {
									return true;
								}
								state = expecting_term;
							}
						} else if(braces.count(token.str)) {
							// We can assume the given expression is valid.
							// Braces must be balanced.
							// Scan forward until finding matching brace, don't need to take into
							// account other types of braces.
							const std::string_view open = normalize_brace(token.str);
							const std::string_view close = normalize_brace(braces.at(token.str));
							int count = 0;
							while(++i < tokens.size()) {
								if(normalize_brace(tokens[i].str) == open) count++;
								else if(normalize_brace(tokens[i].str) == close) {
									if(count-- == 0) {
										break;
									}
								}
							}
							state = expecting_operator;
						} else {
							primitive_assert(false, "unhandled punctuation?");
						}
						break;
					case token_e::keyword:
					case token_e::named_literal:
					case token_e::number:
					case token_e::string:
					case token_e::identifier:
						state = expecting_operator;
					case token_e::whitespace:
						break;
				}
			}
			return false;
		}

		[[gnu::cold]]
		token_t find_last_non_ws(const std::vector<token_t>& tokens, size_t i) {
			// returns empty token_e::whitespace on failure
			while(i--) {
				if(tokens[i].token_type != token_e::whitespace) {
					return tokens[i];
				}
			}
			return {token_e::whitespace, ""};
		}

		[[gnu::cold]]
		static std::string_view get_real_op(const std::vector<token_t>& tokens, const size_t i) {
			// re-coalesce >> if necessary
			bool is_shr = tokens[i].str == ">" && i < tokens.size() - 1 && tokens[i + 1].str == ">";
			return is_shr ? std::string_view(">>") : std::string_view(tokens[i].str);
		}

		// In this function we are essentially exploring all possible parse trees for an expression
		// an making an attempt to disambiguate as much as we can. It's potentially O(2^t) (?) with
		// t being the number of possible templates in the expression, but t is anticipated to
		// always be small.
		// TODO: Safeguard? TODO: Count possible matching angle brackets?
		// TODO: purify?
		[[gnu::cold]] void pseudoparse(
			const std::vector<token_t>& tokens,
			const std::string_view target_op,
			size_t i,
			int current_lowest_precedence,
			int template_depth,
			int middle_index, // where the split currently is, current op = tokens[middle_index]
			std::set<int>& output
		) {
			#ifdef _0_DEBUG_ASSERT_DISAMBIGUATION
			printf("*");
			#endif
			// precedence table is binary, unary operators have highest precedence
			// we can figure out unary / binary easy enough
			enum {
				expecting_operator,
				expecting_term
			} state = expecting_term;
			for(; i < tokens.size(); i++) {
				const token_t& token = tokens[i];
				// scan forward to matching brace
				// can assume braces are balanced
				const auto scan_forward = [this, &i, &tokens](const std::string_view open, const std::string_view close) {
					bool empty = true;
					int count = 0;
					while(++i < tokens.size()) {
						if(normalize_brace(tokens[i].str) == normalize_brace(open)) count++;
						else if(normalize_brace(tokens[i].str) == normalize_brace(close)) {
							if(count-- == 0) {
								break;
							}
						} else if(tokens[i].token_type != token_e::whitespace) {
							empty = false;
						}
					}
					if(i == tokens.size() && count != -1) primitive_assert(false, "ill-formed expression input");
					return empty;
				};
				switch(token.token_type) {
					case token_e::punctuation:
						if(operators.count(token.str)) {
							if(state == expecting_term) {
								// must be unary, continue
							} else {
								// template can only open with a < token, no need to check << or <<=
								// also must be preceeded by an identifier
								if(token.str == "<" && find_last_non_ws(tokens, i).token_type == token_e::identifier) {
									// branch 1: this is a template opening
									pseudoparse(tokens, target_op, i + 1, current_lowest_precedence, template_depth + 1, middle_index, output);
									// branch 2: this is a binary operator // fallthrough
								} else if(token.str == "<" && normalize_brace(find_last_non_ws(tokens, i).str) == "]") {
									// this must be a template parameter list, part of a generic lambda
									bool empty = scan_forward("<", ">");
									primitive_assert(!empty);
									state = expecting_operator;
									continue;
								}
								if(template_depth > 0 && token.str == ">") {
									// No branch here: This must be a close. C++ standard
									// Disambiguates by treating > always as a template parameter
									// list close and >> is broken down.
									// >= and >>= don't need to be broken down and we don't need to
									// worry about re-tokenizing beyond just the simple breakdown.
									// I.e. we don't need to worry about x<2>>>1 which is tokenized
									// as x < 2 >> > 1 but perhaps being intended as x < 2 > >> 1.
									// Standard has saved us from this complexity.
									// Note: >> breakdown moved to initial tokenization so we can
									// take the token vector by reference.
									template_depth--;
									state = expecting_operator;
									continue;
								}
								// binary
								if(template_depth == 0) { // ignore precedence in template parameter list
									// re-coalesce >> if necessary
									std::string_view op = normalize_op(get_real_op(tokens, i));
									if(precedence.count(op))
									if(precedence.at(op) < current_lowest_precedence
									|| (precedence.at(op) == current_lowest_precedence && precedence.at(op) != -9)) {
										middle_index = i;
										current_lowest_precedence = precedence.at(op);
									}
									if(op == ">>") i++;
								}
								state = expecting_term;
							}
						} else if(braces.count(token.str)) {
							// We can assume the given expression is valid.
							// Braces must be balanced.
							// Scan forward until finding matching brace, don't need to take into
							// account other types of braces.
							const std::string_view open = token.str;
							const std::string_view close = braces.at(token.str);
							bool empty = scan_forward(open, close);
							// Handle () and {} when they aren't a call/initializer
							// [] may appear in lambdas when state == expecting_term
							// [](){ ... }() is parsed fine because state == expecting_operator
							// after the captures list. Not concerned with template parameters at
							// the moment.
							if(state == expecting_term && empty && normalize_brace(open) != "[") {
								return; // this is a failed parse tree
							}
							state = expecting_operator;
						} else {
							primitive_assert(false, "unhandled punctuation?");
						}
						break;
					case token_e::keyword:
					case token_e::named_literal:
					case token_e::number:
					case token_e::string:
					case token_e::identifier:
						state = expecting_operator;
					case token_e::whitespace:
						break;
				}
			}
			if(middle_index != -1
			&& normalize_op(get_real_op(tokens, middle_index)) == target_op
			&& template_depth == 0
			&& state == expecting_operator) {
				output.insert(middle_index);
			} else {
				// failed parse tree, ignore
			}
		}

		[[gnu::cold]]
		std::pair<std::string, std::string> _decompose_expression(const std::string& expression, const std::string_view target_op) {
			// While automatic decomposition allows something like `assert(foo(n) == bar<n> + n);`
			// treated as `assert_eq(foo(n), bar<n> + n);` we only get the full expression's string
			// representation.
			// Here we attempt to parse basic info for the raw string representation. Just enough to
			// decomposition into left and right parts for diagnostic.
			// Template parameters make C++ grammar ambiguous without type information. That being
			// said, many expressions can be disambiguated.
			// This code will make guesses about the grammar, essentially doing a traversal of all
			// possibly parse trees and looking for ones that could work. This is O(2^t) with the
			// where t is the number of potential templates. Usually t is small but this should
			// perhaps be limited.
			// Will return {"left", "right"} if unable to decompose unambiguously.
			// Some cases to consider
			//   tgt  expr
			//   ==   a < 1 == 2 > ( 1 + 3 )
			//   ==   a < 1 == 2 > - 3 == ( 1 + 3 )
			//   ==   ( 1 + 3 ) == a < 1 == 2 > - 3 // <- ambiguous
			//   ==   ( 1 + 3 ) == a < 1 == 2 > ()
			//   <    a<x<x<x<x<x<x<x<x<x<1>>>>>>>>>
			//   ==   1 == something<a == b>>2 // <- ambiguous
			//   <    1 == something<a == b>>2 // <- should be an error
			//   <    1 < something<a < b>>2 // <- ambiguous
			// If there is only one candidate from the parse trees considered, expression has been
			// disambiguated. If there are no candidates that's an error. If there is more than one
			// candidate the expression may be ambiguous, but, not necessarily. For example,
			// a < 1 == 2 > - 3 == ( 1 + 3 ) is split the same regardless of whether a < 1 == 2 > is
			// treated as a template or not:
			//   left:  a < 1 == 2 > - 3
			//   right: ( 1 + 3 )
			//   ---
			//   left:  a < 1 == 2 > - 3
			//   right: ( 1 + 3 )
			//   ---
			// Because we're just splitting an expression in half, instead of storing tokens for
			// both sides we can just store an index of the split.
			// Note: We don't need to worry about expressions where there is only a single term.
			// This will only be called on decomposable expressions.
			// Note: The >> to > > token breakdown needed in template parameter lists is done in the
			// initial tokenization so we can pass the token vector by reference and avoid copying
			// for every recursive path (O(t^2)). This does not create an issue for syntax
			// highlighting as long as >> and > are highlighted the same.
			// TODO: currently exploring all disambiguations, could exit early. Worst-case
			// complexity does not change though.
			const auto tokens = _tokenize(expression, true);
			// We're only looking for the split, we can just store a set of split indices. No need
			// to store a vector<pair<vector<token_t>, vector<token_t>>>
			std::set<int> candidates;
			pseudoparse(std::move(tokens), target_op, 0, 0, 0, -1, candidates);
			#ifdef _0_DEBUG_ASSERT_DISAMBIGUATION
			printf("\n%d\n", (int)candidates.size());
			for(size_t m : candidates) {
				std::vector<std::string> left_strings;
				std::vector<std::string> right_strings;
				for(size_t i = 0; i < m; i++) left_strings.push_back(tokens[i].str);
				for(size_t i = m + 1; i < tokens.size(); i++) right_strings.push_back(tokens[i].str);
				printf("left:  %s\n", highlight(std::string(trim(join(left_strings, "")))).c_str());
				printf("right: %s\n", highlight(std::string(trim(join(right_strings, "")))).c_str());
				printf("---\n");
			}
			#endif
			if(candidates.size() == 1) {
				std::vector<std::string> left_strings;
				std::vector<std::string> right_strings;
				size_t m = *candidates.begin();
				for(size_t i = 0; i < m; i++) left_strings.push_back(tokens[i].str);
				for(size_t i = m + 1; i < tokens.size(); i++) right_strings.push_back(tokens[i].str);
				return {
					std::string(trim(join(left_strings, ""))),
					std::string(trim(join(right_strings, "")))
				};
			} else {
				return { "left", "right" };
			}
		}

	public:
		// public static wrappers
		[[gnu::cold]]
		static std::string highlight(const std::string& expression) {
			#ifdef NCOLOR
			return expression;
			#else
			auto blocks = get()._highlight(expression);
			std::string str;
			for(auto& block : blocks) {
				str += block.color;
				str += block.content;
				if(!block.color.empty()) str += RESET;
			}
			return str;
			#endif
		}
		[[gnu::cold]]
		static std::vector<highlight_block> highlight_blocks(const std::string& expression) {
			#ifdef NCOLOR
			return expression;
			#else
			return get()._highlight(expression);
			#endif
		}
		[[gnu::cold]]
		static literal_format get_literal_format(const std::string& expression) {
			return get()._get_literal_format(expression);
		}
		[[gnu::cold]]
		static std::string trim_suffix(const std::string& expression) {
			return expression.substr(0, expression.find_last_not_of("FfUuLlZz") + 1);
		}
		[[gnu::cold]]
		static bool is_bitwise(std::string_view op) {
			return get().bitwise_operators.count(op);
		}
		[[gnu::cold]]
		static bool has_precedence_below_or_equal(const std::string& expression, const std::string_view op) {
			return get()._has_precedence_below_or_equal(expression, op);
		}
		[[gnu::cold]]
		static std::pair<std::string, std::string> decompose_expression(const std::string& expression, const std::string_view target_op) {
			return get()._decompose_expression(expression, target_op);
		}
	};

	static constexpr int log10(int n) {
		int t = 1;
		for(int i = 0; i < [] {
				int j = 0, n = std::numeric_limits<int>::max(); // note: `j` used instead of `i` because of https://bugs.llvm.org/show_bug.cgi?id=51986
				while(n /= 10) j++;
				return j;
			} () - 1; i++) {
			if(n <= t) return i;
			t *= 10;
		}
		return t;
	}

	using path_components = std::vector<std::string>;

	[[gnu::cold]] [[maybe_unused]]
	static path_components parse_path(const std::string_view path) {
		#ifdef _WIN32
		 constexpr std::string_view path_delim = "\\";
		#else
		 constexpr std::string_view path_delim = "/";
		#endif
		// Some cases to consider
		// projects/asserts/demo.cpp               projects   asserts  demo.cpp
		// /glibc-2.27/csu/../csu/libc-start.c  /  glibc-2.27 csu      libc-start.c
		// ./demo.exe                           .  demo.exe
		// ./../demo.exe                        .. demo.exe
		// ../x.hpp                             .. x.hpp
		// /foo/./x                             foo        x
		// /foo//x                              f          x
		path_components parts;
		for(std::string& part : split(path, path_delim)) {
			if(parts.empty()) {
				// first gets added no matter what
				parts.push_back(part);
			} else {
				if(part == "") {
					// nop
				} else if(part == ".") {
					// nop
				} else if(part == "..") {
					// cases where we have unresolvable ..'s, e.g. ./../../demo.exe
					if(parts.back() == "." || parts.back() == "..") {
						parts.push_back(part);
					} else {
						parts.pop_back();
					}
				} else {
					parts.push_back(part);
				}
			}
		}
		primitive_assert(!parts.empty());
		primitive_assert(parts.back() != "." && parts.back() != "..");
		return parts;
	}

	class path_trie {
		// Backwards path trie structure
		// e.g.:
		// a/b/c/d/e     disambiguate to -> c/d/e
		// a/b/f/d/e     disambiguate to -> f/d/e
		//  2   2   1   1   1
		// e - d - c - b - a
		//      \   1   1   1
		//       \ f - b - a
		// Nodes are marked with the number of downstream branches
		size_t downstream_branches = 1;
		std::string root;
		std::unordered_map<std::string, path_trie*> edges;
	public:
		path_trie(std::string root) : root(root) {};
		compl path_trie() {
			for(auto& [k, trie] : edges) {
				delete trie;
			}
		}
		path_trie(const path_trie&) = delete;
		path_trie(path_trie&& other) {
			downstream_branches = other.downstream_branches;
			root = other.root;
			for(auto& [k, trie] : edges) {
				delete trie;
			}
			edges = std::move(other.edges);
		}
		path_trie& operator=(const path_trie&) = delete;
		path_trie& operator=(path_trie&&) = delete;
		void insert(const path_components& path) {
			primitive_assert(path.back() == root);
			insert(path, (int)path.size() - 2);
		}
		path_components disambiguate(const path_components& path) {
			path_components result;
			path_trie* current = this;
			primitive_assert(path.back() == root);
			result.push_back(current->root);
			for(size_t i = path.size() - 2; i >= 1; i--) {
				primitive_assert(current->downstream_branches >= 1);
				if(current->downstream_branches == 1) break;
				const std::string& component = path[i];
				primitive_assert(current->edges.count(component));
				current = current->edges.at(component);
				result.push_back(current->root);
			}
			std::reverse(result.begin(), result.end());
			return result;
		}
	private:
		void insert(const path_components& path, int i) {
			if(i < 0) return;
			if(!edges.count(path[i])) {
				if(!edges.empty()) downstream_branches++; // this is to deal with making leaves have count 1
				edges.insert({path[i], new path_trie(path[i])});
			}
			downstream_branches -= edges.at(path[i])->downstream_branches;
			edges.at(path[i])->insert(path, i - 1);
			downstream_branches += edges.at(path[i])->downstream_branches;
		}
	};

	struct column_t {
		size_t width;
		std::vector<analysis::highlight_block> blocks;
	};

	struct cell_t {
		size_t length;
		std::string content;
	};

	static void wrapped_print(std::vector<column_t> columns) {
		// 2d array rows/columns
		std::vector<std::vector<cell_t>> lines;
		lines.push_back(std::vector<cell_t>(columns.size()));
		// populate one column at a time
		for(size_t i = 0; i < columns.size(); i++) {
			auto [width, blocks] = columns[i];
			size_t current_line = 0;
			for(auto& block : blocks) {
				size_t block_i = 0;
				// digest block
				while(block_i != block.content.size()) {
					if(lines.size() == current_line) lines.push_back(std::vector<cell_t>(columns.size()));
					// number of characters we can extract from the block
					size_t extract = std::min(width - lines[current_line][i].length, block.content.size() - block_i);
					primitive_assert(block_i + extract <= block.content.size());
					auto substr = std::string_view(block.content).substr(block_i, extract);
					// handle newlines
					if(auto x = substr.find('\n'); x != std::string_view::npos) {
						substr = substr.substr(0, x);
						extract = x + 1; // extract newline but don't print
					}
					// append
					lines[current_line][i].content += block.color;
					lines[current_line][i].content += substr;
					lines[current_line][i].content += block.color == "" ? "" : RESET;
					// advance
					block_i += extract;
					lines[current_line][i].length += extract;
					// new line if necessary
					// substr.size() != extract iff newline
					if(lines[current_line][i].length >= width || substr.size() != extract) current_line++;
				}
			}
		}
		// print
		for(auto& line : lines) {
			// don't print empty columns with no content in subsequent columns and more importantly
			// don't print empty spaces they'll mess up lines after terminal resizing even more
			size_t last_col = 0;
			for(size_t i = 0; i < line.size(); i++) {
				if(line[i].content != "") {
					last_col = i;
				}
			}
			for(size_t i = 0; i <= last_col; i++) {
				auto& cell = line[i];
				fprintf(stderr, "%s%-*s%s",
					cell.content.c_str(),
					i == last_col ? 0 : int(columns[i].width - cell.length), "",
					i == last_col ? "\n" : " ");
			}
		}
	}

	[[gnu::cold]]
	static void print_stacktrace() {
		if(auto _trace = get_stacktrace()) {
			auto trace = *_trace;
			// TODO: lower-bound too, or just include assert internals
			auto main = std::find_if(trace.rbegin(), trace.rend(),
				[](const assert_impl_::stacktrace_entry& e) {
					return e.signature == "main";
				});
			//size_t end = &*main - &trace.front(); // todo: what if main == .end()
			size_t end = trace.size() - 1; (void) main;
			// path preprocessing
			// raw full path -> components
			std::unordered_map<std::string, path_components> components;
			// file name -> trie
			std::unordered_map<std::string, path_trie> file_tries;
			for(size_t i = 0; i <= end; i++) {
				const auto& [filename, _1, _2] = trace[i];
				auto path = parse_path(filename);
				if(!components.count(filename)) {
					components.insert({filename, path});
					if(!file_tries.count(path.back())) {
						file_tries.insert(std::make_pair(path.back(), path_trie(path.back())));
					}
					file_tries.at(path.back()).insert(path);
				}
			}
			// raw full path -> minified path
			std::unordered_map<std::string, std::string> files;
			size_t max_file_length = 0;
			for(auto& [raw, path] : components) {
				primitive_assert(!files.count(raw));
				//std::string new_path = raw;
				std::string new_path = join(file_tries.at(path.back()).disambiguate(path), "/");
				files.insert({raw, new_path});
				if(new_path.size() > max_file_length) max_file_length = new_path.size();
			}
			max_file_length = std::min(max_file_length, size_t(80));
			int max_line_numbers_length = log10(std::max_element(trace.begin(), trace.begin() + end,
				[](const assert_impl_::stacktrace_entry& a, const assert_impl_::stacktrace_entry& b) {
					return std::to_string(a.line).size() < std::to_string(b.line).size();
				})->line);
			int frame_offset = 0;
			int term_width = terminal_width(); // will be 0 on error
			for(size_t i = 0; i <= end; i++) {
				const auto& [filename, signature, _line] = trace[i];
				std::string line = _line == 0 ? "?" : std::to_string(_line);
				int recursion_folded = 0;
				if(end - i >= 4) {
					size_t j = 1;
					for( ; i + j <= end; j++) {
						if(trace[i + j] != trace[i] || trace[i + j].signature == "??") break;
					}
					if(j >= 4) {
						trace.erase(trace.begin() + i + 1, trace.begin() + i + j - 1);
						end -= j - 2;
						recursion_folded = j - 2;
					}
				}
				int frame_number = i + 1 + frame_offset;
				// pretty print with columns for wide terminals
				// split printing for small terminals
				if(term_width >= 50) {
					auto sig = analysis::highlight_blocks(signature + "("); // hack for the highlighter
					sig.pop_back();
					std::string frame = std::to_string(frame_number);
					size_t left = 2 + std::max((int)frame.length(), 2);
					size_t middle = std::max((int)line.size(), max_line_numbers_length);
					//constexpr size_t max_file_length = 20;
					size_t remaining_width = term_width - (left + middle + 3 /* spaces */);
					primitive_assert(remaining_width >= 2);
					size_t file_width = std::min(max_file_length, remaining_width / 2);
					size_t sig_width = remaining_width - file_width;
					wrapped_print({
						{ left,       {{"", (frame_number < 10 ? "#  " : "# ") + frame}} },
						{ file_width, {{"", files.at(filename)}} },
						{ middle,     analysis::highlight_blocks(line) }, // intentionally not coloring "?"
						{ sig_width,  sig }
					});
				} else {
					auto sig = analysis::highlight(signature + "("); // hack for the highlighter
					sig = sig.substr(0, sig.rfind("("));
					fprintf(
						stderr, "#%2d %s\n      at %s:%s\n",
						frame_number,
						sig.c_str(),
						filename.c_str(),
						(CYAN + line + RESET).c_str() // yes this is excessive and intentionally coloring "?"
					);
				}
				if(recursion_folded) {
					frame_offset += recursion_folded;
					std::string s = stringf("| %d layers of recursion were folded |", recursion_folded);
					fprintf(stderr, BLUE "|%*s|" RESET "\n", int(s.size() - 2), "");
					fprintf(stderr, BLUE  "%s"   RESET "\n", s.c_str());
					fprintf(stderr, BLUE "|%*s|" RESET "\n", int(s.size() - 2), "");
				}
			}
		} else {
			fprintf(stderr, "Error: Unable to generate stacktrace.");
		}
	}

	[[gnu::cold]]
	static std::string parenthesize_if_necessary(const std::string& expression, const std::string_view op) {
		if(analysis::has_precedence_below_or_equal(expression, op)) {
			return "(" + expression + ")";
		} else {
			return expression;
		}
	}

	[[gnu::cold]] [[maybe_unused]]
	static std::string gen_assert_binary(std::string a_str, const std::string_view op, std::string b_str) {
		return stringf("assert(%s %s %s);",
					   parenthesize_if_necessary(a_str, op).c_str(),
					   std::string(op).c_str(), // string_view isn't null-terminated; TODO: better solution?
					   parenthesize_if_necessary(b_str, op).c_str());
	}

	template<typename T>
	[[gnu::cold]]
	std::vector<std::string> generate_stringifications(T&& v, const std::set<literal_format>& formats) {
		if constexpr(std::is_arithmetic<strip<T>>::value
				 && !std::is_same_v<strip<T>, bool>
				 && !std::is_same_v<strip<T>, char>) {
			std::vector<std::string> vec;
			for(literal_format fmt : formats) {
				// TODO: consider pushing empty fillers to keep columns aligned later on? Does not
				// matter at the moment because floats only have decimal and hex literals but could
				// if more formats are added.
				std::string str = stringify(v, fmt);
				if(!str.empty()) vec.push_back(std::move(str));
			}
			return vec;
		} else {
			return { stringify(v) };
		}
	}

	[[gnu::cold]] [[maybe_unused]]
	static void print_values(const std::vector<std::string>& vec, size_t lw) {
		primitive_assert(vec.size() > 0);
		if(vec.size() == 1) {
			fprintf(stderr, "%s\n", indent(analysis::highlight(vec[0]), 8 + lw + 4, ' ', true).c_str());
		} else {
			// spacing here done carefully to achieve <expr> =  <a>  <b>  <c>, or similar
			// no indentation done here for multiple value printing
			fprintf(stderr, " ");
			for(const auto& str : vec) {
				fprintf(stderr, "%s", analysis::highlight(str).c_str());
				if(str != *vec.end()--) fprintf(stderr, "  ");
			}
			fprintf(stderr, "\n");
		}
	}
	[[gnu::cold]] [[maybe_unused]]
	static std::vector<analysis::highlight_block> get_values(const std::vector<std::string>& vec) {
		primitive_assert(vec.size() > 0);
		if(vec.size() == 1) {
			return analysis::highlight_blocks(vec[0]);
		} else {
			std::vector<analysis::highlight_block> blocks;
			// spacing here done carefully to achieve <expr> =  <a>  <b>  <c>, or similar
			// no indentation done here for multiple value printing
			blocks.push_back({"", " "});
			for(const auto& str : vec) {
				auto h = analysis::highlight_blocks(str);
				blocks.insert(blocks.end(), h.begin(), h.end());
				if(str != *vec.end()--) blocks.push_back({"", "  "});
			}
			return blocks;
		}
	}

	template<typename A, typename B>
	[[gnu::cold]]
	void print_binary_diagnostic(A&& a, B&& b, const char* a_str, const char* b_str, std::string_view op) {
		// Note: op
		// figure out what information we need to print in the where clause
		// find all literal formats involved (literal_format::dec included for everything)
		literal_format lformat = analysis::get_literal_format(a_str);
		literal_format rformat = analysis::get_literal_format(b_str);
		// std::set used so formats are printed in a specific order
		std::set<literal_format> formats = { literal_format::dec, lformat, rformat };
		formats.erase(literal_format::none); // none is just for when the expression isn't a literal
		if(analysis::is_bitwise(op)) formats.insert(literal_format::binary); // always display binary for bitwise
		primitive_assert(formats.size() > 0);
		// generate raw strings for given formats, without highlighting
		std::vector<std::string> lstrings = generate_stringifications(std::forward<A>(a), formats);
		std::vector<std::string> rstrings = generate_stringifications(std::forward<B>(b), formats);
		// pad all columns where there is overlap
		for(size_t i = 0; i < std::min(lstrings.size(), rstrings.size()); i++) {
			// find which clause, left or right, we're padding (entry i)
			std::vector<std::string>& which = lstrings[i].length() < rstrings[i].length() ? lstrings : rstrings;
			int difference = std::abs((int)lstrings[i].length() - (int)rstrings[i].length());
			if(i != which.size() - 1) { // last column excluded as padding is not necessary at the end of the line
				which[i].insert(which[i].end(), difference, ' ');
			}
		}
		primitive_assert(lstrings.size() > 0);
		primitive_assert(rstrings.size() > 0);
		// determine whether we actually gain anything from printing a where clause (e.g. exclude "1 => 1")
		struct { bool left, right; } has_useful_where_clause = {
			formats.size() > 1 || lstrings.size() > 1 || (a_str != lstrings[0] && analysis::trim_suffix(a_str) != lstrings[0]),
			formats.size() > 1 || rstrings.size() > 1 || (b_str != rstrings[0] && analysis::trim_suffix(b_str) != rstrings[0])
		};
		// print where clause
		if(has_useful_where_clause.left || has_useful_where_clause.right) {
			size_t lw = std::max(
				has_useful_where_clause.left  ? strlen(a_str) : 0,
				has_useful_where_clause.right ? strlen(b_str) : 0
			);
			size_t term_width = terminal_width(); // will be 0 on error
			// Limit lw to about half the screen. TODO: Re-evaluate what we want to do here.
			if(term_width > 0) lw = std::min(lw, term_width / 2 - 8 /* indent */ - 4 /* arrow */);
			fprintf(stderr, "    Where:\n");
			if(has_useful_where_clause.left) {
				if(term_width >= 50) {
					wrapped_print({
						{ 7, {{"", ""}} }, // 8 space indent, wrapper will add a space
						{ lw, analysis::highlight_blocks(a_str) },
						{ 2, {{"", "=>"}} },
						{ term_width - lw - 8 /* indent */ - 4 /* arrow */, get_values(lstrings) }
					});
				} else {
					fprintf(stderr, "        %s%*s => ",
							analysis::highlight(a_str).c_str(), int(lw - strlen(a_str)), "");
						print_values(lstrings, lw);
					//fprintf(stderr, "%s\n", join(get_values(lstrings)))
				}
			}
			if(has_useful_where_clause.right) {
				if(term_width >= 50) {
					wrapped_print({
						{ 7, {{"", ""}} }, // 8 space indent, wrapper will add a space
						{ lw, analysis::highlight_blocks(b_str) },
						{ 2, {{"", "=>"}} },
						{ term_width - lw - 8 /* indent */ - 4 /* arrow */, get_values(rstrings) }
					});
				} else {
					fprintf(stderr, "        %s%*s => ",
							analysis::highlight(b_str).c_str(), int(lw - strlen(b_str)), "");
						print_values(rstrings, lw);
					//fprintf(stderr, "%s\n", join(get_values(rstrings)))
				}
			}
		}
	}

	// allow non-fatal assertions
	enum class ASSERT {
		NONFATAL, FATAL
	};

	[[gnu::cold]] [[maybe_unused]]
	static void fail() {
		if(isatty(STDIN_FILENO) && isatty(STDERR_FILENO)) {
			//fprintf(stderr, "\n    Process is suspended, run gdb -p %d to attach\n", getpid());
			//fprintf(stderr,   "    Press any key to continue\n\n");
			//wait_for_keypress();
		}
		#ifdef _0_ASSERT_DEMO
		printf("\n");
		#else
		fflush(stdout);
		fflush(stderr);
		abort();
		#endif
	}

	template<typename A, typename B, typename C>
	[[gnu::cold]] [[gnu::noinline]]
	void assert_fail(expression_decomposer<A, B, C>& decomposer, const char* expr_str, const char* pretty_func, const char* info, ASSERT fatal, source_location location) {
		if(info == nullptr) {
			fprintf(stderr, "Assertion failed at %s:%d: %s:\n", location.file, location.line, pretty_func);
		} else {
			fprintf(stderr, "Assertion failed at %s:%d: %s: %s\n", location.file, location.line, pretty_func, info);
		}
		fprintf(stderr, "    %s\n", analysis::highlight(stringf("assert(%s);", expr_str)).c_str());
		if constexpr(is_nothing<C>) {
			static_assert(is_nothing<B> && !is_nothing<A>);
		} else {
			auto [a_str, b_str] = analysis::decompose_expression(expr_str, C::op_string);
			print_binary_diagnostic(std::forward<A>(decomposer.a), std::forward<B>(decomposer.b), a_str.c_str(), b_str.c_str(), C::op_string);
		}
		fprintf(stderr, "\nStack trace:\n");
		print_stacktrace();
		if(fatal == ASSERT::FATAL) {
			fail();
		}
	}

	template<typename C, typename A, typename B>
	[[gnu::cold]] [[gnu::noinline]]
	void assert_binary_fail(A&& a, B&& b, const char* a_str, const char* b_str, const char* pretty_func, const char* info, ASSERT fatal, source_location location) {
		if(info == nullptr) {
			fprintf(stderr, "Assertion failed at %s:%d: %s:\n", location.file, location.line, pretty_func);
		} else {
			fprintf(stderr, "Assertion failed at %s:%d: %s: %s\n", location.file, location.line, pretty_func, info);
		}
		fprintf(stderr, "    %s\n", analysis::highlight(gen_assert_binary(a_str, C::op_string, b_str)).c_str());
		print_binary_diagnostic(std::forward<A>(a), std::forward<B>(b), a_str, b_str, C::op_string);
		fprintf(stderr, "\nStack trace:\n");
		print_stacktrace();
		if(fatal == ASSERT::FATAL) {
			fail();
		}
	}

	// top-level assert functions emplaced by the macros
	// these are the only non-cold functions

	template<typename A, typename B, typename C>
	void assert(expression_decomposer<A, B, C> decomposer, const char* expr_str, const char* pretty_func, const char* info = nullptr, ASSERT fatal = ASSERT::FATAL, source_location location = {}) {
		if(!decomposer.get_value()) {
			enable_virtual_terminal_processing_if_needed();
			// todo: forward decomposer?
			assert_fail(decomposer, expr_str, analysis::highlight(pretty_func).c_str(), info, fatal, location);
		} else {
			#ifdef _0_ASSERT_DEMO
			assert(expression_decomposer(false), "false", __PRETTY_FUNCTION__, "assert should have failed");
			#endif
		}
	}

	template<typename C, typename A, typename B>
	void assert_binary(A&& a, B&& b, const char* a_str, const char* b_str, const char* pretty_func, const char* info = nullptr, ASSERT fatal = ASSERT::FATAL, source_location location = {}) {
		if(!(bool)C()(a, b)) {
			enable_virtual_terminal_processing_if_needed();
			assert_binary_fail<C>(std::forward<A>(a), std::forward<B>(b), a_str, b_str, analysis::highlight(pretty_func).c_str(), info, fatal, location);
		} else {
			#ifdef _0_ASSERT_DEMO
			assert(expression_decomposer(false), "false", __PRETTY_FUNCTION__, "assert should have failed");
			#endif
		}
	}

	// std::string info overloads

	template<typename A, typename B, typename C>
	void assert(expression_decomposer<A, B, C> decomposer, const char* expr_str, const char* pretty_func, const std::string& info, ASSERT fatal = ASSERT::FATAL, source_location location = {}) {
		assert(std::forward<expression_decomposer<A, B, C>>(decomposer), expr_str, pretty_func, info.c_str(), fatal, location);
	}

	template<typename C, typename A, typename B>
	void assert_binary(A&& a, B&& b, const char* a_str, const char* b_str, const char* pretty_func, const std::string& info, ASSERT fatal = ASSERT::FATAL, source_location location = {}) {
		assert(std::forward<A>(a), std::forward<B>(b), a_str, b_str, pretty_func, info.c_str(), fatal, location);
	}

	[[maybe_unused]]
	static void assume(bool expr) {
		if(!expr) {
			__builtin_unreachable();
		}
	}

	template<typename C, typename A, typename B>
	void assume(A&& a, B&& b) {
		assume(C()(a, b));
	}

	#undef primitive_assert
}

#ifndef NCOLOR
 #undef ESC
 #undef ANSIRGB
#endif
#undef RED
#undef ORANGE
#undef YELLOW
#undef GREEN
#undef BLUE
#undef CYAN
#undef PURPL
#undef RESET

using assert_impl_::ASSERT;

#ifdef NDEBUG
 #ifndef ASSUME_ASSERTS
  #define      assert(...) (void)0
  #define   assert_eq(...) (void)0
  #define  assert_neq(...) (void)0
  #define   assert_lt(...) (void)0
  #define   assert_gt(...) (void)0
  #define assert_lteq(...) (void)0
  #define assert_gteq(...) (void)0
  #define  assert_and(...) (void)0
  #define   assert_or(...) (void)0
 #else
  // Note: This is not a default because Sometimes assertion expressions have side effects that are
  // undesirable at runtime in an `NDEBUG` build like exceptions on std container operations which
  // cannot be optimized away. These often end up not being helpful hints either.
  // TODO: Need to feed through decomposer to preserve sign-unsigned safety on top-level compare?
  #define      assert(expr, ...) assert_impl_::assume(expr)
  #define   assert_eq(a, b, ...) assert_impl_::assume<assert_impl_::equal_to     >(a, b)
  #define  assert_neq(a, b, ...) assert_impl_::assume<assert_impl_::not_equal_to >(a, b)
  #define   assert_lt(a, b, ...) assert_impl_::assume<assert_impl_::less         >(a, b)
  #define   assert_gt(a, b, ...) assert_impl_::assume<assert_impl_::greater      >(a, b)
  #define assert_lteq(a, b, ...) assert_impl_::assume<assert_impl_::less_equal   >(a, b)
  #define assert_gteq(a, b, ...) assert_impl_::assume<assert_impl_::greater_equal>(a, b)
  #define  assert_and(a, b, ...) assert_impl_::assume<assert_impl_::logical_and  >(a, b)
  #define   assert_or(a, b, ...) assert_impl_::assume<assert_impl_::logical_or   >(a, b)
 #endif
#else
 // __PRETTY_FUNCTION__ used because __builtin_FUNCTION() used in source_location (like __FUNCTION__) is just the method name, not signature
 // assert_impl_::expression_decomposer(assert_impl_::expression_decomposer{}) done for ternary support
 #define      assert(expr, ...) _Pragma("GCC diagnostic ignored \"-Wparentheses\"") \
                                assert_impl_::assert(assert_impl_::expression_decomposer(assert_impl_::expression_decomposer{} << expr), \
                                                                                                #expr, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define   assert_eq(a, b, ...) assert_impl_::assert_binary<assert_impl_::equal_to     >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define  assert_neq(a, b, ...) assert_impl_::assert_binary<assert_impl_::not_equal_to >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define   assert_lt(a, b, ...) assert_impl_::assert_binary<assert_impl_::less         >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define   assert_gt(a, b, ...) assert_impl_::assert_binary<assert_impl_::greater      >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define assert_lteq(a, b, ...) assert_impl_::assert_binary<assert_impl_::less_equal   >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define assert_gteq(a, b, ...) assert_impl_::assert_binary<assert_impl_::greater_equal>(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define  assert_and(a, b, ...) assert_impl_::assert_binary<assert_impl_::logical_and  >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define   assert_or(a, b, ...) assert_impl_::assert_binary<assert_impl_::logical_or   >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#endif

#endif
