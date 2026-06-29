#include "keylang.h"

/* French (AZERTY) - vereinfachtes Layout.
 *
 * Besonderheit AZERTY: die Zahlenreihe liegt auf der Shift-Ebene,
 * unshifted liefert sie Symbole/Akzentbuchstaben.
 *
 * Tote Tasten (Zirkumflex/Trema auf 0x1A) liefern 0 zurueck, ebenso
 * das Paragraf-Zeichen "§", da es in CP437 nicht enthalten ist.
 * Akzentbuchstaben werden als CP437-Codes zurueckgegeben (siehe
 * keylang_de.c fuer den Hintergrund dazu).
 */

#define FR_E_ACUTE   0x82 /* é */
#define FR_E_GRAVE   0x8A /* è */
#define FR_C_CEDILLE 0x87 /* ç */
#define FR_A_GRAVE   0x85 /* à */
#define FR_U_GRAVE   0x97 /* ù */
#define FR_POUND     0x9C /* £ */
#define FR_DEG       0xF8 /* ° */
#define FR_MU        0xE6 /* µ */

static char fr_unshifted(unsigned char sc) {
    switch (sc) {
        /* Buchstaben: A/Q und Z/W vertauscht ggue. US, M auf ;-Position */
        case 0x10: return 'a'; case 0x11: return 'z'; case 0x1E: return 'q'; case 0x2C: return 'w';
        case 0x27: return 'm'; case 0x32: return ',';
        case 0x12: return 'e'; case 0x13: return 'r'; case 0x14: return 't'; case 0x15: return 'y';
        case 0x16: return 'u'; case 0x17: return 'i'; case 0x18: return 'o'; case 0x19: return 'p';
        case 0x1F: return 's'; case 0x20: return 'd'; case 0x21: return 'f'; case 0x22: return 'g';
        case 0x23: return 'h'; case 0x24: return 'j'; case 0x25: return 'k'; case 0x26: return 'l';
        case 0x2D: return 'x'; case 0x2E: return 'c'; case 0x2F: return 'v'; case 0x30: return 'b';
        case 0x31: return 'n';
        /* Zahlenreihe unshifted = Symbole/Akzente */
        case 0x02: return '&'; case 0x03: return (char)FR_E_ACUTE; case 0x04: return '"';
        case 0x05: return '\''; case 0x06: return '('; case 0x07: return '-';
        case 0x08: return (char)FR_E_GRAVE; case 0x09: return '_'; case 0x0A: return (char)FR_C_CEDILLE;
        case 0x0B: return (char)FR_A_GRAVE; case 0x0C: return ')'; case 0x0D: return '=';
        case 0x1A: return 0;               /* ^ - tote Taste */
        case 0x1B: return '$';
        case 0x28: return (char)FR_U_GRAVE;
        case 0x2B: return '*';
        case 0x33: return ';'; case 0x34: return ':'; case 0x35: return '!';
        case 0x39: return ' '; case 0x1C: return '\n'; case 0x0E: return '\b'; case 0x0F: return '\t';
        default: return 0;
    }
}

static char fr_shifted(unsigned char sc) {
    switch (sc) {
        case 0x10: return 'A'; case 0x11: return 'Z'; case 0x1E: return 'Q'; case 0x2C: return 'W';
        case 0x27: return 'M'; case 0x32: return '?';
        case 0x12: return 'E'; case 0x13: return 'R'; case 0x14: return 'T'; case 0x15: return 'Y';
        case 0x16: return 'U'; case 0x17: return 'I'; case 0x18: return 'O'; case 0x19: return 'P';
        case 0x1F: return 'S'; case 0x20: return 'D'; case 0x21: return 'F'; case 0x22: return 'G';
        case 0x23: return 'H'; case 0x24: return 'J'; case 0x25: return 'K'; case 0x26: return 'L';
        case 0x2D: return 'X'; case 0x2E: return 'C'; case 0x2F: return 'V'; case 0x30: return 'B';
        case 0x31: return 'N';
        /* Zahlenreihe shifted = die eigentlichen Ziffern */
        case 0x02: return '1'; case 0x03: return '2'; case 0x04: return '3'; case 0x05: return '4';
        case 0x06: return '5'; case 0x07: return '6'; case 0x08: return '7'; case 0x09: return '8';
        case 0x0A: return '9'; case 0x0B: return '0';
        case 0x0C: return (char)FR_DEG;
        case 0x0D: return '+';
        case 0x1A: return 0;               /* Trema - tote Taste */
        case 0x1B: return (char)FR_POUND;
        case 0x28: return '%';
        case 0x2B: return (char)FR_MU;
        case 0x33: return '.'; case 0x34: return '/';
        case 0x35: return 0;               /* § - in CP437 nicht enthalten */
        case 0x39: return ' '; case 0x1C: return '\n'; case 0x0E: return '\b'; case 0x0F: return '\t';
        default: return 0;
    }
}

static char fr_get_ascii(unsigned char sc, int shift) {
    return shift ? fr_shifted(sc) : fr_unshifted(sc);
}

const keyboard_layout_t keylang_fr = {
    .name = "French (AZERTY)",
    .short_name = "FR",
    .get_ascii = fr_get_ascii
};