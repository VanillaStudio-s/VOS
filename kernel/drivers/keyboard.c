#include "kernel.h"
#include "drivers/screen.h"
#include "drivers/keyboard.h"

int shift_pressed       = 0;
int current_layout      = 0;   /* Index in layouts[] - siehe unten */
int current_system_lang = 0;

#define QUEUE_SIZE 32
static unsigned char key_queue[QUEUE_SIZE];
static int head = 0, tail = 0;

void push_key_event(unsigned char sc) {
    key_queue[tail] = sc;
    tail = (tail + 1) % QUEUE_SIZE;
}

int get_key_event(unsigned char* sc) {
    if (head == tail) return 0;
    *sc = key_queue[head];
    head = (head + 1) % QUEUE_SIZE;
    return 1;
}

/* ===================== Tastatur-Layouts ===================== *
 * Jede Sprache lebt in drivers/keyboardlang/keylang_xx.c.
 * Neue Sprache hinzufuegen: Datei anlegen, in keylang.h
 * deklarieren und hier ins Array eintragen - der Rest
 * (Auswahl, Anzeige) passt sich automatisch an.            */
static const keyboard_layout_t* layouts[] = {
    &keylang_en,
    &keylang_de,
    &keylang_fr,
};
#define LAYOUT_COUNT ((int)(sizeof(layouts) / sizeof(layouts[0])))

void keyboard_lang_selector(int lang) {
    if (lang >= 0 && lang < LAYOUT_COUNT) current_layout = lang;
}

int keyboard_get_layout_count(void) {
    return LAYOUT_COUNT;
}

const char* keyboard_get_layout_name(int idx) {
    if (idx < 0 || idx >= LAYOUT_COUNT) return "";
    return layouts[idx]->name;
}

/* Baut eine Zeile wie " -> [0] US-English (QWERTY) [ACTIVE]\n"
 * ohne printf/snprintf, da Freestanding-Umgebung. */
static void build_layout_line(char* buf, int active, int idx, const char* name) {
    int p = 0;
    if (active) { buf[p++] = ' '; buf[p++] = '-'; buf[p++] = '>'; buf[p++] = ' '; }
    else        { buf[p++] = ' '; buf[p++] = ' '; buf[p++] = ' '; buf[p++] = ' '; }
    buf[p++] = '[';
    buf[p++] = (char)('0' + idx);  /* reicht fuer bis zu 10 Layouts (0-9) */
    buf[p++] = ']';
    buf[p++] = ' ';
    int i = 0;
    while (name[i]) buf[p++] = name[i++];
    if (active) {
        const char* suf = " [ACTIVE]";
        int j = 0;
        while (suf[j]) buf[p++] = suf[j++];
    }
    buf[p++] = '\n';
    buf[p]   = 0;
}

void show_keyboard_types(void) {
    my_puts("Available keyboard types:\n");
    char line[96];
    for (int i = 0; i < LAYOUT_COUNT; i++) {
        int active = (i == current_layout);
        build_layout_line(line, active, i, layouts[i]->name);
        if (active) my_puts_color(line, 0x0A);
        else        my_puts(line);
    }
}

char get_ascii_char(unsigned char sc, int sh) {
    if (current_layout < 0 || current_layout >= LAYOUT_COUNT) return 0;
    return layouts[current_layout]->get_ascii(sc, sh);
}

/* ===================== System-Sprache (UI-Texte) ===================== */

void language_selector(const char* args) {
    if (!args[0]) {
        my_puts("System languages:\n");
        if (!current_system_lang) {
            my_puts_color(" -> [0] English [ACTIVE]\n", 0x0A);
            my_puts("    [1] Deutsch\n");
        } else {
            my_puts("    [0] English\n");
            my_puts_color(" -> [1] Deutsch [ACTIVE]\n", 0x0A);
        }
        return;
    }
    if (args[0] == '0') { current_system_lang = 0; my_puts_color("Language: English\n", 0x0A); }
    else if (args[0] == '1') { current_system_lang = 1; my_puts_color("Sprache: Deutsch\n", 0x0A); }
    else my_puts_color("Invalid language ID\n", 0x0C);
}

/* ===================== Sondertasten ===================== *
 * Escape, F1-F12, Tab, Pfeiltasten usw. liefern keinen ASCII-
 * Wert. Sie sind unabhaengig vom Tastatur-Layout, da Hardware-
 * Scancodes (Set 1) immer gleich sind - nur die Beschriftung
 * der Buchstaben unterscheidet sich je Sprache.            */

int is_key_release(unsigned char sc) {
    return (sc & 0x80) != 0;
}

unsigned char key_make_code(unsigned char sc) {
    return (unsigned char)(sc & 0x7F);
}

special_key_t get_special_key(unsigned char sc) {
    sc = key_make_code(sc);
    switch (sc) {
        case 0x01: return KEY_ESC;
        case 0x0F: return KEY_TAB;
        case 0x3A: return KEY_CAPSLOCK;
        case 0x1D: return KEY_LCTRL;
        case 0x38: return KEY_LALT;
        case 0x2A: return KEY_LSHIFT;
        case 0x36: return KEY_RSHIFT;
        case 0x45: return KEY_NUMLOCK;
        case 0x46: return KEY_SCROLLLOCK;
        case 0x3B: return KEY_F1;  case 0x3C: return KEY_F2;
        case 0x3D: return KEY_F3;  case 0x3E: return KEY_F4;
        case 0x3F: return KEY_F5;  case 0x40: return KEY_F6;
        case 0x41: return KEY_F7;  case 0x42: return KEY_F8;
        case 0x43: return KEY_F9;  case 0x44: return KEY_F10;
        case 0x57: return KEY_F11; case 0x58: return KEY_F12;
        /* Numpad ohne NumLock = Pfeil-/Navigationstasten.
         * Hinweis: die "echten" Pfeil-/Nav-Tasten ausserhalb des
         * Ziffernblocks senden zusaetzlich ein 0xE0-Praefix-Byte.
         * Das wird hier NICHT ausgewertet, da push_key_event nur
         * ein Byte pro Aufruf entgegennimmt - dafuer muesste die
         * ISR zwei Bytes erkennen und z.B. zusammenfassen. */
        case 0x48: return KEY_UP;
        case 0x50: return KEY_DOWN;
        case 0x4B: return KEY_LEFT;
        case 0x4D: return KEY_RIGHT;
        case 0x47: return KEY_HOME;
        case 0x4F: return KEY_END;
        case 0x49: return KEY_PGUP;
        case 0x51: return KEY_PGDN;
        case 0x52: return KEY_INSERT;
        case 0x53: return KEY_DELETE;
        default: return KEY_NONE;
    }
}