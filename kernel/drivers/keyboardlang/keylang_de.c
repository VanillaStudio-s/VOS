#include "keylang.h"

/* DE-German (QWERTZ).
 *
 * Hinweis Umlaute: ae/oe/ue/ss werden als CP437-Codes zurueckgegeben
 * (Standard-Codepage im klassischen VGA-Textmodus). Falls dein Kernel
 * eine andere Codepage/Font nutzt, muessen diese Werte angepasst werden.
 *
 * Tote Tasten (Akut "´" auf 0x0D und Zirkumflex "^" auf 0x29) liefern
 * 0 zurueck, da sie technisch erst mit der naechsten Taste ein Zeichen
 * ergeben (z.B. ^ + e = ê) - das muesste eine eigene Kombinierlogik
 * im Eingabe-Handler uebernehmen, nicht das Layout selbst.
 *
 * Das Paragraf-Zeichen "§" (shift+3) ist in CP437 nicht enthalten und
 * wird daher ebenfalls als 0 zurueckgegeben.
 */

#define DE_AE   0x84 /* ä */
#define DE_OE   0x94 /* ö */
#define DE_UE   0x81 /* ü */
#define DE_SS   0xE1 /* ß */
#define DE_AE_U 0x8E /* Ä */
#define DE_OE_U 0x99 /* Ö */
#define DE_UE_U 0x9A /* Ü */
#define DE_DEG  0xF8 /* ° */

static char de_unshifted(unsigned char sc) {
    switch (sc) {
        case 0x1E: return 'a'; case 0x30: return 'b'; case 0x2E: return 'c'; case 0x20: return 'd';
        case 0x12: return 'e'; case 0x21: return 'f'; case 0x22: return 'g'; case 0x23: return 'h';
        case 0x17: return 'i'; case 0x24: return 'j'; case 0x25: return 'k'; case 0x26: return 'l';
        case 0x32: return 'm'; case 0x31: return 'n'; case 0x18: return 'o'; case 0x19: return 'p';
        case 0x10: return 'q'; case 0x13: return 'r'; case 0x1F: return 's'; case 0x14: return 't';
        case 0x16: return 'u'; case 0x2F: return 'v'; case 0x11: return 'w'; case 0x2D: return 'x';
        case 0x2C: return 'y'; case 0x15: return 'z'; /* Y/Z vertauscht ggue. US */
        case 0x02: return '1'; case 0x03: return '2'; case 0x04: return '3'; case 0x05: return '4';
        case 0x06: return '5'; case 0x07: return '6'; case 0x08: return '7'; case 0x09: return '8';
        case 0x0A: return '9'; case 0x0B: return '0';
        case 0x0C: return (char)DE_SS;     /* ß */
        case 0x0D: return 0;               /* ´ - tote Taste */
        case 0x1A: return (char)DE_UE;     /* ü */
        case 0x1B: return '+';
        case 0x27: return (char)DE_OE;     /* ö */
        case 0x28: return (char)DE_AE;     /* ä */
        case 0x29: return 0;               /* ^ - tote Taste */
        case 0x2B: return '#';
        case 0x33: return ',';
        case 0x34: return '.';
        case 0x35: return '-';
        case 0x39: return ' '; case 0x1C: return '\n'; case 0x0E: return '\b'; case 0x0F: return '\t';
        default: return 0;
    }
}

static char de_shifted(unsigned char sc) {
    switch (sc) {
        case 0x1E: return 'A'; case 0x30: return 'B'; case 0x2E: return 'C'; case 0x20: return 'D';
        case 0x12: return 'E'; case 0x21: return 'F'; case 0x22: return 'G'; case 0x23: return 'H';
        case 0x17: return 'I'; case 0x24: return 'J'; case 0x25: return 'K'; case 0x26: return 'L';
        case 0x32: return 'M'; case 0x31: return 'N'; case 0x18: return 'O'; case 0x19: return 'P';
        case 0x10: return 'Q'; case 0x13: return 'R'; case 0x1F: return 'S'; case 0x14: return 'T';
        case 0x16: return 'U'; case 0x2F: return 'V'; case 0x11: return 'W'; case 0x2D: return 'X';
        case 0x2C: return 'Y'; case 0x15: return 'Z';
        case 0x02: return '!'; case 0x03: return '"';
        case 0x04: return 0;               /* § - in CP437 nicht enthalten */
        case 0x05: return '$'; case 0x06: return '%'; case 0x07: return '&'; case 0x08: return '/';
        case 0x09: return '('; case 0x0A: return ')'; case 0x0B: return '=';
        case 0x0C: return '?';
        case 0x0D: return 0;               /* tote Taste */
        case 0x1A: return (char)DE_UE_U;   /* Ü */
        case 0x1B: return '*';
        case 0x27: return (char)DE_OE_U;   /* Ö */
        case 0x28: return (char)DE_AE_U;   /* Ä */
        case 0x29: return (char)DE_DEG;    /* ° */
        case 0x2B: return '\'';
        case 0x33: return ';';
        case 0x34: return ':';
        case 0x35: return '_';
        case 0x39: return ' '; case 0x1C: return '\n'; case 0x0E: return '\b'; case 0x0F: return '\t';
        default: return 0;
    }
}

static char de_get_ascii(unsigned char sc, int shift) {
    return shift ? de_shifted(sc) : de_unshifted(sc);
}

const keyboard_layout_t keylang_de = {
    .name = "DE-German (QWERTZ)",
    .short_name = "DE",
    .get_ascii = de_get_ascii
};