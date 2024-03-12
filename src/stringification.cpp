#include <bitset>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>

#include "analysis.hpp"
#include "utils.hpp"

#include <libassert/assert.hpp>

namespace libassert::detail {
    /*
     * literal format management
     */

    [[nodiscard]] constexpr std::underlying_type<literal_format>::type operator&(literal_format a, literal_format b) {
        return static_cast<std::underlying_type<literal_format>::type>(a) &
               static_cast<std::underlying_type<literal_format>::type>(b);
    }

    std::mutex literal_format_config_mutex;
    literal_format_mode current_literal_format_mode;
    literal_format current_fixed_literal_format;

    thread_local literal_format thread_current_literal_format = literal_format::default_format;
}

namespace libassert {
    LIBASSERT_EXPORT void set_literal_format_mode(literal_format_mode mode) {
        std::unique_lock lock(detail::literal_format_config_mutex);
        detail::current_literal_format_mode = mode;
    }

    LIBASSERT_EXPORT void set_fixed_literal_format(literal_format format) {
        std::unique_lock lock(detail::literal_format_config_mutex);
        detail::current_fixed_literal_format = format;
        detail::current_literal_format_mode = literal_format_mode::fixed_variations;
    }
}

namespace libassert::detail {
    std::pair<literal_format_mode, literal_format> get_literal_format_config() {
        std::unique_lock lock(literal_format_config_mutex);
        return {current_literal_format_mode, current_fixed_literal_format};
    }

    // get current literal_format configuration for the thread
    [[nodiscard]] LIBASSERT_EXPORT literal_format get_thread_current_literal_format() {
        return thread_current_literal_format;
    }

    // sets the current literal_format configuration for the thread
    LIBASSERT_EXPORT void set_thread_current_literal_format(literal_format format) {
        thread_current_literal_format = format;
    }

    LIBASSERT_EXPORT literal_format set_literal_format(
        std::string_view left_expression,
        std::string_view right_expression,
        std::string_view op,
        bool integer_character
    ) {
        auto previous = get_thread_current_literal_format();
        auto [mode, fixed_format] = get_literal_format_config();
        if(mode == literal_format_mode::infer) {
            auto lformat = get_literal_format(left_expression);
            auto rformat = get_literal_format(right_expression);
            auto format = lformat | rformat;
            if(integer_character) { // if one is a character and the other is not
                format = format | literal_format::integer_character;
            }
            if(is_bitwise(op)) {
                format = format | literal_format::integer_binary;
            }
            set_thread_current_literal_format(format);
        } else if(mode == literal_format_mode::no_variations) {
            set_thread_current_literal_format(literal_format::default_format);
        } else if(mode == literal_format_mode::fixed_variations) {
            set_thread_current_literal_format(fixed_format);
        } else {
            LIBASSERT_PRIMITIVE_ASSERT(false);
        }
        return previous;
    }

    LIBASSERT_EXPORT void restore_literal_format(literal_format format) {
        set_thread_current_literal_format(format);
    }

    constexpr auto non_default_integer_formats = literal_format::integer_hex
                                               | literal_format::integer_octal
                                               | literal_format::integer_binary
                                            //    | literal_format::integer_character
                                               ;

    constexpr auto non_default_float_formats = literal_format::float_hex;

    LIBASSERT_EXPORT bool has_multiple_formats() {
        auto format = get_thread_current_literal_format();
        return popcount(format & non_default_integer_formats) || popcount(format & non_default_float_formats);
    }

    /*
     * Stringification
     */

    LIBASSERT_ATTR_COLD
    static std::string escape_string(const std::string_view str, char quote) {
        std::string escaped;
        escaped += quote;
        for(const char c : str) {
            if(c == '\\') escaped += "\\\\";
            else if(c == '\t') escaped += "\\t";
            else if(c == '\r') escaped += "\\r";
            else if(c == '\n') escaped += "\\n";
            else if(c == quote) escaped += std::initializer_list<char>{'\\', quote};
            else if(c >= 32 && c <= 126) escaped += c; // printable
            else {
                constexpr const char * const hexdig = "0123456789abcdef";
                escaped += std::string("\\x") + hexdig[c >> 4] + hexdig[c & 0xF];
            }
        }
        escaped += quote;
        return escaped;
    }

    namespace stringification {
        LIBASSERT_ATTR_COLD std::string stringify(std::string_view value) {
            return escape_string(value, '"');
        }

        LIBASSERT_ATTR_COLD std::string stringify(std::nullptr_t) {
            return "nullptr";
        }

        LIBASSERT_ATTR_COLD std::string stringify(char value) {
            if(get_thread_current_literal_format() & literal_format::integer_character) {
                return stringify(static_cast<int>(value));
            } else {
                return escape_string({&value, 1}, '\'');
            }
        }

        LIBASSERT_ATTR_COLD std::string stringify(bool value) {
            return value ? "true" : "false";
        }

        template<typename T, typename std::enable_if<is_integral_and_not_bool<T>, int>::type = 0>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        static std::string stringify_integral(T value, literal_format format) {
            std::ostringstream oss;
            switch(format) {
                case literal_format::integer_character:
                    if(
                        cmp_greater_equal(value, std::numeric_limits<char>::min()) &&
                        cmp_less_equal(value, std::numeric_limits<char>::max())
                    ) {
                        char c = static_cast<char>(value);
                        return escape_string({&c, 1}, '\'');
                    } else {
                        // TODO: Handle this better
                        return "<no char>";
                    }
                    break;
                case literal_format::integer_hex:
                    oss<<std::showbase<<std::hex;
                    break;
                case literal_format::integer_octal:
                    oss<<std::showbase<<std::oct;
                    break;
                case literal_format::integer_binary:
                    oss<<"0b"<<std::bitset<sizeof(value) * 8>(value);
                    goto r;
                case literal_format::default_format:
                    break;
                default:
                    LIBASSERT_PRIMITIVE_ASSERT(false, "unexpected literal format requested for printing");
            }
            oss<<value;
            r: return std::move(oss).str();
        }

        template<typename T, typename std::enable_if<is_integral_and_not_bool<T>, int>::type = 0>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        static std::string stringify_integral(T value) {
            auto current_format = get_thread_current_literal_format();
            std::string result = stringify_integral(value, literal_format::default_format);
            if(current_format & literal_format::integer_character) {
                result = stringify_integral(value, literal_format::integer_character) + ' ' + std::move(result);
            }
            if(current_format & non_default_integer_formats) {
                for(auto format : {
                    literal_format::integer_hex,
                    literal_format::integer_octal,
                    literal_format::integer_binary
                }) {
                    if(current_format & format) {
                        result += ' ';
                        result += stringify_integral(value, format);
                    }
                }
            }
            return result;
        }

        LIBASSERT_ATTR_COLD std::string stringify(short value) {
            return stringify_integral(value);
        }

        LIBASSERT_ATTR_COLD std::string stringify(int value) {
            return stringify_integral(value);
        }

        LIBASSERT_ATTR_COLD std::string stringify(long value) {
            return stringify_integral(value);
        }

        LIBASSERT_ATTR_COLD std::string stringify(long long value) {
            return stringify_integral(value);
        }

        LIBASSERT_ATTR_COLD std::string stringify(unsigned short value) {
            return stringify_integral(value);
        }

        LIBASSERT_ATTR_COLD std::string stringify(unsigned int value) {
            return stringify_integral(value);
        }

        LIBASSERT_ATTR_COLD std::string stringify(unsigned long value) {
            return stringify_integral(value);
        }

        LIBASSERT_ATTR_COLD std::string stringify(unsigned long long value) {
            return stringify_integral(value);
        }

        template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        static std::string stringify_floating_point(T value, literal_format format) {
            std::ostringstream oss;
            if(format == literal_format::float_hex) {
                // apparently std::hexfloat automatically prepends "0x" while std::hex does not
                oss<<std::hexfloat;
            }
            oss<<std::setprecision(std::numeric_limits<T>::max_digits10)<<value;
            std::string s = std::move(oss).str();
            // std::showpoint adds a bunch of unecessary digits, so manually doing it correctly here
            if(s.find('.') == std::string::npos) {
                s += ".0";
            }
            return s;
        }

        template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        LIBASSERT_ATTR_COLD [[nodiscard]]
        static std::string stringify_floating_point(T value) {
            auto current_format = get_thread_current_literal_format();
            std::string result = stringify_floating_point(value, literal_format::default_format);
            if(current_format & non_default_float_formats) {
                for(auto format : { literal_format::float_hex }) {
                    if(current_format & format) {
                        if(!result.empty()) {
                            result += ' ';
                        }
                        result += stringify_floating_point(value, format);
                    }
                }
            }
            return result;
        }

        LIBASSERT_ATTR_COLD std::string stringify(float value) {
            return stringify_floating_point(value);
        }

        LIBASSERT_ATTR_COLD std::string stringify(double value) {
            return stringify_floating_point(value);
        }

        LIBASSERT_ATTR_COLD std::string stringify(long double value) {
            return stringify_floating_point(value);
        }

        LIBASSERT_ATTR_COLD std::string stringify(std::error_code ec) {
            return ec.category().name() + (':' + std::to_string(ec.value())) + ' ' + ec.message();
        }

        LIBASSERT_ATTR_COLD std::string stringify(std::error_condition ec) {
            return ec.category().name() + (':' + std::to_string(ec.value())) + ' ' + ec.message();
        }

        #if __cplusplus >= 202002L
        LIBASSERT_ATTR_COLD std::string stringify(std::strong_ordering value) {
                if(value == std::strong_ordering::less)       return "std::strong_ordering::less";
                if(value == std::strong_ordering::equivalent) return "std::strong_ordering::equivalent";
                if(value == std::strong_ordering::equal)      return "std::strong_ordering::equal";
                if(value == std::strong_ordering::greater)    return "std::strong_ordering::greater";
                return "Unknown std::strong_ordering value";
        }
        LIBASSERT_ATTR_COLD std::string stringify(std::weak_ordering value) {
                if(value == std::weak_ordering::less)       return "std::weak_ordering::less";
                if(value == std::weak_ordering::equivalent) return "std::weak_ordering::equivalent";
                if(value == std::weak_ordering::greater)    return "std::weak_ordering::greater";
                return "Unknown std::weak_ordering value";
        }
        LIBASSERT_ATTR_COLD std::string stringify(std::partial_ordering value) {
                if(value == std::partial_ordering::less)       return "std::partial_ordering::less";
                if(value == std::partial_ordering::equivalent) return "std::partial_ordering::equivalent";
                if(value == std::partial_ordering::greater)    return "std::partial_ordering::greater";
                if(value == std::partial_ordering::unordered)  return "std::partial_ordering::unordered";
                return "Unknown std::partial_ordering value";
        }
        #endif

        LIBASSERT_ATTR_COLD std::string stringify_pointer_value(const void* value) {
            if(value == nullptr) {
                return "nullptr";
            }
            std::ostringstream oss;
            // Manually format the pointer - ostream::operator<<(void*) falls back to %p which
            // is implementation-defined. MSVC prints pointers without the leading "0x" which
            // messes up the highlighter.
            oss<<std::showbase<<std::hex<<uintptr_t(value);
            return std::move(oss).str();
        }
    }
}
