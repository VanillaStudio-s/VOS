#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "drivers/keyboardlang/keylang.h"

/* ---- Tastatur-Eingabe-Puffer ---- */
void push_key_event(unsigned char sc);
int  get_key_event(unsigned char* sc);

/* ---- Tastatur-Layout (physische Belegung: US, DE, FR, ...) ---- */
extern int current_layout;

void keyboard_lang_selector(int lang);
void show_keyboard_types(void);
char get_ascii_char(unsigned char sc, int shift);

int         keyboard_get_layout_count(void);
const char* keyboard_get_layout_name(int idx);

/* ---- System-Sprache (Oberflaechentexte: EN/DE/...) ---- */
extern int current_system_lang;
void language_selector(const char* args);

/* ---- Sondertasten (Escape, F1-F12, Pfeiltasten, ...) ----
 * Diese Tasten liefern keinen ASCII-Wert und sind unabhaengig vom
 * Tastatur-Layout, da ihre Scancodes Hardware-Standard sind. */
typedef enum {
    KEY_NONE = 0,
    KEY_ESC,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_TAB,
    KEY_CAPSLOCK,
    KEY_LCTRL,
    KEY_LALT,
    KEY_LSHIFT,
    KEY_RSHIFT,
    KEY_NUMLOCK,
    KEY_SCROLLLOCK,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN,
    KEY_INSERT, KEY_DELETE
} special_key_t;

special_key_t  get_special_key(unsigned char sc);
int            is_key_release(unsigned char sc);
unsigned char  key_make_code(unsigned char sc);

#endif