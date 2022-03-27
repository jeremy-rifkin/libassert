#include <regex>
#include <stdio.h>
#include <string>
#include <string_view>
#include <vector>

inline std::vector<std::string> split(std::string_view s, std::string_view delim) {
    std::vector<std::string> vec;
    size_t old_pos = 0;
    size_t pos = 0;
    std::string token;
    while((pos = s.find(delim, old_pos)) != std::string::npos) {
            token = s.substr(old_pos, pos - old_pos);
            vec.push_back(token);
            old_pos = pos + delim.length();
    }
    //if(old_pos != s.length()) { // TODO: verify condition?
        vec.push_back(std::string(s.substr(old_pos)));
    //}
    return vec;
}

// https://stackoverflow.com/a/25385766/15675011
// TODO: template
constexpr const char * const ws = " \t\n\r\f\v";

// trim from end of string (right)
inline std::string& rtrim(std::string& s, const char* t = ws) {
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from beginning of string (left)
inline std::string& ltrim(std::string& s, const char* t = ws) {
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from both ends of string (right then left)
inline std::string& trim(std::string& s, const char* t = ws) {
    return ltrim(rtrim(s, t), t);
}

template<typename... T> std::string stringf(T... args) {
    size_t length = snprintf(0, 0, args...);
    if(length < 0) abort();
    std::string str(length, 0);
    snprintf(str.data(), length + 1, args...);
    return str;
}

#define let auto

int main() {
    std::string match_raw = R"QQ(
        0b0
        0B0
        0b1'10101010'0'1
        0
        0771237
        0'7'7'1'237
        120958701982375086125098123650981237409871234
        1'1234'234'2'2
        0X11
        0x1ff0f
        0x1'f'f'0f
        0x1aA
        1.5
        1.5'5
        1.
        1.f
        1.e2
        1.5E1
        1.5E-1
        1.5E+1
        1.5E1L
        0x1f.aP2
        0x1f.aP+2f
        0x1f.aP-2
        0x1p2
        1e2
        1e2f
    )QQ";
    std::string dont_match_raw = R"QQ(
        0B
        0b'1'1'0'1
        0b1'1'0'1'
        0b1'1''0'1
        '0
        0'
        078
        0''7'7'1'237
        1234'2'2'2'''1
        '1
        1'
        0X
        0xabcq
        0x'a'bcf
        0xa''bcf
        0xa'bcf'
        something
        foo.bar()
        1+2
        template<typename C>
        1 5
        1.'5
        '1.5
        1'.5
        1.5'
        1.5E1a
        1.5E-
        1.5'E+1
        1.5E1'L
        0x1f.ae2
        0x1f.a+2f
        0x1f.aP-2a0
        0x1f.a'P2
        0x1f.aP'2f
        0x1f.aP-2'
        0x1p'2
        1'e2
        1'e2f
        0x'1p2
        '1e2
        1e2'f
        0'x1p2
        1'e2
        1e2f'
    )QQ";
    let match_cases = split(trim(match_raw), "\n");
    let dont_match_cases = split(trim(dont_match_raw), "\n");
    for(let& set : {&match_cases, &dont_match_cases}) {
        for(let& item : *set) {
            item = trim(item);
        }
    }
    static std::string optional_integer_suffix = "(?:[Uu](?:LL?|ll?|Z|z)?|(?:LL?|ll?|Z|z)[Uu]?)?";
    static std::regex int_binary  = std::regex("^0[Bb][01](?:'?[01])*" + optional_integer_suffix + "$");
    static std::regex int_octal   = std::regex("^0(?:'?[0-7])+" + optional_integer_suffix + "$");
    static std::regex int_decimal = std::regex("^(?:0|[1-9](?:'?\\d)*)" + optional_integer_suffix + "$");
    static std::regex int_hex     = std::regex("^0[Xx](?!')(?:'?[\\da-fA-F])+" + optional_integer_suffix + "$");

    static std::string digit_sequence = "\\d(?:'?\\d)*";
    static std::string fractional_constant = stringf("(?:(?:%s)?\\.%s|%s\\.)", digit_sequence.c_str(), digit_sequence.c_str(), digit_sequence.c_str());
    static std::string exponent_part = "(?:[Ee][\\+-]?" + digit_sequence + ")";
    static std::string suffix = "[FfLl]";
    static std::regex float_decimal = std::regex(stringf("^(?:%s%s?|%s%s)%s?$",
                                    fractional_constant.c_str(), exponent_part.c_str(),
                                    digit_sequence.c_str(), exponent_part.c_str(), suffix.c_str()));
    static std::string hex_digit_sequence = "[\\da-fA-F](?:'?[\\da-fA-F])*";
    static std::string hex_frac_const = stringf("(?:(?:%s)?\\.%s|%s\\.)", hex_digit_sequence.c_str(), hex_digit_sequence.c_str(), hex_digit_sequence.c_str());
    static std::string binary_exp = "[Pp][\\+-]?" + digit_sequence;
    static std::regex float_hex = std::regex(stringf("^0[Xx](?:%s|%s)%s%s?$",
                                    hex_frac_const.c_str(), hex_digit_sequence.c_str(), binary_exp.c_str(), suffix.c_str()));
    let matches_any = [&](std::string str) {
        let matches = [](std::string value, std::regex re) {
            std::smatch base_match;
            return std::regex_match(value, base_match, re);
        };
        return matches(str, int_binary)
            || matches(str, int_octal)
            || matches(str, int_decimal)
            || matches(str, int_hex)
            || matches(str, float_decimal)
            || matches(str, float_hex);
    };
    bool ok = true;
    for(let const& item : match_cases) {
        if(!matches_any(item)) {
            printf("test case failed, valid literal not matched: %s\n", item.c_str());
            ok = false;
        }
    }
    for(let const& item : dont_match_cases) {
        if(matches_any(item)) {
            printf("test case failed, invalid literal matched: %s\n", item.c_str());
            ok = false;
        }
    }
    printf("Done\n");
    return !ok;
}
