#ifndef COMMON_HPP
#define COMMON_HPP

#define ESC "\033["
#define ANSIRGB(r, g, b) ESC "38;2;" #r ";" #g ";" #b "m"
#define RESET ESC "0m"
// Slightly modified one dark pro colors
// Original: https://coolors.co/e06b74-d19a66-e5c07a-98c379-62aeef-55b6c2-c678dd
// Modified: https://coolors.co/e06b74-d19a66-e5c07a-90cc66-62aeef-56c2c0-c678dd
#define RED    ANSIRGB(224, 107, 116)
#define ORANGE ANSIRGB(209, 154, 102)
#define YELLOW ANSIRGB(229, 192, 122)
#define GREEN  ANSIRGB(150, 205, 112) // modified
#define BLUE   ANSIRGB(98,  174, 239)
#define CYAN   ANSIRGB(86,  194, 192) // modified
#define PURPL  ANSIRGB(198, 120, 221)

// TODO
#define RED_ALT    ESC "31m"
#define ORANGE_ALT ESC "33m" // using yellow as orange
#define YELLOW_ALT ESC "34m" // based off use (identifiers in scope res) it's better to color as blue here
#define GREEN_ALT  ESC "32m"
#define BLUE_ALT   ESC "34m"
#define CYAN_ALT   ESC "36m"
#define PURPL_ALT  ESC "35m"

#endif
