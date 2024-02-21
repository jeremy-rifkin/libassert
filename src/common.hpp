#ifndef COMMON_HPP
#define COMMON_HPP

#define ESC "\033["
#define ANSIRGB(r, g, b) ESC "38;2;" #r ";" #g ";" #b "m"
#define RESET ESC "0m"
// Slightly modified one dark pro colors
// Original: https://coolors.co/e06b74-d19a66-e5c07a-98c379-62aeef-55b6c2-c678dd
// Modified: https://coolors.co/e06b74-d19a66-e5c07a-90cc66-62aeef-56c2c0-c678dd
#define RGB_RED    ANSIRGB(224, 107, 116)
#define RGB_ORANGE ANSIRGB(209, 154, 102)
#define RGB_YELLOW ANSIRGB(229, 192, 122)
#define RGB_GREEN  ANSIRGB(150, 205, 112) // modified
#define RGB_BLUE   ANSIRGB(98,  174, 239)
#define RGB_CYAN   ANSIRGB(86,  194, 192) // modified
#define RGB_PURPL  ANSIRGB(198, 120, 221)

#define BASIC_RED    ESC "31m"
#define BASIC_ORANGE ESC "33m" // using yellow as orange
#define BASIC_YELLOW ESC "34m" // based off use (identifiers in scope res) it's better to color as blue here
#define BASIC_GREEN  ESC "32m"
#define BASIC_BLUE   ESC "34m"
#define BASIC_CYAN   ESC "36m"
#define BASIC_PURPL  ESC "35m"

#define IS_WINDOWS 0

#if defined(_WIN32)
 #undef IS_WINDOWS
 #define IS_WINDOWS 1
#endif

#endif
