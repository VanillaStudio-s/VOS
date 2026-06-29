#ifndef KEYLANG_H
#define KEYLANG_H

/* Gemeinsame Schnittstelle fuer alle Tastatur-Layouts (physische
 * Tastenbelegung). Jede Sprache lebt in einer eigenen Datei
 * (keylang_de.c, keylang_en.c, ...) und stellt ein
 * keyboard_layout_t-Objekt bereit, das hier nur deklariert wird.
 *
 * Neue Sprache hinzufuegen:
 *   1. drivers/keyboardlang/keylang_xx.c anlegen (Vorlage: keylang_en.c)
 *   2. darin "const keyboard_layout_t keylang_xx = { ... };" definieren
 *   3. hier die extern-Deklaration eintragen
 *   4. in drivers/keyboard.c im Array "layouts[]" eintragen
 *   -> show_keyboard_types() und keyboard_lang_selector() passen sich
 *      automatisch an, da sie ueber das Array iterieren.
 */

typedef struct {
    const char* name;        /* Anzeigename, z.B. "US-English (QWERTY)" */
    const char* short_name;  /* Kurzform, z.B. "EN" */
    char (*get_ascii)(unsigned char sc, int shift);
} keyboard_layout_t;

extern const keyboard_layout_t keylang_en;
extern const keyboard_layout_t keylang_de;
extern const keyboard_layout_t keylang_fr;

#endif