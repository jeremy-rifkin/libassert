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
 #define ASSERT_DETAIL_IS_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
 #define ASSERT_DETAIL_IS_GCC 1
#elif defined(_MSC_VER)
 #define ASSERT_DETAIL_IS_MSVC 1
 #include <iso646.h> // alternative operator tokens are standard but msvc requires the include or /permissive- or /Za
#else
 #error "Unsupported compiler"
#endif

#if ASSERT_DETAIL_IS_CLANG || ASSERT_DETAIL_IS_GCC
 #define ASSERT_DETAIL_PFUNC __extension__ __PRETTY_FUNCTION__
 #define ASSERT_DETAIL_ATTR_COLD     [[gnu::cold]]
 #define ASSERT_DETAIL_ATTR_NOINLINE [[gnu::noinline]]
 #define ASSERT_DETAIL_UNREACHABLE __builtin_unreachable()
#else
 #define ASSERT_DETAIL_PFUNC __FUNCSIG__
 #define ASSERT_DETAIL_ATTR_COLD
 #define ASSERT_DETAIL_ATTR_NOINLINE __declspec(noinline)
 #define ASSERT_DETAIL_UNREACHABLE __assume(false)
#endif

#if ASSERT_DETAIL_IS_MSVC
 #define assert_detail_strong_expect(expr, value) (expr)
#elif defined(__clang__) && __clang_major__ >= 11 || __GNUC__ >= 9
 #define assert_detail_strong_expect(expr, value) __builtin_expect_with_probability((expr), (value), 1)
#else
 #define assert_detail_strong_expect(expr, value) __builtin_expect((expr), (value))
#endif

// if defined by a previous #include...
#ifdef assert
 #undef assert
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

	class assertion_printer;
}

#ifndef ASSERT_FAIL
 #define ASSERT_FAIL assert_detail_default_fail_action
#endif

void ASSERT_FAIL(assert_detail::assertion_printer& printer, assert_detail::assert_type type,
                 assert_detail::ASSERTION fatal);

#define ASSERT_DETAIL_PHONY_USE(E) do { using x [[maybe_unused]] = decltype(E); } while(0)

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
	 #define assert_detail_primitive_assert(c, ...) primitive_assert_impl(c, false, #c, \
	                                                                      ASSERT_DETAIL_PFUNC, ##__VA_ARGS__)
	#else
	 #define assert_detail_primitive_assert(c, ...) ASSERT_DETAIL_PHONY_USE(c)
	#endif

	/*
	 * string utilities
	 */

	[[nodiscard]] std::string bstringf(const char* format, ...);

	[[nodiscard]] std::string strip_colors(const std::string& str);

	/*
	 * system wrappers
	 */

	[[nodiscard]] std::string strerror_wrapper(int err); // stupid C stuff, stupid microsoft stuff

	// will be 0 on error
	[[nodiscard]] int terminal_width(int fd);

	/*
	 * Stacktrace implementation
	 */

	// All in the .cpp

	void* get_stacktrace_opaque();

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
	[[nodiscard]] constexpr bool cmp_equal(T t, U u) {
		using UT = std::make_unsigned_t<T>;
		using UU = std::make_unsigned_t<U>;
		if constexpr(std::is_signed_v<T> == std::is_signed_v<U>)
			return t == u;
		else if constexpr(std::is_signed_v<T>)
			return t >= 0 && UT(t) == u;
		else
			return u >= 0 && t == UU(u);
	}

	template<typename T, typename U>
	[[nodiscard]] constexpr bool cmp_not_equal(T t, U u) {
		return !cmp_equal(t, u);
	}

	template<typename T, typename U>
	[[nodiscard]] constexpr bool cmp_less(T t, U u) {
		using UT = std::make_unsigned_t<T>;
		using UU = std::make_unsigned_t<U>;
		if constexpr(std::is_signed_v<T> == std::is_signed_v<U>)
			return t < u;
		else if constexpr(std::is_signed_v<T>)
			return t < 0  || UT(t) < u;
		else
			return u >= 0 && t < UU(u);
	}

	template<typename T, typename U>
	[[nodiscard]] constexpr bool cmp_greater(T t, U u) {
		return cmp_less(u, t);
	}

	template<typename T, typename U>
	[[nodiscard]] constexpr bool cmp_less_equal(T t, U u) {
		return !cmp_less(u, t);
	}

	template<typename T, typename U>
	[[nodiscard]] constexpr bool cmp_greater_equal(T t, U u) {
		return !cmp_less(t, u);
	}

	// Lots of boilerplate
	// std:: implementations don't allow two separate types for lhs/rhs
	// Note: is this macro potentially bad when it comes to debugging(?)
	namespace ops {
		#define assert_detail_gen_op_boilerplate(name, op) struct name { \
			static constexpr std::string_view op_string = #op; \
			template<typename A, typename B> \
			ASSERT_DETAIL_ATTR_COLD [[nodiscard]] \
			constexpr auto operator()(A&& lhs, B&& rhs) const { /* no need to forward ints */ \
				return std::forward<A>(lhs) op std::forward<B>(rhs); \
			} \
		}
		#define assert_detail_gen_op_boilerplate_special(name, op, cmp) struct name { \
			static constexpr std::string_view op_string = #op; \
			template<typename A, typename B> \
			ASSERT_DETAIL_ATTR_COLD [[nodiscard]] \
			constexpr auto operator()(A&& lhs, B&& rhs) const { /* no need to forward ints */ \
				if constexpr(is_integral_and_not_bool<A> && is_integral_and_not_bool<B>) return cmp(lhs, rhs); \
				else return std::forward<A>(lhs) op std::forward<B>(rhs); \
			} \
		}
		assert_detail_gen_op_boilerplate(shl, <<);
		assert_detail_gen_op_boilerplate(shr, >>);
		assert_detail_gen_op_boilerplate_special(eq,   ==, cmp_equal);
		assert_detail_gen_op_boilerplate_special(neq,  !=, cmp_not_equal);
		assert_detail_gen_op_boilerplate_special(gt,    >, cmp_greater);
		assert_detail_gen_op_boilerplate_special(lt,    <, cmp_less);
		assert_detail_gen_op_boilerplate_special(gteq, >=, cmp_greater_equal);
		assert_detail_gen_op_boilerplate_special(lteq, <=, cmp_less_equal);
		assert_detail_gen_op_boilerplate(band,   &);
		assert_detail_gen_op_boilerplate(bxor,   ^);
		assert_detail_gen_op_boilerplate(bor,    |);
		assert_detail_gen_op_boilerplate(land,   &&);
		assert_detail_gen_op_boilerplate(lor,    ||);
		assert_detail_gen_op_boilerplate(assign, =);
		assert_detail_gen_op_boilerplate(add_assign,  +=);
		assert_detail_gen_op_boilerplate(sub_assign,  -=);
		assert_detail_gen_op_boilerplate(mul_assign,  *=);
		assert_detail_gen_op_boilerplate(div_assign,  /=);
		assert_detail_gen_op_boilerplate(mod_assign,  %=);
		assert_detail_gen_op_boilerplate(shl_assign,  <<=);
		assert_detail_gen_op_boilerplate(shr_assign,  >>=);
		assert_detail_gen_op_boilerplate(band_assign, &=);
		assert_detail_gen_op_boilerplate(bxor_assign, ^=);
		assert_detail_gen_op_boilerplate(bor_assign,  |=);
		#undef assert_detail_gen_op_boilerplate
		#undef assert_detail_gen_op_boilerplate_special
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
		#define assert_detail_gen_op_boilerplate(functor, op) \
		template<typename O> [[nodiscard]] auto operator op(O&& operand) && { \
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
		assert_detail_gen_op_boilerplate(ops::shr, >>)
		assert_detail_gen_op_boilerplate(ops::eq, ==)
		assert_detail_gen_op_boilerplate(ops::neq, !=)
		assert_detail_gen_op_boilerplate(ops::gt, >)
		assert_detail_gen_op_boilerplate(ops::lt, <)
		assert_detail_gen_op_boilerplate(ops::gteq, >=)
		assert_detail_gen_op_boilerplate(ops::lteq, <=)
		assert_detail_gen_op_boilerplate(ops::band, &)
		assert_detail_gen_op_boilerplate(ops::bxor, ^)
		assert_detail_gen_op_boilerplate(ops::bor, |)
		assert_detail_gen_op_boilerplate(ops::land, &&)
		assert_detail_gen_op_boilerplate(ops::lor, ||)
		assert_detail_gen_op_boilerplate(ops::assign, =)
		assert_detail_gen_op_boilerplate(ops::add_assign, +=)
		assert_detail_gen_op_boilerplate(ops::sub_assign, -=)
		assert_detail_gen_op_boilerplate(ops::mul_assign, *=)
		assert_detail_gen_op_boilerplate(ops::div_assign, /=)
		assert_detail_gen_op_boilerplate(ops::mod_assign, %=)
		assert_detail_gen_op_boilerplate(ops::shl_assign, <<=)
		assert_detail_gen_op_boilerplate(ops::shr_assign, >>=)
		assert_detail_gen_op_boilerplate(ops::band_assign, &=)
		assert_detail_gen_op_boilerplate(ops::bxor_assign, ^=)
		assert_detail_gen_op_boilerplate(ops::bor_assign, |=)
		#undef assert_detail_gen_op_boilerplate
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
	ASSERT_DETAIL_ATTR_COLD [[nodiscard]]
	constexpr std::string_view type_name() {
		// Cases to handle:
		// gcc:   constexpr std::string_view ns::type_name() [with T = int; std::string_view = std::basic_string_view<char>]
		// clang: std::string_view ns::type_name() [T = int]
		// msvc:  class std::basic_string_view<char,struct std::char_traits<char> > __cdecl ns::type_name<int>(void)
		#if ASSERT_DETAIL_IS_CLANG
		 return substring_bounded_by(ASSERT_DETAIL_PFUNC, "[T = ", "]");
		#elif ASSERT_DETAIL_IS_GCC
		 return substring_bounded_by(ASSERT_DETAIL_PFUNC, "[with T = ", "; std::string_view = ");
		#elif ASSERT_DETAIL_IS_MSVC
		 return substring_bounded_by(ASSERT_DETAIL_PFUNC, "type_name<", ">(void)");
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
	ASSERT_DETAIL_ATTR_COLD [[nodiscard]]
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
					assert_detail_primitive_assert(false, "unexpected literal format requested for printing");
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

	[[nodiscard]] std::string print_stacktrace(void* raw_trace, int term_width);

	template<typename T>
	ASSERT_DETAIL_ATTR_COLD [[nodiscard]]
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

	struct binary_diagnostics_descriptor {
		std::vector<std::string> lstrings;
		std::vector<std::string> rstrings;
	    std::string a_str;
		std::string b_str;
		bool multiple_formats;
		bool present = false;
		binary_diagnostics_descriptor(); // = default; in the .cpp
		binary_diagnostics_descriptor(std::vector<std::string>& lstrings, std::vector<std::string>& rstrings,
		                              std::string a_str, std::string b_str, bool multiple_formats);
		~binary_diagnostics_descriptor(); // = default; in the .cpp
		binary_diagnostics_descriptor(const binary_diagnostics_descriptor&) = delete;
		binary_diagnostics_descriptor(binary_diagnostics_descriptor&&); // = default; in the .cpp
		binary_diagnostics_descriptor& operator=(const binary_diagnostics_descriptor&) = delete;
		binary_diagnostics_descriptor& operator=(binary_diagnostics_descriptor&&); // = default; in the .cpp
	};

	[[nodiscard]]
	std::string print_binary_diagnostic_deferred(size_t term_width, binary_diagnostics_descriptor& diagnostics);

	void sort_and_dedup(literal_format(&)[4]);

	template<typename A, typename B>
	ASSERT_DETAIL_ATTR_COLD [[nodiscard]]
	binary_diagnostics_descriptor print_binary_diagnostic(const A& a, const B& b, const char* a_str, const char* b_str,
	                                                      std::string_view op) {
		using lf = literal_format;
		// Note: op
		// figure out what information we need to print in the where clause
		// find all literal formats involved (literal_format::dec included for everything)
		auto lformat = get_literal_format(a_str);
		auto rformat = get_literal_format(b_str);
		// formerly used std::set here, now using array + sorting, `none` entries will be at the end and ignored
		lf formats[4] = { lf::dec, lformat, rformat, // ↓ always display binary for bitwise
		                  is_bitwise(op) ? lf::binary : lf::none };
		sort_and_dedup(formats); // print in specific order, avoid duplicates
		// generate raw strings for given formats, without highlighting
		std::vector<std::string> lstrings = generate_stringifications(a, formats);
		std::vector<std::string> rstrings = generate_stringifications(b, formats);
		// defer bulk of the logic to print_binary_diagnostic_deferred
		return binary_diagnostics_descriptor { lstrings, rstrings, a_str, b_str, formats[1] != lf::none };
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

	#define ASSERT_DETAIL_X(x) #x
	#define ASSERT_DETAIL_Y(x) ASSERT_DETAIL_X(x)
	constexpr const std::string_view errno_expansion = ASSERT_DETAIL_Y(errno);
	#undef ASSERT_DETAIL_Y
	#undef ASSERT_DETAIL_X

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

	[[nodiscard]] std::string print_extra_diagnostics(size_t term_width,
	                                                  const decltype(extra_diagnostics::entries)& extra_diagnostics);

	template<typename T>
	ASSERT_DETAIL_ATTR_COLD
	void process_arg(extra_diagnostics& entry, size_t i, const char* const* const args_strings, T& t) {
		if constexpr(isa<T, ASSERTION>) {
			entry.fatality = t;
		} else {
			// three cases to handle: assert message, errno, and regular diagnostics
			#if ASSERT_DETAIL_IS_MSVC
			 #pragma warning(push)
			 #pragma warning(disable: 4127) // MSVC thinks constexpr should be used here. It should not.
			#endif
			if(isa<T, strip<decltype(errno)>> && args_strings[i] == errno_expansion) {
			#if ASSERT_DETAIL_IS_MSVC
			 #pragma warning(pop)
			#endif
				// this is redundant and useless but the body for errno handling needs to be in an
				// if constexpr wrapper
				if constexpr(isa<T, strip<decltype(errno)>>) {
				// errno will expand to something hideous like (*__errno_location()),
				// may as well replace it with "errno"
				entry.entries.push_back({ "errno", bstringf("%2d \"%s\"", t, strerror_wrapper(t).c_str()) });
				}
			} else {
				if constexpr(is_string_type<T>) {
					if(i == 0) {
						entry.message = t;
						return;
					}
				}
				entry.entries.push_back({ args_strings[i], stringify(t, literal_format::dec) });
			}
		}
	}

	template<typename... Args>
	ASSERT_DETAIL_ATTR_COLD [[nodiscard]]
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

	class assertion_printer {
		const assert_static_parameters* params;
		const extra_diagnostics& processed_args;
		binary_diagnostics_descriptor& binary_diagnostics;
		void* raw_trace; // TODO
		size_t sizeof_args;
	public:
		assertion_printer() = delete;
		assertion_printer(const assert_static_parameters* params, const extra_diagnostics& processed_args,
		                  binary_diagnostics_descriptor& binary_diagnostics, void* raw_trace, size_t sizeof_args);
		~assertion_printer();
		assertion_printer(const assertion_printer&) = delete;
		assertion_printer(assertion_printer&&) = delete;
		assertion_printer& operator=(const assertion_printer&) = delete;
		assertion_printer& operator=(assertion_printer&&) = delete;
		std::string operator()(int width);
	};

	template<typename A, typename B, typename C, typename... Args>
	ASSERT_DETAIL_ATTR_COLD ASSERT_DETAIL_ATTR_NOINLINE
	void assert_fail(expression_decomposer<A, B, C>& decomposer,
	                 const assert_static_parameters* params, Args&&... args) {
		lock l;
		const auto args_strings = params->args_strings;
		size_t args_strings_count = count_args_strings(args_strings);
		assert_detail_primitive_assert((sizeof...(args) == 0 && args_strings_count == 2)
		                               || args_strings_count == sizeof...(args) + 1);
		// process_args needs to be called as soon as possible in case errno needs to be read
		const auto processed_args = process_args(args_strings, args...);
		const auto fatal = processed_args.fatality;
		void* raw_trace = get_stacktrace_opaque();
		// generate header
		std::string output;
		binary_diagnostics_descriptor binary_diagnostics;
		// generate binary diagnostics
		if constexpr(is_nothing<C>) {
			static_assert(is_nothing<B> && !is_nothing<A>);
			(void)decomposer; // suppress warning in msvc
		} else {
			auto [a_str, b_str] = decompose_expression(params->expr_str, C::op_string);
			binary_diagnostics = print_binary_diagnostic(decomposer.a, decomposer.b,
			                                             a_str.c_str(), b_str.c_str(), C::op_string);
		}
		// send off
		assertion_printer printer {
			params,
			processed_args,
			binary_diagnostics,
			raw_trace,
			sizeof...(args)
		};
		::ASSERT_FAIL(printer, params->type, fatal);
	}

	template<typename A, typename B, typename C, typename... Args>
	ASSERT_DETAIL_ATTR_COLD ASSERT_DETAIL_ATTR_NOINLINE [[nodiscard]]
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
		[[maybe_unused]] constexpr bool ret_lhs = decomposer.ret_lhs();
		if(assert_detail_strong_expect(!static_cast<bool>(value), 0)) {
			#ifdef NDEBUG
			 if(params->type == assert_type::assertion) { // will be constant propagated
				ASSERT_DETAIL_UNREACHABLE;
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
		#ifdef ASSERT_DETAIL_IS_DEMO
		} else {
			assert_detail_primitive_assert(false);
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

#if ASSERT_DETAIL_IS_CLANG || ASSERT_DETAIL_IS_GCC
 // Macro mapping utility by William Swanson https://github.com/swansontec/map-macro/blob/master/map.h
 #define ASSERT_DETAIL_EVAL0(...) __VA_ARGS__
 #define ASSERT_DETAIL_EVAL1(...) ASSERT_DETAIL_EVAL0(ASSERT_DETAIL_EVAL0(ASSERT_DETAIL_EVAL0(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL2(...) ASSERT_DETAIL_EVAL1(ASSERT_DETAIL_EVAL1(ASSERT_DETAIL_EVAL1(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL3(...) ASSERT_DETAIL_EVAL2(ASSERT_DETAIL_EVAL2(ASSERT_DETAIL_EVAL2(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL4(...) ASSERT_DETAIL_EVAL3(ASSERT_DETAIL_EVAL3(ASSERT_DETAIL_EVAL3(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL(...)  ASSERT_DETAIL_EVAL4(ASSERT_DETAIL_EVAL4(ASSERT_DETAIL_EVAL4(__VA_ARGS__)))
 #define ASSERT_DETAIL_MAP_END(...)
 #define ASSERT_DETAIL_MAP_OUT
 #define ASSERT_DETAIL_MAP_COMMA ,
 #define ASSERT_DETAIL_MAP_GET_END2() 0, ASSERT_DETAIL_MAP_END
 #define ASSERT_DETAIL_MAP_GET_END1(...) ASSERT_DETAIL_MAP_GET_END2
 #define ASSERT_DETAIL_MAP_GET_END(...) ASSERT_DETAIL_MAP_GET_END1
 #define ASSERT_DETAIL_MAP_NEXT0(test, next, ...) next ASSERT_DETAIL_MAP_OUT
 #define ASSERT_DETAIL_MAP_NEXT1(test, next) ASSERT_DETAIL_MAP_NEXT0(test, next, 0)
 #define ASSERT_DETAIL_MAP_NEXT(test, next)  ASSERT_DETAIL_MAP_NEXT1(ASSERT_DETAIL_MAP_GET_END test, next)
 #define ASSERT_DETAIL_MAP0(f, x, peek, ...) f(x) ASSERT_DETAIL_MAP_NEXT(peek, ASSERT_DETAIL_MAP1)(f, peek, __VA_ARGS__)
 #define ASSERT_DETAIL_MAP1(f, x, peek, ...) f(x) ASSERT_DETAIL_MAP_NEXT(peek, ASSERT_DETAIL_MAP0)(f, peek, __VA_ARGS__)
 #define ASSERT_DETAIL_MAP_LIST_NEXT1(test, next) ASSERT_DETAIL_MAP_NEXT0(test, ASSERT_DETAIL_MAP_COMMA next, 0)
 #define ASSERT_DETAIL_MAP_LIST_NEXT(test, next)  ASSERT_DETAIL_MAP_LIST_NEXT1(ASSERT_DETAIL_MAP_GET_END test, next)
 #define ASSERT_DETAIL_MAP_LIST0(f, x, peek, ...) \
                                   f(x) ASSERT_DETAIL_MAP_LIST_NEXT(peek, ASSERT_DETAIL_MAP_LIST1)(f, peek, __VA_ARGS__)
 #define ASSERT_DETAIL_MAP_LIST1(f, x, peek, ...) \
                                   f(x) ASSERT_DETAIL_MAP_LIST_NEXT(peek, ASSERT_DETAIL_MAP_LIST0)(f, peek, __VA_ARGS__)
 #define ASSERT_DETAIL_MAP(f, ...) ASSERT_DETAIL_EVAL(ASSERT_DETAIL_MAP1(f, __VA_ARGS__, ()()(), ()()(), ()()(), 0))
#else
 // https://stackoverflow.com/a/29474124/15675011
 #define ASSERT_DETAIL_PLUS_TEXT_(x,y) x ## y
 #define ASSERT_DETAIL_PLUS_TEXT(x, y) ASSERT_DETAIL_PLUS_TEXT_(x, y)
 #define ASSERT_DETAIL_ARG_1(_1, ...) _1
 #define ASSERT_DETAIL_ARG_2(_1, _2, ...) _2
 #define ASSERT_DETAIL_ARG_3(_1, _2, _3, ...) _3
 #define ASSERT_DETAIL_ARG_40( _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, \
                 _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, \
                 _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, \
                 _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, \
                 ...) _39
 #define ASSERT_DETAIL_OTHER_1(_1, ...) __VA_ARGS__
 #define ASSERT_DETAIL_OTHER_3(_1, _2, _3, ...) __VA_ARGS__
 #define ASSERT_DETAIL_EVAL0(...) __VA_ARGS__
 #define ASSERT_DETAIL_EVAL1(...) ASSERT_DETAIL_EVAL0(ASSERT_DETAIL_EVAL0(ASSERT_DETAIL_EVAL0(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL2(...) ASSERT_DETAIL_EVAL1(ASSERT_DETAIL_EVAL1(ASSERT_DETAIL_EVAL1(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL3(...) ASSERT_DETAIL_EVAL2(ASSERT_DETAIL_EVAL2(ASSERT_DETAIL_EVAL2(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL4(...) ASSERT_DETAIL_EVAL3(ASSERT_DETAIL_EVAL3(ASSERT_DETAIL_EVAL3(__VA_ARGS__)))
 #define ASSERT_DETAIL_EVAL(...) ASSERT_DETAIL_EVAL4(ASSERT_DETAIL_EVAL4(ASSERT_DETAIL_EVAL4(__VA_ARGS__)))
 #define ASSERT_DETAIL_EXPAND(x) x
 #define ASSERT_DETAIL_MAP_SWITCH(...)\
     ASSERT_DETAIL_EXPAND(ASSERT_DETAIL_ARG_40(__VA_ARGS__, 2, 2, 2, 2, 2, 2, 2, 2, 2,\
             2, 2, 2, 2, 2, 2, 2, 2, 2, 2,\
             2, 2, 2, 2, 2, 2, 2, 2, 2,\
             2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0))
 #define ASSERT_DETAIL_MAP_A(...) ASSERT_DETAIL_PLUS_TEXT(ASSERT_DETAIL_MAP_NEXT_, \
                                            ASSERT_DETAIL_MAP_SWITCH(0, __VA_ARGS__)) (ASSERT_DETAIL_MAP_B, __VA_ARGS__)
 #define ASSERT_DETAIL_MAP_B(...) ASSERT_DETAIL_PLUS_TEXT(ASSERT_DETAIL_MAP_NEXT_, \
                                            ASSERT_DETAIL_MAP_SWITCH(0, __VA_ARGS__)) (ASSERT_DETAIL_MAP_A, __VA_ARGS__)
 #define ASSERT_DETAIL_MAP_CALL(fn, Value) ASSERT_DETAIL_EXPAND(fn(Value))
 #define ASSERT_DETAIL_MAP_OUT
 #define ASSERT_DETAIL_MAP_NEXT_2(...)\
     ASSERT_DETAIL_MAP_CALL(ASSERT_DETAIL_EXPAND(ASSERT_DETAIL_ARG_2(__VA_ARGS__)), \
     ASSERT_DETAIL_EXPAND(ASSERT_DETAIL_ARG_3(__VA_ARGS__))) \
     ASSERT_DETAIL_EXPAND(ASSERT_DETAIL_ARG_1(__VA_ARGS__)) \
     ASSERT_DETAIL_MAP_OUT \
     (ASSERT_DETAIL_EXPAND(ASSERT_DETAIL_ARG_2(__VA_ARGS__)), ASSERT_DETAIL_EXPAND(ASSERT_DETAIL_OTHER_3(__VA_ARGS__)))
 #define ASSERT_DETAIL_MAP_NEXT_0(...)
 #define ASSERT_DETAIL_MAP(...)    ASSERT_DETAIL_EVAL(ASSERT_DETAIL_MAP_A(__VA_ARGS__))
#endif

#define ASSERT_DETAIL_STRINGIFY(x) #x,
#define ASSERT_DETAIL_COMMA ,

#if ASSERT_DETAIL_IS_CLANG || ASSERT_DETAIL_IS_GCC
 #define ASSERT_DETAIL_STMTEXPR(B, R) __extension__ ({ B; R; })
 #define ASSERT_DETAIL_WARNING_PRAGMA _Pragma("GCC diagnostic ignored \"-Wparentheses\"")
#else
 #define ASSERT_DETAIL_STMTEXPR(B, R) [&] { B; return R; } ()
 #define ASSERT_DETAIL_WARNING_PRAGMA
#endif

#if ASSERT_DETAIL_IS_GCC
 // __VA_OPT__ needed for GCC, https://gcc.gnu.org/bugzilla/show_bug.cgi?id=44317
 #define ASSERT_DETAIL_VA_ARGS(...) __VA_OPT__(,) __VA_ARGS__
#else
 // clang properly eats the comma with ##__VA_ARGS__
 #define ASSERT_DETAIL_VA_ARGS(...) , ##__VA_ARGS__
#endif

// __PRETTY_FUNCTION__ used because __builtin_FUNCTION() used in source_location (like __FUNCTION__) is just the method
// name, not signature
#define ASSERT_DETAIL_STATIC_DATA(name, type, expr_str, ...) \
                                  ASSERT_DETAIL_STMTEXPR( \
                                    /* extra string here because of extra comma from map, also serves as terminator */ \
                                    /* ASSERT_DETAIL_STRINGIFY ASSERT_DETAIL_VA_ARGS because msvc */ \
                                    static constexpr const char* const arg_strings[] = { \
                                      ASSERT_DETAIL_MAP(ASSERT_DETAIL_STRINGIFY ASSERT_DETAIL_VA_ARGS(__VA_ARGS__)) "" \
                                    }; \
                                    static constexpr assert_detail::assert_static_parameters params = { \
                                      name ASSERT_DETAIL_COMMA \
                                      type ASSERT_DETAIL_COMMA \
                                      expr_str ASSERT_DETAIL_COMMA \
                                      ASSERT_DETAIL_PFUNC ASSERT_DETAIL_COMMA \
                                      arg_strings ASSERT_DETAIL_COMMA \
                                    }, \
                                    &params \
                                  )

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
                            ASSERT_DETAIL_STMTEXPR(, \
                              ASSERT_DETAIL_WARNING_PRAGMA \
                              assert_detail::expression_decomposer(assert_detail::expression_decomposer{} << expr) \
                            ), \
                            ASSERT_DETAIL_STATIC_DATA(name, assert_detail::assert_type::assertion, #expr, __VA_ARGS__) \
                            ASSERT_DETAIL_VA_ARGS(__VA_ARGS__) \
                          )

#define ASSERT(expr, ...) ASSERT_INVOKE(expr, "ASSERT", __VA_ARGS__)

#ifndef ASSERT_NO_LOWERCASE
 // provided for <assert.h> compatability
 #define assert(expr, ...) ASSERT_INVOKE(expr, "assert", __VA_ARGS__)
#endif

#define VERIFY(expr, ...) assert_detail::assert<true>( \
                            ASSERT_DETAIL_STMTEXPR(, \
                              ASSERT_DETAIL_WARNING_PRAGMA \
                              assert_detail::expression_decomposer(assert_detail::expression_decomposer{} << expr) \
                            ), \
                            ASSERT_DETAIL_STATIC_DATA("VERIFY", assert_detail::assert_type::verify, \
                                                      #expr, __VA_ARGS__) \
                            ASSERT_DETAIL_VA_ARGS(__VA_ARGS__) \
                          )

#ifndef NDEBUG
 #define CHECK(expr, ...) assert_detail::assert<false>( \
                            ASSERT_DETAIL_STMTEXPR(, \
                              ASSERT_DETAIL_WARNING_PRAGMA \
                              assert_detail::expression_decomposer(assert_detail::expression_decomposer{} << expr) \
                            ), \
                            ASSERT_DETAIL_STATIC_DATA("CHECK", assert_detail::assert_type::check, #expr, __VA_ARGS__) \
                            ASSERT_DETAIL_VA_ARGS(__VA_ARGS__) \
                          )
#else
 // omitting the expression could cause unused variable warnings, surpressing for now
 // TODO: Is this the right design decision? Re-evaluated whether PHONY_USE should be used here or internally at all
 #define CHECK(expr, ...) ASSERT_DETAIL_PHONY_USE(expr)
#endif

#ifndef ASSERT_DETAIL_IS_CPP // keep macros for the .cpp
 #undef ASSERT_DETAIL_IS_CLANG
 #undef ASSERT_DETAIL_IS_GCC
 #undef ASSERT_DETAIL_IS_MSVC
 #undef ASSERT_DETAIL_ATTR_COLD
 #undef ASSERT_DETAIL_ATTR_NOINLINE
 #undef assert_detail_strong_expect
 #undef assert_detail_primitive_assert
#endif

#endif
