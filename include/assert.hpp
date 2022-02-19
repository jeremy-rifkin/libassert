#ifndef ASSERT_HPP
#define ASSERT_HPP

// Jeremy Rifkin 2021
// https://github.com/jeremy-rifkin/asserts

#include <bitset>
#include <functional>
#include <iomanip>
#include <limits>
#include <optional>
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

#ifdef ASSERT_FAIL
 extern void ASSERT_FAIL();
#endif
#ifdef VERIFY_FAIL
 extern void VERIFY_FAIL();
#endif
#ifdef CHECK_FAIL
 extern void CHECK_FAIL();
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

	template<typename... T>
	[[gnu::cold]]
	std::string stringf(T... args) {
		int length = snprintf(0, 0, args...);
		if(length < 0) primitive_assert(false, "Invalid arguments to stringf");
		std::string str(length, 0);
		snprintf(str.data(), length + 1, args...);
		return str;
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

	std::string indent(const std::string_view str, size_t depth, char c = ' ', bool ignore_first = false);

	/*
	 * system wrappers
	 */

	void enable_virtual_terminal_processing_if_needed();

	#ifdef _WIN32
	typedef int pid_t;
	#endif

	pid_t getpid();

	void wait_for_keypress();

	bool isatty(int fd);

	// https://stackoverflow.com/questions/23369503/get-size-of-terminal-window-rows-columns
	int terminal_width();

	std::string strerror_wrapper(int err); // stupid C stuff, stupid microsoft stuff

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
	[[gnu::cold]]
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
	[[gnu::cold]]
	constexpr bool cmp_not_equal(T t, U u) {
		return !cmp_equal(t, u);
	}

	template<typename T, typename U>
	[[gnu::cold]]
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
	[[gnu::cold]]
	constexpr bool cmp_greater(T t, U u) {
		return cmp_less(u, t);
	}

	template<typename T, typename U>
	[[gnu::cold]]
	constexpr bool cmp_less_equal(T t, U u) {
		return !cmp_greater(t, u);
	}

	template<typename T, typename U>
	[[gnu::cold]]
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
			[[gnu::cold]] constexpr auto operator()(A&& lhs, B&& rhs) const { /* no need to forward ints */ \
				return std::forward<A>(lhs) op std::forward<B>(rhs); \
			} \
		}
		#define gen_op_boilerplate_special(name, op, cmp) struct name { \
			static constexpr std::string_view op_string = #op; \
			template<typename A, typename B> \
			[[gnu::cold]] constexpr auto operator()(A&& lhs, B&& rhs) const { /* no need to forward ints */ \
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
		decltype(auto) get_value() {
			if constexpr(is_nothing<C>) {
				static_assert(is_nothing<B> && !is_nothing<A>);
				return (((((((((((((((((((a)))))))))))))))))));
			} else {
				return C()(a, b);
			}
		}
		operator bool() { // for ternary support
			return (bool)get_value();
		}
		// return true if the lhs should be returned, false if full computed value should be
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
		template<typename O> auto operator<<(O&& operand) {
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
		#define gen_op_boilerplate(functor, op) template<typename O> auto operator op(O&& operand) { \
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

	std::string prettify_type(std::string type);

	std::string highlight(const std::string& expression);

	std::vector<highlight_block> highlight_blocks(const std::string& expression);

	literal_format get_literal_format(const std::string& expression);

	std::string trim_suffix(const std::string& expression);

	bool is_bitwise(std::string_view op);

	std::pair<std::string, std::string> decompose_expression(const std::string& expression, const std::string_view target_op);

	/*
	 * stringification
	 */

	std::string escape_string(const std::string_view str, char quote);

	template<typename T>
	[[gnu::cold]]
	constexpr std::string_view type_name() {
		// Cases to handle:
		// gcc:   constexpr std::string_view ns::type_name() [with T = int; std::string_view = std::basic_string_view<char>]
		// clang: std::string_view ns::type_name() [T = int]
		auto substring_bounded_by = [](std::string_view sig, std::string_view l, std::string_view r) {
			primitive_assert(sig.find(l) != std::string_view::npos);
			primitive_assert(sig.rfind(r) != std::string_view::npos);
			primitive_assert(sig.find(l) < sig.rfind(r));
			auto i = sig.find(l) + l.length();
			return sig.substr(i, sig.rfind(r) - i);
		};
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

	template<typename T>
	[[gnu::cold]]
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
				std::ostringstream oss;
				// Manually format the pointer - ostream::operator<<(void*) falls back to %p which
				// is implementation-defined. MSVC prints pointers without the leading "0x" which
				// messes up the highlighter.
				oss<<std::showbase<<std::hex<<uintptr_t(t);
				return std::move(oss).str();
			}
		} else if constexpr(std::is_integral_v<T>) {
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
				return stringf("<instance of %s>", prettify_type(std::string(type_name<T>())).c_str());
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

	void wrapped_print(const std::vector<column_t>& columns);

	void print_stacktrace();

	template<typename T>
	[[gnu::cold]]
	std::vector<std::string> generate_stringifications(const T& v, const std::set<literal_format>& formats) {
		if constexpr(std::is_arithmetic<strip<T>>::value
				 && !isa<T, bool>
				 && !isa<T, char>) {
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

	void print_values(const std::vector<std::string>& vec, size_t lw);

	std::vector<highlight_block> get_values(const std::vector<std::string>& vec);

	constexpr int min_term_width = 50;

	template<typename A, typename B>
	[[gnu::cold]]
	void print_binary_diagnostic(const A& a, const B& b, const char* a_str, const char* b_str, std::string_view op) {
		// Note: op
		// figure out what information we need to print in the where clause
		// find all literal formats involved (literal_format::dec included for everything)
		auto lformat = get_literal_format(a_str);
		auto rformat = get_literal_format(b_str);
		// std::set used so formats are printed in a specific order
		std::set<literal_format> formats = { literal_format::dec, lformat, rformat };
		formats.erase(literal_format::none); // none is just for when the expression isn't a literal
		if(is_bitwise(op)) formats.insert(literal_format::binary); // always display binary for bitwise
		primitive_assert(formats.size() > 0);
		// generate raw strings for given formats, without highlighting
		std::vector<std::string> lstrings = generate_stringifications(a, formats);
		std::vector<std::string> rstrings = generate_stringifications(b, formats);
		primitive_assert(lstrings.size() > 0);
		primitive_assert(rstrings.size() > 0);
		// pad all columns where there is overlap
		// TODO: Use column printer instead of manual padding.
		for(size_t i = 0; i < std::min(lstrings.size(), rstrings.size()); i++) {
			// find which clause, left or right, we're padding (entry i)
			std::vector<std::string>& which = lstrings[i].length() < rstrings[i].length() ? lstrings : rstrings;
			int difference = std::abs((int)lstrings[i].length() - (int)rstrings[i].length());
			if(i != which.size() - 1) { // last column excluded as padding is not necessary at the end of the line
				which[i].insert(which[i].end(), difference, ' ');
			}
		}
		// determine whether we actually gain anything from printing a where clause (e.g. exclude "1 => 1")
		struct { bool left, right; } has_useful_where_clause = {
			formats.size() > 1 || lstrings.size() > 1 || (a_str != lstrings[0] && trim_suffix(a_str) != lstrings[0]),
			formats.size() > 1 || rstrings.size() > 1 || (b_str != rstrings[0] && trim_suffix(b_str) != rstrings[0])
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
			auto print_clause = [term_width, lw](const char* expr_str, std::vector<std::string>& expr_strs) {
				if(term_width >= min_term_width) {
					wrapped_print({
						{ 7, {{"", ""}} }, // 8 space indent, wrapper will add a space
						{ lw, highlight_blocks(expr_str) },
						{ 2, {{"", "=>"}} },
						{ term_width - lw - 8 /* indent */ - 4 /* arrow */, get_values(expr_strs) }
					});
				} else {
					fprintf(stderr, "        %s%*s => ",
							highlight(expr_str).c_str(), int(lw - strlen(expr_str)), "");
						print_values(expr_strs, lw);
				}
			};
			if(has_useful_where_clause.left) {
				print_clause(a_str, lstrings);
			}
			if(has_useful_where_clause.right) {
				print_clause(b_str, rstrings);
			}
		}
	}

	/*
	 * actual assertion main bodies, finally
	 */

	// allow non-fatal assertions
	enum class ASSERT {
		NONFATAL, FATAL
	};

	struct verification_failure : std::exception {
		virtual const char* what() const noexcept final override;
	};

	struct check_failure : std::exception {
		virtual const char* what() const noexcept final override;
	};

	#define X(x) #x
	#define Y(x) X(x)
	constexpr const std::string_view errno_expansion = Y(errno);
	#undef Y
	#undef X

	struct extra_diagnostics {
		ASSERT fatality = ASSERT::FATAL;
		std::string message;
		std::vector<std::pair<std::string, std::string>> entries;
		extra_diagnostics();
		~extra_diagnostics();
		extra_diagnostics(const extra_diagnostics&) = delete;
		extra_diagnostics(extra_diagnostics&&); // = default; in the .cpp
		extra_diagnostics& operator=(const extra_diagnostics&) = delete;
		extra_diagnostics& operator=(extra_diagnostics&&) = delete;
	};

	[[gnu::cold]] [[maybe_unused]]
	static void process_args_step(extra_diagnostics&, size_t, const char* const* const) { }

	template<typename T>
	[[gnu::cold]]
	void process_arg(extra_diagnostics& entry, size_t i, const char* const* const args_strings, T& t) {
		if constexpr(isa<T, ASSERT>) {
			entry.fatality = t;
		} else {
			// three cases to handle: assert message, errno, and regular diagnostics
			if(isa<T, strip<decltype(errno)>> && args_strings[i] == errno_expansion) {
				// this is redundant and useless but the body for errno handling needs to be in an
				// if constexpr wrapper
				if constexpr(isa<T, strip<decltype(errno)>>) {
				// errno will expand to something hideous like (*__errno_location()),
				// may as well replace it with "errno"
				entry.entries.push_back({ "errno", stringf("%2d \"%s\"", t, strerror_wrapper(t).c_str()) });
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
	[[gnu::cold]]
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

	enum class assert_type {
		assert,
		verify,
		check
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

	template<typename A, typename B, typename C, typename... Args>
	[[gnu::cold]] [[gnu::noinline]]
	void assert_fail(expression_decomposer<A, B, C>& decomposer,
	                 const assert_static_parameters* params, Args&&... args) {
		lock l;
		const auto [ name, type, expr_str, location, args_strings ] = *params;
		size_t args_strings_count = count_args_strings(args_strings);
		primitive_assert((sizeof...(args) == 0 && args_strings_count == 2)
		                 || args_strings_count == sizeof...(args) + 1);
		// process_args needs to be called as soon as possible in case errno needs to be read
		auto [fatal, message, extra_diagnostics] = process_args(args_strings, args...);
		enable_virtual_terminal_processing_if_needed();
		if(message != "") {
			fprintf(stderr, "%s failed at %s:%d: %s: %s\n",
			                 assert_type_name(type), location.file, location.line,
			                 highlight(location.function).c_str(), message.c_str());
		} else {
			fprintf(stderr, "%s failed at %s:%d: %s:\n", assert_type_name(type),
			                          location.file, location.line, highlight(location.function).c_str());
		}
		fprintf(stderr, "    %s\n", highlight(stringf("%s(%s%s);", name, expr_str,
		                                        sizeof...(args) > 0 ? ", ..." : "")).c_str());
		if constexpr(is_nothing<C>) {
			static_assert(is_nothing<B> && !is_nothing<A>);
		} else {
			auto [a_str, b_str] = decompose_expression(expr_str, C::op_string);
			print_binary_diagnostic(decomposer.a, decomposer.b, a_str.c_str(), b_str.c_str(), C::op_string);
		}
		if(!extra_diagnostics.empty()) {
			fprintf(stderr, "    Extra diagnostics:\n");
			size_t term_width = terminal_width(); // will be 0 on error
			size_t lw = 0;
			for(auto& entry : extra_diagnostics) {
				lw = std::max(lw, entry.first.size());
			}
			for(auto& entry : extra_diagnostics) {
				if(term_width >= min_term_width) {
					wrapped_print({
						{ 7, {{"", ""}} }, // 8 space indent, wrapper will add a space
						{ lw, highlight_blocks(entry.first) },
						{ 2, {{"", "=>"}} },
						{ term_width - lw - 8 /* indent */ - 4 /* arrow */, highlight_blocks(entry.second) }
					});
				} else {
					fprintf(stderr, "        %s%*s => %s\n",
							highlight(entry.first).c_str(), int(lw - entry.first.length()), "",
							indent(highlight(entry.second), 8 + lw + 4, ' ', true).c_str());
				}
			}
		}
		fprintf(stderr, "\nStack trace:\n");
		print_stacktrace();
		if(fatal == ASSERT::FATAL) {
			#ifndef _0_ASSERT_DEMO
			 switch(type) {
			 	case assert_type::assert:
			 		#ifdef ASSERT_FAIL
			 		 ::ASSERT_FAIL();
			 		#else
			 		 fflush(stdout);
			 		 fflush(stderr);
			 		 abort();
			 		#endif
			 		break;
			 	case assert_type::verify:
			 		#ifdef VERIFY_FAIL
			 		 ::VERIFY_FAIL();
			 		#else
			 		 throw verification_failure();
			 		#endif
			 		break;
			 	case assert_type::check:
			 		#ifdef CHECK_FAIL
			 		 ::CHECK_FAIL();
			 		#else
			 		 throw check_failure();
			 		#endif
			 		break;
				default:
					primitive_assert(false);
					__builtin_unreachable();
			 }
			#endif
		}
		#ifdef _0_ASSERT_DEMO
		 fprintf(stderr, "\n");
		#endif
	}

	template<typename A, typename B, typename C, typename... Args>
	[[gnu::cold]] [[gnu::noinline]]
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
		if(__builtin_expect_with_probability(!static_cast<bool>(value), 0, 1)) {
			#ifdef NDEBUG
			 if(params->type == assert_type::assert) { // will be constant propagated
				__builtin_unreachable();
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
				return (value);
			}
		}
	}

	#ifndef _0_ASSERT_CPP // keep macros for the .cpp
	 #undef primitive_assert
	 #undef internal_verify
	#endif
}

#ifndef _0_ASSERT_CPP // keep macros for the .cpp
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
#endif

using assert_detail::ASSERT;

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

// Note: assert_detail::expression_decomposer(assert_detail::expression_decomposer{} << expr) done for ternary support
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
#define ASSERT_INVOKE(expr, name, ...) \
                          assert_detail::assert<true>( \
                            __extension__ ({ \
                              _Pragma("GCC diagnostic ignored \"-Wparentheses\"") \
                              assert_detail::expression_decomposer(assert_detail::expression_decomposer{} << expr); \
                            }), \
                            ASSERT_DETAIL_STATIC_DATA(name, assert_detail::assert_type::assert, #expr, __VA_ARGS__) \
                            ASSERT_DETAIL_VA_ARGS(__VA_ARGS__) \
                          )

#define ASSERT(expr, ...) ASSERT_INVOKE(expr, "ASSERT", __VA_ARGS__)

#ifndef ASSERT_NO_LOWERCASE
 // provided for <assert.h> compatability
 #define assert(expr, ...) ASSERT_INVOKE(expr, "assert", __VA_ARGS__)
#endif

// assert_detail::expression_decomposer(assert_detail::expression_decomposer{} << expr) done for ternary support
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
 // assert_detail::expression_decomposer(assert_detail::expression_decomposer{} << expr) done for ternary support
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

#endif
