#include <iomanip>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <stdlib.h>
#include <string_view>
#include <string.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

#ifndef NCOLOR
#define ESC "\033["
#define RED ESC "1;31m"
#define GREEN ESC "1;32m"
#define BLUE ESC "1;34m"
#define CYAN ESC "1;36m"
#define PURPL ESC "1;35m"
#define DARK ESC "1;30m"
#define RESET ESC "0m"
#else
#define RED ""
#define GREEN ""
#define BLUE ""
#define CYAN ""
#define PURPL ""
#define DARK ""
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

	template<typename I>
	[[gnu::cold]]
	static std::string join(I iter, I end, const std::string& delim) {
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

	template<class C>
	[[gnu::cold]]
	static std::string join(C container, std::string delim) {
		return join(container.begin(), container.end(), delim);
	}

	template<size_t N>
	[[gnu::cold]]
	static std::string join(const std::string (&container)[N], std::string delim) {
		return join(std::begin(container), std::end(container), delim);
	}

	// boilerplate
	// std:: implementations don't allow two separate types for lhs/rhs
	struct equal_to {
		template<typename A, typename B>
		constexpr bool operator()(const A& lhs, const B& rhs) const {
			return lhs == rhs;
		}
	};

	struct not_equal_to {
		template<typename A, typename B>
		constexpr bool operator()(const A& lhs, const B& rhs) const {
			return lhs != rhs;
		}
	};

	struct greater {
		template<typename A, typename B>
		constexpr bool operator()(const A& lhs, const B& rhs) const {
			return lhs > rhs;
		}
	};

	struct less {
		template<typename A, typename B>
		constexpr bool operator()(const A& lhs, const B& rhs) const {
			return lhs < rhs;
		}
	};

	struct greater_equal {
		template<typename A, typename B>
		constexpr bool operator()(const A& lhs, const B& rhs) const {
			return lhs >= rhs;
		}
	};

	struct less_equal {
		template<typename A, typename B>
		constexpr bool operator()(const A& lhs, const B& rhs) const {
			return lhs <= rhs;
		}
	};

	struct logical_and {
		template<typename A, typename B>
		constexpr bool operator()(const A& lhs, const B& rhs) const {
			return lhs && rhs;
		}
	};

	struct logical_or {
		template<typename A, typename B>
		constexpr bool operator()(const A& lhs, const B& rhs) const {
			return lhs || rhs;
		}
	};

	template<class T> constexpr const char* operator_string = nullptr;
	template<> constexpr const char* operator_string<equal_to     > = "==";
	template<> constexpr const char* operator_string<not_equal_to > = "!=";
	template<> constexpr const char* operator_string<greater      > = ">";
	template<> constexpr const char* operator_string<less         > = "<";
	template<> constexpr const char* operator_string<greater_equal> = ">=";
	template<> constexpr const char* operator_string<less_equal   > = "<=";
	template<> constexpr const char* operator_string<logical_and  > = "&&";
	template<> constexpr const char* operator_string<logical_or   > = "||";

	// macro to make asserts non-fatal
	enum class ASSERT {
		NONFATAL, FATAL
	};

	template<char quote>
	[[gnu::cold]]
	std::string escape_string(const std::string& str) {
		std::string escaped;
		escaped += quote;
		for(unsigned char c : str) {
			switch(c) {
				case quote: escaped += "\\" + std::to_string(quote); break;
				case '\\':  escaped += "\\\\"; break;
				case '\t':  escaped += "\\t";  break;
				case '\r':  escaped += "\\r";  break;
				case '\n':  escaped += "\\n";  break;
				default:
					if(c >= 32 && c <= 126) { // printable
						escaped += c;
					} else {
						constexpr const char * const hexdig = "0123456789abcdef";
						escaped += std::string("\\x") + hexdig[c >> 4] + hexdig[c & 0xF];
					}
			}
		}
		escaped += quote;
		return escaped;
	}

	[[maybe_unused]]
	static std::string indent(std::string str, size_t count, char indent=' ', bool ignore_first=false) {
		size_t i = 0;
		while((i = str.find('\n', i)) != std::string::npos) {
			i++;
			str.insert(i, count, indent);
		}
		if(!ignore_first) str.insert(0, count, indent);
		return str;
	}

	template<typename T> using strip = std::remove_cv_t<std::remove_reference_t<T>>;

	enum class literal_format {
		dec,
		hex,
		octal,
		binary,
		none
	};

	template<class T>
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

	// TODO: This should be implemented better. More robustly.
	// TODO: Improve literal system from hard-coded switches...?
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
					// TODO: re-evaluate this...? What if just comparing two buffer pointers?
					return "nullptr";
				}
			}
			return escape_string<'"'>(std::string(t)); // string view may need explicit construction?
		} else if constexpr (std::is_same_v<strip<T>, char>) {
			return escape_string<'\''>({t});
		} else if constexpr (std::is_same_v<strip<T>, bool>) {
			return t ? "true" : "false"; // streams/boolalpha not needed for this
		} else if constexpr (std::is_integral_v<T>) {
			std::ostringstream oss;
			switch(fmt) {
				case literal_format::dec:
					break;
				case literal_format::hex:
					oss<<"0x"<<std::hex;
					break;
				case literal_format::octal:
					oss<<"0"<<std::oct;
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
					oss<<"0x"<<std::hexfloat;
					break;
				default:
					primitive_assert(false, "unexpected literal format requested for printing");
			}
			oss<<std::setprecision(std::numeric_limits<T>::max_digits10)<<t;
			return std::move(oss).str();
		} else if constexpr (std::is_same_v<T, std::nullptr_t>) {
			return "nullptr";
		} else if constexpr (std::is_pointer_v<T>) {
			if(t == nullptr) { // weird nullptr shenanigans, only prints "nullptr" for nullptr_t
				return "nullptr";
			} else {
				std::ostringstream oss;
				oss<<t;
				return std::move(oss).str();
			}
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
	static std::string union_regexes(std::initializer_list<std::string> regexes) {
		std::string composite;
		for(const std::string& str : regexes) {
			if(composite != "") composite += "|";
			composite += stringf("(?:%s)", str.c_str());
		}
		return composite;
	}
	
	// file-local singleton, at most 8 BSS bytes per TU in debug mode is no problem
	// at runtime could be 464 heap bytes per TU but in practice should only happen in one TU
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

	private:
		// could all be const but I don't want to try to pack everything into an init list
		std::vector<std::pair<token_e, std::regex>> rules; // could be std::array but I don't want to hard-code a size
		std::string int_binary;
		std::string int_octal;
		std::string int_decimal;
		std::string int_hex;
		std::string float_decimal;
		std::string float_hex;
		std::unordered_map<std::string, int> precedence;
		std::unordered_map<std::string, std::string> braces = {
			// template angle brackets excluded from this analysis
			{ "(", ")" }, { "{", "}" }, { "[", "]" }
		};
		// punctuators to highlight
		std::unordered_set<std::string> highlight_ops = {
			"~", "!", "+", "-", "*", "/", "%", "^", "&", "|", "=", "+=", "-=", "*=", "/=", "%=",
			"^=", "&=", "|=", "==", "!=", "<", ">", "<=", ">=", "<=>", "&&", "||", "<<", ">>",
			"<<=", ">>=", "++", "--", "and", "or", "xor", "not", "bitand", "bitor", "compl",
			"and_eq", "or_eq", "xor_eq", "not_eq"
		};
		// all operators
		std::unordered_set<std::string> operators = {
			":", "...", "?", "::", ".", ".*", "->", "->*", "~", "!", "+", "-", "*", "/", "%", "^",
			"&", "|", "=", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "==", "!=", "<", ">",
			"<=", ">=", "<=>", "&&", "||", "<<", ">>", "<<=", ">>=", "++", "--", ",", "and", "or",
			"xor", "not", "bitand", "bitor", "compl", "and_eq", "or_eq", "xor_eq", "not_eq"
		};
		std::vector<std::pair<std::regex, literal_format>> literal_formats;

		[[gnu::cold]]
		analysis() {
			// https://eel.is/c++draft/gram.lex
			// generate regular expressions
			std::string keywords[] = { "alignas", "constinit", "false", "public", "true",
				"alignof", "const_cast", "float", "register", "try", "asm", "continue", "for",
				"reinterpret_cast", "typedef", "auto", "co_await", "friend", "requires", "typeid",
				"bool", "co_return", "goto", "return", "typename", "break", "co_yield", "if",
				"short", "union", "case", "decltype", "inline", "signed", "unsigned", "catch",
				"default", "int", "sizeof", "using", "char", "delete", "long", "static", "virtual",
				"char8_t", "do", "mutable", "static_assert", "void", "char16_t", "double",
				"namespace", "static_cast", "volatile", "char32_t", "dynamic_cast", "new", "struct",
				"wchar_t", "class", "else", "noexcept", "switch", "while", "concept", "enum",
				"nullptr", "template", "const", "explicit", "operator", "this", "consteval",
				"export", "private", "thread_local", "constexpr", "extern", "protected", "throw" };
			std::string punctuators[] = { "{", "}", "[", "]", "(", ")", "<:", ":>", "<%",
				"%>", ";", ":", "...", "?", "::", ".", ".*", "->", "->*", "~", "!", "+", "-", "*",
				"/", "%", "^", "&", "|", "=", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "==",
				"!=", "<", ">", "<=", ">=", "<=>", "&&", "||", "<<", ">>", "<<=", ">>=", "++", "--",
				",", "and", "or", "xor", "not", "bitand", "bitor", "compl", "and_eq", "or_eq",
				"xor_eq", "not_eq" };
			// Sort longest -> shortest (secondarily A->Z)
			const auto cmp = [](const std::string& a, const std::string& b) {
				if(a.length() > b.length()) return true;
				else if(a.length() == b.length()) return a < b;
				else return false;
			};
			std::sort(std::begin(keywords), std::end(keywords), cmp);
			std::sort(std::begin(punctuators), std::end(punctuators), cmp);
			// Escape special characters
			const auto escape = [](const std::string& str) {
				const std::regex special_chars { R"([-[\]{}()*+?.,\^$|#\s])" };
				return std::regex_replace(str, special_chars, "\\$&");
			};
			std::transform(std::begin(keywords), std::end(keywords), std::begin(keywords), escape);
			std::transform(std::begin(punctuators), std::end(punctuators), std::begin(punctuators), escape);
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
			std::string char_literal = R"((?:u8|[UuL])?'(?:\\[0-7]{1,3}|\\x[\da-fA-F]+|\\.|[^\n'])*')";
			std::string string_literal = R"((?:u8|[UuL])?"(?:\\[0-7]{1,3}|\\x[\da-fA-F]+|\\.|[^\n"])*")";
			std::string raw_string_literal = R"((?:u8|[UuL])?R"([^ ()\\t\r\v\n]*)\((?:(?!\)\2\").)*\)\2")"; // \2 because the first capture is the match without the rest of the file
			// final rule set
			// regex is greedy, sequencing rules are as follow:
			// keyword > identifier
			// number > punctuation (for .1f)
			// float > int (for 1.5)
			// hex int > decimal int
			// named_literal > identifier
			// string > identifier (for R"(foobar)")
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
				//fprintf(stderr, "%s : %s\n", rules_raw[i].first.c_str(), str.c_str());
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
			const std::unordered_map<int, std::vector<std::string>> precedences = {
				{  0, { "<", "<=", ">=", ">" } },
				{ -1, { "==", "!=" } },
				{ -2, { "&" } },
				{ -3, { "^" } },
				{ -4, { "|" } },
				{ -5, { "&&" } },
				{ -6, { "||" } },
				{ -7, { "?", ":", "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=" } },
				{ -8, { "," } }
			};
			std::unordered_map<std::string, int> table;
			for(const auto& [ p, ops ] : precedences) {
				for(const auto& op : ops) {
					table.insert({op, p});
				}
			}
			precedence = table;
		}

		[[gnu::cold]]
		std::vector<token_t> _tokenize(const std::string& expression) {
			std::vector<token_t> tokens;
			size_t i = 0;
			while(i < expression.length()) {
				std::smatch match;
				bool at_least_one_matched = false;
				for(const auto& [ type, re ] : rules) {
					if(std::regex_match(std::begin(expression) + i, std::end(expression), match, re)) {
						//fprintf(stderr, "%s : %s\n", name.c_str(), match[1].str().c_str());
						//fflush(stdout);
						tokens.push_back({ type, match[1].str() });
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
		std::string _highlight(const std::string& expression) try {
			// TODO: improve highlighting, will require more situational awareness
			const auto tokens = tokenize(expression);
			std::string str;
			for(const auto& token : tokens) {
				switch(token.token_type) {
					case token_e::keyword:
						str += PURPL + token.str + RESET;
						break;
					case token_e::punctuation:
						if(highlight_ops.count(token.str)) {
							str += BLUE + token.str + RESET;
						} else {
							str += token.str;
						}
						break;
					case token_e::named_literal:
						str += RED + token.str + RESET;
						break;
					case token_e::number:
						str += CYAN + token.str + RESET;
						break;
					case token_e::string:
						str += GREEN + token.str + RESET;
						break;
					case token_e::identifier:
					case token_e::whitespace:
						str += token.str;
						break;
				}
			}
			return str;
		} catch(...) {
			// yes, yes, never use exceptions for control flow, but performance really does not matter here!
			return expression;
		}

		[[gnu::cold]]
		literal_format _get_literal_format(const std::string& expression) {
			std::smatch match;
			for(auto& [ re, type ] : literal_formats) {
				if(std::regex_match(expression, match, re)) {
					return type;
				}
			}
			return literal_format::none; // not a literal
		}

		[[gnu::cold]]
		bool _has_precedence_below_or_equal(const std::string& expression, const std::string& op) {
			// TODO: there is redundant tokenization here
			const auto tokens = analysis::tokenize(expression);
			// precedence table is binary, unary operators have highest precedence
			// we can figure out unary / binary easy enough
			enum {
				expecting_operator,
				expecting_term
			} state = expecting_term;
			for(std::size_t i = 0; i < tokens.size(); i++) {
				const analysis::token_t& token = tokens[i];
				switch(token.token_type) {
					case analysis::token_e::punctuation:
						if(operators.count(token.str)) {
							if(state == expecting_term) {
								// must be unary, continue
							} else {
								// binary
								if(precedence.count(token.str)
								&& precedence.count(op)
								&& precedence.at(token.str) <= precedence.at(op)) {
									return true;
								}
								state = expecting_term;
							}
						} else if(braces.count(token.str)) {
							// We can assume the given expression is valid.
							// Braces must be balanced.
							// Scan forward until finding matching brace, don't need to take into
							// account other types of braces.
							const std::string& open = token.str;
							const std::string& close = braces.at(token.str);
							int count = 0;
							while(++i < tokens.size()) {
								if(tokens[i].str == open) count++;
								else if(tokens[i].str == close) {
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
					case analysis::token_e::keyword:
					case analysis::token_e::named_literal:
					case analysis::token_e::number:
					case analysis::token_e::string:
					case analysis::token_e::identifier:
						state = expecting_operator;
					case analysis::token_e::whitespace:
						break;
				}
			}
			return false;
		}

	public:
		// public static wrappers for
		[[gnu::cold]]
		static std::vector<token_t> tokenize(const std::string& expression) {
			return get()._tokenize(expression);
		}
		[[gnu::cold]]
		static std::string highlight(const std::string& expression) {
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
		static bool has_precedence_below_or_equal(const std::string& expression, const std::string& op) {
			return get()._has_precedence_below_or_equal(expression, op);
		}
	};

	[[gnu::cold]] [[maybe_unused]]
	static std::string parenthesize_if_necessary(const std::string& expression, const std::string& op) {
		if(analysis::has_precedence_below_or_equal(expression, op)) {
			return "(" + expression + ")";
		} else {
			return expression;
		}
	}

	[[gnu::cold]] [[maybe_unused]]
	static std::string gen_assert_binary(std::string a_str, const std::string& op, std::string b_str) {
		return stringf("assert(%s %s %s);",
					   parenthesize_if_necessary(a_str, op).c_str(),
					   op.c_str(),
					   parenthesize_if_necessary(b_str, op).c_str());
	}

	template<typename T>
	[[gnu::cold]]
	std::vector<std::string> generate_stringifications(T&& v, const std::set<literal_format>& formats) {
		if constexpr(std::is_arithmetic<T>::value
				 && !std::is_same_v<strip<T>, bool>
				 && !std::is_same_v<strip<T>, char>) {
			std::vector<std::string> vec;
			for(literal_format fmt : formats) {
				vec.push_back(stringify(v, fmt));
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
			if(vec.size() > 1) fprintf(stderr, " ");
			for(const auto& str : vec) {
				fprintf(stderr, "%s", analysis::highlight(str).c_str());
				if(str != *vec.end()--) fprintf(stderr, "  ");
			}
			fprintf(stderr, "\n");
		}
	}

	template<typename C, typename A, typename B>
	[[gnu::cold]]
	void print_binary_diagnostic(A&& a, B&& b, const char* a_str, const char* b_str) {
		// figure out what information we need to print in the where clause
		// find all literal formats involved (literal_format::dec included for everything)
		literal_format lformat = analysis::get_literal_format(a_str);
		literal_format rformat = analysis::get_literal_format(b_str);
		std::set<literal_format> formats = { literal_format::dec, lformat, rformat };
		formats.erase(literal_format::none); // none is just for when the expression isn't a literal
		primitive_assert(formats.size() > 0);
		// generate raw strings for given formats, without highlighting
		auto lstrings = generate_stringifications(std::forward<A>(a), formats);
		auto rstrings = generate_stringifications(std::forward<B>(b), formats);
		// pad all columns where there is overlap (last column excluded as padding is not necessary there)
		for(size_t i = 0; i < std::min(lstrings.size(), rstrings.size()) - 1; i++) {
			std::string& shorter_column = lstrings[i].length() < rstrings[i].length() ? lstrings[i] : rstrings[i];
			int difference = std::abs((int)lstrings[i].length() - (int)rstrings[i].length());
			shorter_column.insert(shorter_column.end(), difference, ' ');
		}
		primitive_assert(lstrings.size() > 0);
		primitive_assert(rstrings.size() > 0);
		// determine whether we actually gain anything from printing a where clause (e.g. exclude "1 => 1")
		struct { bool left, right; } has_useful_where_clause = {
			formats.size() > 1 || lstrings.size() > 1 || a_str != lstrings[0],
			formats.size() > 1 || rstrings.size() > 1 || b_str != rstrings[0]
		};
		// print assert statement
		fprintf(stderr, "    %s\n", analysis::highlight(gen_assert_binary(a_str, operator_string<C>, b_str)).c_str());
		// print where clause
		if(has_useful_where_clause.left || has_useful_where_clause.right) {
			size_t lw = std::max(
				has_useful_where_clause.left  ? strlen(a_str) : 0,
				has_useful_where_clause.right ? strlen(b_str) : 0
			);
			fprintf(stderr, "    Where:\n");
			if(has_useful_where_clause.left) {
				fprintf(stderr, "        %s%*s => ",
						analysis::highlight(a_str).c_str(), int(lw - strlen(a_str)), "");
					print_values(lstrings, lw);
			}
			if(has_useful_where_clause.right) {
				fprintf(stderr, "        %s%*s => ",
						analysis::highlight(b_str).c_str(), int(lw - strlen(b_str)), "");
					print_values(rstrings, lw);
			}
		}
	}

	[[gnu::cold]] [[gnu::noinline]]
	static void assert_fail(const char* expr_str, const char* pretty_func, const char* info, ASSERT fatal, source_location location) {
		if(info == nullptr) {
			fprintf(stderr, "Assertion failed at %s:%d: %s:\n"
				    "    %s\n",
				    location.file, location.line, pretty_func,
				    analysis::highlight(stringf("assert(%s);", expr_str)).c_str());
		} else {
			fprintf(stderr, "Assertion failed at %s:%d: %s: %s\n"
				   "    %s\n",
				   location.file, location.line, pretty_func, info,
				   analysis::highlight(stringf("assert(%s);", expr_str)).c_str());
		}
		if(fatal == ASSERT::FATAL) {
			#ifdef ASSERT_DEMO
			printf("\n");
			#else
			fflush(stdout);
			abort();
			#endif
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
		print_binary_diagnostic<C>(std::forward<A>(a), std::forward<B>(b), a_str, b_str);
		if(fatal == ASSERT::FATAL) {
			#ifdef ASSERT_DEMO
			printf("\n");
			#else
			fflush(stdout);
			abort();
			#endif
		}
	}

	// top-level assert functions emplaced by the macros
	// these are the only non-cold functions

	[[maybe_unused]]
	static void assert(bool expr, const char* expr_str, const char* pretty_func, const char* info = nullptr, ASSERT fatal = ASSERT::FATAL, source_location location = {}) {
		if(!expr) {
			assert_fail(expr_str, pretty_func, info, fatal, location);
		} else {
			#ifdef ASSERT_DEMO
			assert(false, "false", __PRETTY_FUNCTION__, "assert should have failed");
			#endif
		}
	}

	template<typename C, typename A, typename B>
	void assert_binary(A&& a, B&& b, const char* a_str, const char* b_str, const char* pretty_func, const char* info = nullptr, ASSERT fatal = ASSERT::FATAL, source_location location = {}) {
		if(!C()(a, b)) {
			assert_binary_fail<C>(std::forward<A>(a), std::forward<B>(b), a_str, b_str, pretty_func, info, fatal, location);
		} else {
			#ifdef ASSERT_DEMO
			assert(false, "false", __PRETTY_FUNCTION__, "assert should have failed");
			#endif
		}
	}

	// std::string info overloads

	[[maybe_unused]]
	static void assert(bool expr, const char* expr_str, const char* pretty_func, const std::string& info, ASSERT fatal = ASSERT::FATAL, source_location location = {}) {
		if(!expr) {
			assert_fail(expr_str, pretty_func, info.c_str(), fatal, location);
		} else {
			#ifdef ASSERT_DEMO
			assert(false, "false", __PRETTY_FUNCTION__, "assert should have failed");
			#endif
		}
	}

	template<typename C, typename A, typename B>
	void assert_binary(A&& a, B&& b, const char* a_str, const char* b_str, const char* pretty_func, const std::string& info, ASSERT fatal = ASSERT::FATAL, source_location location = {}) {
		if(!C()(a, b)) {
			assert_binary_fail<C>(std::forward<A>(a), std::forward<B>(b), a_str, b_str, pretty_func, info.c_str(), fatal, location);
		} else {
			#ifdef ASSERT_DEMO
			assert(false, "false", __PRETTY_FUNCTION__, "assert should have failed");
			#endif
		}
	}

	[[maybe_unused]]
	static void assume(bool expr) {
		if(!expr) {
			__builtin_unreachable();
		}
	}

	template<typename C, typename A, typename B>
	void assume(A&& a, B&& b) {
		if(!C()(a, b)) {
			__builtin_unreachable();
		}
	}

	#undef primitive_assert
}

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
 #define      assert(expr, ...) assert_impl_::assert(expr, #expr, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define   assert_eq(a, b, ...) assert_impl_::assert_binary<assert_impl_::equal_to     >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define  assert_neq(a, b, ...) assert_impl_::assert_binary<assert_impl_::not_equal_to >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define   assert_lt(a, b, ...) assert_impl_::assert_binary<assert_impl_::less         >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define   assert_gt(a, b, ...) assert_impl_::assert_binary<assert_impl_::greater      >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define assert_lteq(a, b, ...) assert_impl_::assert_binary<assert_impl_::less_equal   >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define assert_gteq(a, b, ...) assert_impl_::assert_binary<assert_impl_::greater_equal>(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define  assert_and(a, b, ...) assert_impl_::assert_binary<assert_impl_::logical_and  >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
 #define   assert_or(a, b, ...) assert_impl_::assert_binary<assert_impl_::logical_or   >(a, b, #a, #b, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#endif
