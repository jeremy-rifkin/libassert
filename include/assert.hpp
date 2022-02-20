#ifndef ASSERT_HPP
#define ASSERT_HPP

// Jeremy Rifkin 2021, 2022
// https://github.com/jeremy-rifkin/asserts

#include <iomanip>
#include <limits>
#include <sstream>
#include <stdio.h>
#include <string_view>
#include <string.h>
#include <string>
#include <vector>

#if defined(__clang__)
 #define IS_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
 #define IS_GCC 1
#else
 #error "no"
#endif
#if defined(_WIN32)
 #define IS_WINDOWS 1
 #ifndef STDIN_FILENO
  #define STDIN_FILENO _fileno(stdin)
  #define STDOUT_FILENO _fileno(stdout)
  #define STDERR_FILENO _fileno(stderr)
 #endif
#elif defined(__linux)
 #define IS_LINUX 1
 #include <unistd.h>
#else
 #error "no"
#endif

#if defined(__clang__) && __clang_major__ >= 11 || __GNUC__ >= 9
 #define strong_expect(expr, value) __builtin_expect_with_probability((expr), (value), 1)
#else
 #define strong_expect(expr, value) __builtin_expect((expr), (value))
#endif

// if defined by a previous #include...
#ifdef assert
 #undef assert
#endif

#ifndef NCOLOR
 #define ESC "\033["
 #define ANSIRGB(r, g, b) ESC "38;2;" #r ";" #g ";" #b "m"
 #define RESET ESC "0m"
 // Slightly modified one dark pro colors
 // Original: https://coolors.co/e06b74-d19a66-e5c07a-98c379-62aeef-55b6c2-c678dd
 // Modified: https://coolors.co/e06b74-d19a66-e5c07a-90cc66-62aeef-56c2c0-c678dd
 #define RED ANSIRGB(224, 107, 116)
 #define ORANGE ANSIRGB(209, 154, 102)
 #define YELLOW ANSIRGB(229, 192, 122)
 #define GREEN ANSIRGB(150, 205, 112) // modified
 #define BLUE ANSIRGB(98, 174, 239)
 #define CYAN ANSIRGB(86, 194, 192) // modified
 #define PURPL ANSIRGB(198, 120, 221)
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

namespace assert_detail {
	enum class ASSERTION {
		NONFATAL, FATAL
	};

	enum class assert_type {
		assertion,
		verify,
		check
	};
}

#ifdef ASSERT_FAIL
 void ASSERT_FAIL(std::string message, assert_detail::assert_type type, assert_detail::ASSERTION fatal);
#endif

#define PHONY_USE(E) do { using x [[maybe_unused]] = decltype(E); } while(0)

namespace assert_detail {
	// Lightweight helper, eventually may use C++20 std::source_location if this library no longer
	// targets C++17. Note: __builtin_FUNCTION only returns the name, so __PRETTY_FUNCTION__ is
	// still needed.
	struct source_location {
		const char* const file;
		const char* const function;
		const int line;
		constexpr source_location(
			const char* function /*= __builtin_FUNCTION()*/,
			const char* file     = __builtin_FILE(),
			int line             = __builtin_LINE()
		) : file(file), function(function), line(line) {}
	};

	// bootstrap with primitive implementations
	void primitive_assert_impl(bool condition, bool verify, const char* expression,
	                           source_location location, const char* message = nullptr);

	#ifndef NDEBUG
	 #define primitive_assert(c, ...) primitive_assert_impl(c, false, #c, __extension__ __PRETTY_FUNCTION__, ##__VA_ARGS__)
	#else
	 #define primitive_assert(c, ...) PHONY_USE(c)
	#endif

	// Still present in release mode, nonfatal
	#define internal_verify(c, ...) primitive_assert_impl(c, true, #c, __extension__ __PRETTY_FUNCTION__, ##__VA_ARGS__)

	/*
	 * string utilities
	 */
	[[nodiscard]] std::string bstringf(const char* format, ...);

	[[nodiscard]] std::string indent(const std::string_view str, size_t depth, char c = ' ', bool ignore_first = false);

	/*
	 * system wrappers
	 */

	void enable_virtual_terminal_processing_if_needed();

	#ifdef _WIN32
	typedef int pid_t;
	#endif

	[[nodiscard]] pid_t getpid();

	void wait_for_keypress();

	[[nodiscard]] bool isatty(int fd);

	// https://stackoverflow.com/questions/23369503/get-size-of-terminal-window-rows-columns
	[[nodiscard]] int terminal_width();

	[[nodiscard]] std::string strerror_wrapper(int err); // stupid C stuff, stupid microsoft stuff

	/*
	 * Stacktrace implementation
	 */

	// All in the .cpp

	/*
	 * Expression decomposition
	 */

	struct nothing {};

	template<typename T> constexpr bool is_nothing = std::is_same_v<T, nothing>;

	// Hack to get around static_assert(false); being evaluated before any instantiation, even under
	// an if-constexpr branch
	template<typename T> constexpr bool always_false = false;

	template<typename T> using strip = std::remove_cv_t<std::remove_reference_t<T>>;

	template<typename A, typename B> constexpr bool isa = std::is_same_v<strip<A>, B>; // intentionally not stripping B

	// Is integral but not boolean
	template<typename T> constexpr bool is_integral_and_not_bool = std::is_integral_v<strip<T>> && !isa<T, bool>;

	// Lots of boilerplate
	// Using int comparison functions here to support proper signed comparisons. Need to make sure
	// assert(map.count(1) == 2) doesn't produce a warning. It wouldn't under normal circumstances
	// but it would in this library due to the parameters being forwarded down a long chain.
	// And we want to provide as much robustness as possible anyways.
	// Copied and pasted from https://en.cppreference.com/w/cpp/utility/intcmp
	// Not using std:: versions because library is targetting C++17
	template<typename T, typename U>
	[[gnu::cold]] [[nodiscard]]
	constexpr bool cmp_equal(T t, U u) {
		using UT = std::make_unsigned_t<T>;
		using UU = std::make_unsigned_t<U>;
		if constexpr(std::is_signed_v<T> == std::is_signed_v<U>)
			return t == u;
		else if constexpr(std::is_signed_v<T>)
			return t < 0 ? false : UT(t) == u;
		else
			return u < 0 ? false : t == UU(u);
	}

	template<typename T, typename U>
	[[gnu::cold]] [[nodiscard]]
	constexpr bool cmp_not_equal(T t, U u) {
		return !cmp_equal(t, u);
	}

	template<typename T, typename U>
	[[gnu::cold]] [[nodiscard]]
	constexpr bool cmp_less(T t, U u) {
		using UT = std::make_unsigned_t<T>;
		using UU = std::make_unsigned_t<U>;
		if constexpr(std::is_signed_v<T> == std::is_signed_v<U>)
			return t < u;
		else if constexpr(std::is_signed_v<T>)
			return t < 0 ? true : UT(t) < u;
		else
			return u < 0 ? false : t < UU(u);
	}

	template<typename T, typename U>
	[[gnu::cold]] [[nodiscard]]
	constexpr bool cmp_greater(T t, U u) {
		return cmp_less(u, t);
	}

	template<typename T, typename U>
	[[gnu::cold]] [[nodiscard]]
	constexpr bool cmp_less_equal(T t, U u) {
		return !cmp_greater(t, u);
	}

	template<typename T, typename U>
	[[gnu::cold]] [[nodiscard]]
	constexpr bool cmp_greater_equal(T t, U u) {
		return !cmp_less(t, u);
	}

	// Lots of boilerplate
	// std:: implementations don't allow two separate types for lhs/rhs
	// Note: is this macro potentially bad when it comes to debugging(?)
	namespace ops {
		#define gen_op_boilerplate(name, op) struct name { \
			static constexpr std::string_view op_string = #op; \
			template<typename A, typename B> \
			[[gnu::cold]] [[nodiscard]] \
			constexpr auto operator()(A&& lhs, B&& rhs) const { /* no need to forward ints */ \
				return std::forward<A>(lhs) op std::forward<B>(rhs); \
			} \
		}
		#define gen_op_boilerplate_special(name, op, cmp) struct name { \
			static constexpr std::string_view op_string = #op; \
			template<typename A, typename B> \
			[[gnu::cold]] [[nodiscard]] \
			constexpr auto operator()(A&& lhs, B&& rhs) const { /* no need to forward ints */ \
				if constexpr(is_integral_and_not_bool<A> && is_integral_and_not_bool<B>) return cmp(lhs, rhs); \
				else return std::forward<A>(lhs) op std::forward<B>(rhs); \
			} \
		}
		gen_op_boilerplate(shl, <<);
		gen_op_boilerplate(shr, >>);
		gen_op_boilerplate_special(eq,   ==, cmp_equal);
		gen_op_boilerplate_special(neq,  !=, cmp_not_equal);
		gen_op_boilerplate_special(gt,    >, cmp_greater);
		gen_op_boilerplate_special(lt,    <, cmp_less);
		gen_op_boilerplate_special(gteq, >=, cmp_greater_equal);
		gen_op_boilerplate_special(lteq, <=, cmp_less_equal);
		gen_op_boilerplate(band,   &);
		gen_op_boilerplate(bxor,   ^);
		gen_op_boilerplate(bor,    |);
		gen_op_boilerplate(land,   &&);
		gen_op_boilerplate(lor,    ||);
		gen_op_boilerplate(assign, =);
		gen_op_boilerplate(add_assign,  +=);
		gen_op_boilerplate(sub_assign,  -=);
		gen_op_boilerplate(mul_assign,  *=);
		gen_op_boilerplate(div_assign,  /=);
		gen_op_boilerplate(mod_assign,  %=);
		gen_op_boilerplate(shl_assign,  <<=);
		gen_op_boilerplate(shr_assign,  >>=);
		gen_op_boilerplate(band_assign, &=);
		gen_op_boilerplate(bxor_assign, ^=);
		gen_op_boilerplate(bor_assign,  |=);
		#undef gen_op_boilerplate
	}

	// I learned this automatic expression decomposition trick from lest:
	// https://github.com/martinmoene/lest/blob/master/include/lest/lest.hpp#L829-L853
	//
	// I have improved upon the trick by supporting more operators and generally improving
	// functionality.
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
		// not copyable
		expression_decomposer(const expression_decomposer&) = delete;
		expression_decomposer& operator=(const expression_decomposer&) = delete;
		// allow move assignment
		expression_decomposer(expression_decomposer&&) = default;
		expression_decomposer& operator=(expression_decomposer&&) = delete;
		// value constructors
		template<typename U>
		explicit expression_decomposer(U&& a) : a(std::forward<U>(a)) {}
		template<typename U, typename V>
		explicit expression_decomposer(U&& a, V&& b) : a(std::forward<U>(a)), b(std::forward<V>(b)) {}
		/* Ownership logic:
		 *  One of two things can happen to this class
		 *   - Either it is composed with another operation
		 * 	    + The value of the subexpression represented by this is computed (either get_value()
		 *        or operator bool), either A& or C()(a, b)
		 *      + Or, just the lhs is moved B is nothing
		 *   - Or this class represents the whole expression
		 *      + The value is computed (either A& or C()(a, b))
		 *      + a and b are referenced freely
		 *      + Either the value is taken or a is moved out
		 * Ensuring the value is only computed once is left to the assert handler
		 */
		[[nodiscard]]
		decltype(auto) get_value() {
			if constexpr(is_nothing<C>) {
				static_assert(is_nothing<B> && !is_nothing<A>);
				return (((((((((((((((((((a)))))))))))))))))));
			} else {
				return C()(a, b);
			}
		}
		[[nodiscard]]
		operator bool() { // for ternary support
			return (bool)get_value();
		}
		// return true if the lhs should be returned, false if full computed value should be
		[[nodiscard]]
		constexpr bool ret_lhs() {
			static_assert(!is_nothing<A>);
			static_assert(is_nothing<B> == is_nothing<C>);
			if constexpr(is_nothing<C>) {
				// if there is no top-level binary operation, A is the only thing that can be returned
				return true;
			} else {
				if constexpr(
					C::op_string == "=="
				 || C::op_string == "!="
				 || C::op_string == "<"
				 || C::op_string == ">"
				 || C::op_string == "<="
				 || C::op_string == ">="
				 || C::op_string == "&&"
				 || C::op_string == "||"
				) {
					return true;
				} else {
					// might change these later
					// << >> & ^ |
					// all compound assignments
					return false;
				}
			}
		}
		[[nodiscard]]
		A take_lhs() { // should only be called if ret_lhs() == true
			if constexpr(std::is_lvalue_reference<A>::value) {
				return ((((a))));
			} else {
				return std::move(a);
			}
		}
		// Need overloads for operators with precedence <= bitshift
		// TODO: spaceship operator?
		// Note: Could decompose more than just comparison and boolean operators, but it would take
		// a lot of work and I don't think it's beneficial for this library.
		template<typename O> [[nodiscard]] auto operator<<(O&& operand) && {
			using Q = std::conditional_t<std::is_rvalue_reference_v<O>, std::remove_reference_t<O>, O>;
			if constexpr(is_nothing<A>) {
				static_assert(is_nothing<B> && is_nothing<C>);
				return expression_decomposer<Q, nothing, nothing>(std::forward<O>(operand));
			} else if constexpr(is_nothing<B>) {
				static_assert(is_nothing<C>);
				return expression_decomposer<A, Q, ops::shl>(std::forward<A>(a), std::forward<O>(operand));
			} else {
				static_assert(!is_nothing<C>);
				return expression_decomposer<decltype(get_value()), O, ops::shl>(std::forward<A>(get_value()), std::forward<O>(operand));
			}
		}
		#define gen_op_boilerplate(functor, op) template<typename O> [[nodiscard]] auto operator op(O&& operand) && { \
			static_assert(!is_nothing<A>); \
			using Q = std::conditional_t<std::is_rvalue_reference_v<O>, std::remove_reference_t<O>, O>; \
			if constexpr(is_nothing<B>) { \
				static_assert(is_nothing<C>); \
				return expression_decomposer<A, Q, functor>(std::forward<A>(a), std::forward<O>(operand)); \
			} else { \
				static_assert(!is_nothing<C>); \
				return expression_decomposer<decltype(get_value()), Q, functor>(std::forward<A>(get_value()), std::forward<O>(operand)); \
			} \
		}
		gen_op_boilerplate(ops::shr, >>)
		gen_op_boilerplate(ops::eq, ==)
		gen_op_boilerplate(ops::neq, !=)
		gen_op_boilerplate(ops::gt, >)
		gen_op_boilerplate(ops::lt, <)
		gen_op_boilerplate(ops::gteq, >=)
		gen_op_boilerplate(ops::lteq, <=)
		gen_op_boilerplate(ops::band, &)
		gen_op_boilerplate(ops::bxor, ^)
		gen_op_boilerplate(ops::bor, |)
		gen_op_boilerplate(ops::land, &&)
		gen_op_boilerplate(ops::lor, ||)
		gen_op_boilerplate(ops::assign, =)
		gen_op_boilerplate(ops::add_assign, +=)
		gen_op_boilerplate(ops::sub_assign, -=)
		gen_op_boilerplate(ops::mul_assign, *=)
		gen_op_boilerplate(ops::div_assign, /=)
		gen_op_boilerplate(ops::mod_assign, %=)
		gen_op_boilerplate(ops::shl_assign, <<=)
		gen_op_boilerplate(ops::shr_assign, >>=)
		gen_op_boilerplate(ops::band_assign, &=)
		gen_op_boilerplate(ops::bxor_assign, ^=)
		gen_op_boilerplate(ops::bor_assign, |=)
		#undef gen_op_boilerplate
	};

	#ifndef NDEBUG
	 static_assert(std::is_same<decltype(std::declval<expression_decomposer<int, nothing, nothing>>().get_value()), int&>::value);
	 static_assert(std::is_same<decltype(std::declval<expression_decomposer<int&, nothing, nothing>>().get_value()), int&>::value);
	 static_assert(std::is_same<decltype(std::declval<expression_decomposer<int, int, ops::lteq>>().get_value()), bool>::value);
	 static_assert(std::is_same<decltype(std::declval<expression_decomposer<int, int, ops::lteq>>().take_lhs()), int>::value);
	 static_assert(std::is_same<decltype(std::declval<expression_decomposer<int&, int, ops::lteq>>().take_lhs()), int&>::value);
	#endif

	// for ternary support
	template<typename U> expression_decomposer(U&&)
	         -> expression_decomposer<std::conditional_t<std::is_rvalue_reference_v<U>, std::remove_reference_t<U>, U>>;

	/*
	 * C++ syntax analysis logic
	 */

	enum class literal_format {
		dec,
		hex,
		octal,
		binary,
		none
	};

	struct highlight_block {
		std::string_view color;
		std::string content;
		// Get as much code into the .cpp as possible
		highlight_block(std::string_view, std::string);
		highlight_block(const highlight_block&);
		highlight_block(highlight_block&&);
		~highlight_block();
		highlight_block& operator=(const highlight_block&);
		highlight_block& operator=(highlight_block&&);
	};

	[[nodiscard]] std::string prettify_type(std::string type);

	[[nodiscard]] std::string highlight(const std::string& expression);

	[[nodiscard]] std::vector<highlight_block> highlight_blocks(const std::string& expression);

	[[nodiscard]] literal_format get_literal_format(const std::string& expression);

	[[nodiscard]] std::string trim_suffix(const std::string& expression);

	[[nodiscard]] bool is_bitwise(std::string_view op);

	[[nodiscard]] std::pair<std::string, std::string> decompose_expression(const std::string& expression,
	                                                                       const std::string_view target_op);

	/*
	 * stringification
	 */

	[[nodiscard]] std::string escape_string(const std::string_view str, char quote);

	[[nodiscard]] std::string_view substring_bounded_by(std::string_view sig, std::string_view l, std::string_view r);

	template<typename T>
	[[gnu::cold]] [[nodiscard]]
	constexpr std::string_view type_name() {
		// Cases to handle:
		// gcc:   constexpr std::string_view ns::type_name() [with T = int; std::string_view = std::basic_string_view<char>]
		// clang: std::string_view ns::type_name() [T = int]
		#if IS_CLANG
		 return substring_bounded_by(__extension__ __PRETTY_FUNCTION__, "[T = ", "]");
		#elif IS_GCC
		 return substring_bounded_by(__extension__ __PRETTY_FUNCTION__, "[with T = ", "; std::string_view = ");
		#else
		 static_assert(false, "unsupported compiler")
		#endif
	}

	// https://stackoverflow.com/questions/28309164/checking-for-existence-of-an-overloaded-member-function
	template<typename T> class has_stream_overload {
		template<typename C, typename = decltype(std::declval<std::ostringstream>() << std::declval<C>())>
		static std::true_type test(int);
		template<typename C>
		static std::false_type test(...);
	public:
		static constexpr bool value = decltype(test<T>(0))::value;
	};

	template<typename T> constexpr bool has_stream_overload_v = has_stream_overload<T>::value;

	template<typename T> constexpr bool is_string_type =
	       isa<T, std::string>
	    || isa<T, std::string_view>
	    || isa<std::decay_t<strip<T>>, char*> // <- covers literals (i.e. const char(&)[N]) too
	    || isa<std::decay_t<strip<T>>, const char*>;

	// test cases
	static_assert(is_string_type<char*>);
	static_assert(is_string_type<const char*>);
	static_assert(is_string_type<char[5]>);
	static_assert(is_string_type<const char[5]>);
	static_assert(!is_string_type<char(*)[5]>);
	static_assert(is_string_type<char(&)[5]>);
	static_assert(is_string_type<const char (&)[27]>);
	static_assert(!is_string_type<std::vector<char>>);
	static_assert(!is_string_type<int>);
	static_assert(is_string_type<std::string>);
	static_assert(is_string_type<std::string_view>);

	[[nodiscard]] std::string stringify_int(unsigned long long, literal_format, bool, size_t);

	[[nodiscard]] std::string stringify_float(unsigned long long, literal_format, bool, size_t);

	template<typename T>
	[[gnu::cold]] [[nodiscard]]
	std::string stringify(const T& t, [[maybe_unused]] literal_format fmt = literal_format::none) {
		// bool and char need to be before std::is_integral
		if constexpr(is_string_type<T>) {
			if constexpr(std::is_pointer_v<strip<T>>) {
				if(t == nullptr) {
					return "nullptr";
				}
			}
			// TODO: re-evaluate this...? What if just comparing two buffer pointers?
			return escape_string(t, '"'); // string view may need explicit construction?
		} else if constexpr(isa<T, char>) {
			return escape_string({&t, 1}, '\'');
		} else if constexpr(isa<T, bool>) {
			return t ? "true" : "false"; // streams/boolalpha not needed for this
		} else if constexpr(isa<T, std::nullptr_t>) {
			return "nullptr";
		} else if constexpr(std::is_pointer_v<strip<T>>) {
			if(t == nullptr) { // weird nullptr shenanigans, only prints "nullptr" for nullptr_t
				return "nullptr";
			} else {
				// Manually format the pointer - ostream::operator<<(void*) falls back to %p which
				// is implementation-defined. MSVC prints pointers without the leading "0x" which
				// messes up the highlighter.
				return stringify_int(uintptr_t(t), literal_format::hex, true, sizeof(uintptr_t));
			}
		} else if constexpr(std::is_integral_v<T>) {
			return stringify_int(t, fmt, std::is_unsigned<T>::value, sizeof(t));
		} else if constexpr(std::is_floating_point_v<T>) {
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
			std::string s = std::move(oss).str();
			// std::showpoint adds a bunch of unecessary digits, so manually doing it correctly here
			if(s.find('.') == std::string::npos) s += ".0";
			return s;
		} else {
			if constexpr(has_stream_overload_v<T>) {
				std::ostringstream oss;
				oss<<t;
				return std::move(oss).str();
			} else {
				return bstringf("<instance of %s>", prettify_type(std::string(type_name<T>())).c_str());
			}
		}
	}

	/*
	 * stack trace printing
	 */

	struct column_t {
		size_t width;
		std::vector<highlight_block> blocks;
		bool right_align = false;
		column_t(size_t, std::vector<highlight_block>, bool = false);
		column_t(const column_t&);
		column_t(column_t&&);
		~column_t();
		column_t& operator=(const column_t&);
		column_t& operator=(column_t&&);
	};

	[[nodiscard]] std::string wrapped_print(const std::vector<column_t>& columns);

	[[nodiscard]] std::string print_stacktrace();

	template<typename T>
	[[gnu::cold]] [[nodiscard]]
	std::vector<std::string> generate_stringifications(const T& v, const literal_format (&formats)[4]) {
		if constexpr(std::is_arithmetic<strip<T>>::value
				 && !isa<T, bool>
				 && !isa<T, char>) {
			std::vector<std::string> vec;
			for(literal_format fmt : formats) {
				if(fmt == literal_format::none) break;
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

	[[nodiscard]] std::string print_values(const std::vector<std::string>& vec, size_t lw);

	[[nodiscard]] std::vector<highlight_block> get_values(const std::vector<std::string>& vec);

	[[nodiscard]]
	std::string print_binary_diagnostic_deferred(const literal_format (&formats)[4], std::vector<std::string>& lstrings,
	                                             std::vector<std::string>& rstrings, const char* a_str,
	                                             const char* b_str);

	void sort_and_dedup(literal_format(&)[4]);

	template<typename A, typename B>
	[[gnu::cold]] [[nodiscard]]
	std::string print_binary_diagnostic(const A& a, const B& b, const char* a_str, const char* b_str, std::string_view op) {
		// Note: op
		// figure out what information we need to print in the where clause
		// find all literal formats involved (literal_format::dec included for everything)
		auto lformat = get_literal_format(a_str);
		auto rformat = get_literal_format(b_str);
		// formerly used std::set here, now using array + sorting, `none` entries will be at the end and ignored
		literal_format formats[4] = { literal_format::dec, lformat, rformat, // ↓ always display binary for bitwise
		                              is_bitwise(op) ? literal_format::binary : literal_format::none };
		sort_and_dedup(formats); // print in specific order, avoid duplicates
		// generate raw strings for given formats, without highlighting
		std::vector<std::string> lstrings = generate_stringifications(a, formats);
		std::vector<std::string> rstrings = generate_stringifications(b, formats);
		// defer bulk of the logic to the .cpp
		return print_binary_diagnostic_deferred(formats, lstrings, rstrings, a_str, b_str);
	}

	/*
	 * actual assertion handling, finally
	 */

	struct verification_failure : std::exception {
		virtual const char* what() const noexcept final override;
	};

	struct check_failure : std::exception {
		virtual const char* what() const noexcept final override;
	};

	void default_fail_action(std::string, assert_detail::assert_type, assert_detail::ASSERTION);

	#define X(x) #x
	#define Y(x) X(x)
	constexpr const std::string_view errno_expansion = Y(errno);
	#undef Y
	#undef X

	struct extra_diagnostics {
		ASSERTION fatality = ASSERTION::FATAL;
		std::string message;
		std::vector<std::pair<std::string, std::string>> entries;
		extra_diagnostics();
		~extra_diagnostics();
		extra_diagnostics(const extra_diagnostics&) = delete;
		extra_diagnostics(extra_diagnostics&&); // = default; in the .cpp
		extra_diagnostics& operator=(const extra_diagnostics&) = delete;
		extra_diagnostics& operator=(extra_diagnostics&&) = delete;
	};

	[[nodiscard]] std::string print_extra_diagnostics(const decltype(extra_diagnostics::entries)& extra_diagnostics);

	template<typename T>
	[[gnu::cold]]
	void process_arg(extra_diagnostics& entry, size_t i, const char* const* const args_strings, T& t) {
		if constexpr(isa<T, ASSERTION>) {
			entry.fatality = t;
		} else {
			// three cases to handle: assert message, errno, and regular diagnostics
			if(isa<T, strip<decltype(errno)>> && args_strings[i] == errno_expansion) {
				// this is redundant and useless but the body for errno handling needs to be in an
				// if constexpr wrapper
				if constexpr(isa<T, strip<decltype(errno)>>) {
				// errno will expand to something hideous like (*__errno_location()),
				// may as well replace it with "errno"
				entry.entries.push_back({ "errno", bstringf("%2d \"%s\"", t, strerror_wrapper(t).c_str()) });
				}
			} else {
				if(i == 0) {
					entry.message = t;
				} else {
					entry.entries.push_back({ args_strings[i], stringify(t, literal_format::dec) });
				}
			}
		}
	}

	template<typename... Args>
	[[gnu::cold]] [[nodiscard]]
	extra_diagnostics process_args(const char* const* const args_strings, Args&... args) {
		extra_diagnostics entry;
		size_t i = 0;
		(process_arg(entry, i++, args_strings, args), ...);
		(void)args_strings;
		return entry;
	}

	struct lock {
		lock();
		~lock();
		lock(const lock&) = delete;
		lock(lock&&) = delete;
		lock& operator=(const lock&) = delete;
		lock& operator=(lock&&) = delete;
	};

	const char* assert_type_name(assert_type t);

	// collection of assertion data that can be put in static storage and all passed by a single pointer
	struct assert_static_parameters {
		const char* name;
		assert_type type;
		const char* expr_str;
		source_location location;
		const char* const* args_strings;
	};

	size_t count_args_strings(const char* const* const);

	constexpr int min_term_width = 50;

	template<typename A, typename B, typename C, typename... Args>
	[[gnu::cold]] [[gnu::noinline]]
	void assert_fail(expression_decomposer<A, B, C>& decomposer,
	                 const assert_static_parameters* params, Args&&... args) {
		lock l;
		const auto& [ name, type, expr_str, location, args_strings ] = *params;
		size_t args_strings_count = count_args_strings(args_strings);
		primitive_assert((sizeof...(args) == 0 && args_strings_count == 2)
		                 || args_strings_count == sizeof...(args) + 1);
		// process_args needs to be called as soon as possible in case errno needs to be read
		const auto [fatal, message, extra_diagnostics] = process_args(args_strings, args...);
		std::string output;
		if(message != "") {
			output += bstringf("%s failed at %s:%d: %s: %s\n",
			                   assert_type_name(type), location.file, location.line,
			                   highlight(location.function).c_str(), message.c_str());
		} else {
			output += bstringf("%s failed at %s:%d: %s:\n", assert_type_name(type),
			                   location.file, location.line, highlight(location.function).c_str());
		}
		output += bstringf("    %s\n", highlight(bstringf("%s(%s%s);", name, expr_str,
		                                         sizeof...(args) > 0 ? ", ..." : "")).c_str());
		if constexpr(is_nothing<C>) {
			static_assert(is_nothing<B> && !is_nothing<A>);
		} else {
			auto [a_str, b_str] = decompose_expression(expr_str, C::op_string);
			output += print_binary_diagnostic(decomposer.a, decomposer.b, a_str.c_str(), b_str.c_str(), C::op_string);
		}
		if(!extra_diagnostics.empty()) {
			output += print_extra_diagnostics(extra_diagnostics);
		}
		output += "\nStack trace:\n";
		output += print_stacktrace();
		#ifdef ASSERT_FAIL
		 ::ASSERT_FAIL(output, type, fatal);
		#else
		 default_fail_action(output, type, fatal);
		#endif
	}

	template<typename A, typename B, typename C, typename... Args>
	[[gnu::cold]] [[gnu::noinline]] [[nodiscard]]
	expression_decomposer<A, B, C> assert_fail_m(expression_decomposer<A, B, C> decomposer,
	                                             const assert_static_parameters* params, Args&&... args) {
		assert_fail(decomposer, params, std::forward<Args>(args)...);
		return decomposer;
	}

	// top-level assert functions emplaced by the macros
	// these are the only non-cold functions
	template<bool R, typename A, typename B, typename C, typename... Args>
	decltype(auto) assert(expression_decomposer<A, B, C> decomposer,
	                      const assert_static_parameters* params, Args&&... args) {
		decltype(auto) value = decomposer.get_value();
		constexpr bool ret_lhs = decomposer.ret_lhs();
		if(strong_expect(!static_cast<bool>(value), 0)) {
			#ifdef NDEBUG
			 if(params->type == assert_type::assertion) { // will be constant propagated
				__builtin_unreachable();
			 }
			 // If an assert fails under -DNDEBUG this whole branch will be marked unreachable but
			 // without optimizations control flow can fallthrough the above statement. It's of
			 // course all UB once an assertion fails in -DNDEBUG but this is an attempt to produce
			 // better behavior.
			 if(params->type == assert_type::assertion) {
				fprintf(stderr, "Catastropic error: Assertion failed in -DNDEBUG\n");
				abort();
			 }
			 // verify calls will procede with fail call
			 // check is excluded at macro definition, nothing needed here
			#endif
			// TODO: decide threshold, maybe check trivially movable too?
			if constexpr(sizeof decomposer > 32) {
				assert_fail(decomposer, params, std::forward<Args>(args)...);
			} else {
				// std::move it to assert_fail_m, will be moved back to r
				auto r = assert_fail_m(std::move(decomposer), params, std::forward<Args>(args)...);
				// can't move-assign back to decomposer if it holds reference members
				decomposer.compl expression_decomposer();
				new (&decomposer) expression_decomposer(std::move(r));
			}
		#ifdef _0_ASSERT_DEMO
		} else {
			primitive_assert(false);
		#endif
		}
		if constexpr(R) { // return lhs or value for assert and verify
			if constexpr(ret_lhs) {
				// Note: std::launder needed in 17 in case of placement new / move shenanigans above
				// https://timsong-cpp.github.io/cppwp/n4659/basic.life#8.3
				return std::launder(&decomposer)->take_lhs();
			} else {
				if constexpr(std::is_lvalue_reference<decltype(value)>::value) {
					return (value);
				} else {
					return value;
				}
			}
		}
	}
}

using assert_detail::ASSERTION;

// Macro mapping utility by William Swanson https://github.com/swansontec/map-macro/blob/master/map.h
#define EVAL0(...) __VA_ARGS__
#define EVAL1(...) EVAL0(EVAL0(EVAL0(__VA_ARGS__)))
#define EVAL2(...) EVAL1(EVAL1(EVAL1(__VA_ARGS__)))
#define EVAL3(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))
#define EVAL4(...) EVAL3(EVAL3(EVAL3(__VA_ARGS__)))
#define EVAL(...)  EVAL4(EVAL4(EVAL4(__VA_ARGS__)))
#define MAP_END(...)
#define MAP_OUT
#define MAP_COMMA ,
#define MAP_GET_END2() 0, MAP_END
#define MAP_GET_END1(...) MAP_GET_END2
#define MAP_GET_END(...) MAP_GET_END1
#define MAP_NEXT0(test, next, ...) next MAP_OUT
#define MAP_NEXT1(test, next) MAP_NEXT0(test, next, 0)
#define MAP_NEXT(test, next)  MAP_NEXT1(MAP_GET_END test, next)
#define MAP0(f, x, peek, ...) f(x) MAP_NEXT(peek, MAP1)(f, peek, __VA_ARGS__)
#define MAP1(f, x, peek, ...) f(x) MAP_NEXT(peek, MAP0)(f, peek, __VA_ARGS__)
#define MAP_LIST_NEXT1(test, next) MAP_NEXT0(test, MAP_COMMA next, 0)
#define MAP_LIST_NEXT(test, next)  MAP_LIST_NEXT1(MAP_GET_END test, next)
#define MAP_LIST0(f, x, peek, ...) f(x) MAP_LIST_NEXT(peek, MAP_LIST1)(f, peek, __VA_ARGS__)
#define MAP_LIST1(f, x, peek, ...) f(x) MAP_LIST_NEXT(peek, MAP_LIST0)(f, peek, __VA_ARGS__)
#define MAP(f, ...) EVAL(MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))

#define ASSERT_DETAIL_STRINGIFY(x) #x,

// __PRETTY_FUNCTION__ used because __builtin_FUNCTION() used in source_location (like __FUNCTION__) is just the method
// name, not signature
#define ASSERT_DETAIL_STATIC_DATA(name, type, expr_str, ...) \
                                  __extension__ ({ \
                                    /* extra string here because of extra comma from map and also serves as terminator */ \
                                    static constexpr const char* const arg_strings[] = { \
                                      MAP(ASSERT_DETAIL_STRINGIFY, __VA_ARGS__) "" \
                                    }; \
                                    static constexpr assert_detail::assert_static_parameters params = { \
                                      name, \
                                      type, \
                                      expr_str, \
                                      __extension__ __PRETTY_FUNCTION__, \
                                      arg_strings, \
                                    }; \
                                    &params; \
                                  })

#if IS_GCC
 // __VA_OPT__ needed for GCC, https://gcc.gnu.org/bugzilla/show_bug.cgi?id=44317
 #define ASSERT_DETAIL_VA_ARGS(...) __VA_OPT__(,) __VA_ARGS__
#else
 // clang properly eats the comma with ##__VA_ARGS__
 #define ASSERT_DETAIL_VA_ARGS(...) , ##__VA_ARGS__
#endif

// Note about statement expressions: These are needed for two reasons. The first is putting the arg string array and
// source location structure in .rodata rather than on the stack, the second is a _Pragma for warnings which isn't
// allowed in the middle of an expression by GCC. The semantics are similar to a function return:
// Given M m; in parent scope, ({ m; }) is an rvalue M&& rather than an lvalue
// ({ M m; m; }) doesn't move, it copies
// ({ M{}; }) does move
// Of relevance to this: in foo(__extension__ ({ M{1} + M{1}; })); the lifetimes of the M{1} objects end during the
// statement expression but the lifetime of the returned object is extend to the end of the full foo() expression.
// Note: There is a current issue with tarnaries: auto x = assert(b ? y : y); must copy y. This can be fixed with
// lambdas but that's potentially very expensive compile-time wise. Need to investigate further.
// Note: assert_detail::expression_decomposer(assert_detail::expression_decomposer{} << expr) done for ternary support
#define ASSERT_INVOKE(expr, name, ...) \
                          assert_detail::assert<true>( \
                            __extension__ ({ \
                              _Pragma("GCC diagnostic ignored \"-Wparentheses\"") \
                              assert_detail::expression_decomposer(assert_detail::expression_decomposer{} << expr); \
                            }), \
                            ASSERT_DETAIL_STATIC_DATA(name, assert_detail::assert_type::assertion, #expr, __VA_ARGS__) \
                            ASSERT_DETAIL_VA_ARGS(__VA_ARGS__) \
                          )

#define ASSERT(expr, ...) ASSERT_INVOKE(expr, "ASSERT", __VA_ARGS__)

#ifndef ASSERT_NO_LOWERCASE
 // provided for <assert.h> compatability
 #define assert(expr, ...) ASSERT_INVOKE(expr, "assert", __VA_ARGS__)
#endif

#define VERIFY(expr, ...) assert_detail::assert<true>( \
                            __extension__ ({ \
                              _Pragma("GCC diagnostic ignored \"-Wparentheses\"") \
                              assert_detail::expression_decomposer(assert_detail::expression_decomposer{} << expr); \
                            }), \
                            ASSERT_DETAIL_STATIC_DATA("VERIFY", assert_detail::assert_type::verify, \
                                                      #expr, __VA_ARGS__) \
                            ASSERT_DETAIL_VA_ARGS(__VA_ARGS__) \
                          )

#ifndef NDEBUG
 #define CHECK(expr, ...) assert_detail::assert<false>( \
                            __extension__ ({ \
                              _Pragma("GCC diagnostic ignored \"-Wparentheses\"") \
                              assert_detail::expression_decomposer(assert_detail::expression_decomposer{} << expr); \
                            }), \
                            ASSERT_DETAIL_STATIC_DATA("CHECK", assert_detail::assert_type::check, #expr, __VA_ARGS__) \
                            ASSERT_DETAIL_VA_ARGS(__VA_ARGS__) \
                          )
#else
 // omitting the expression could cause unused variable warnings, surpressing for now
 // TODO: Is this the right design decision? Re-evaluated whether PHONY_USE should be used here or internally at all
 #define CHECK(expr, ...) PHONY_USE(expr)
#endif

#ifndef _0_ASSERT_CPP // keep macros for the .cpp
 #undef IS_CLANG
 #undef IS_GCC
 #undef IS_WINDOWS
 #undef IS_LINUX
 #undef STDIN_FILENO
 #undef STDOUT_FILENO
 #undef STDERR_FILENO
 #undef strong_expect
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
 #undef primitive_assert
 #undef internal_verify
#endif

#endif
