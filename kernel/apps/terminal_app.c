#include "kernel.h"
#include "drivers/screen.h"
#include "gfx/gfx.h"
#include "gui/gui.h"
#include "gui/theme.h"
#include "gui/window.h"
#include "gui/notify.h"
#include "mem/heap.h"
#include "fs/fs.h"
#include "sys/user.h"
#include "sys/system.h"
#include "sys/clipboard.h"
#include "apps/texteditor.h"
#include "apps/terminal_app.h"
#include "shell/shell.h"

static int ta_id = -1;
static int ta_input_start = 0;

#define TA_LINES  22
#define TA_COLS   62
#define TA_FW      8
#define TA_FH     14

static char ta_buf[TA_LINES][TA_COLS + 1];
static int  ta_cur_line;
static int  ta_cur_col;

/* ── Output-Buffer: eine Zeile scrollen ───────────────── */
static void _ta_newline(void) {
    ta_cur_line++; ta_cur_col = 0;
    if (ta_cur_line >= TA_LINES) {
        for (int i = 0; i < TA_LINES - 1; i++) {
            int j = 0;
            while (ta_buf[i + 1][j]) { ta_buf[i][j] = ta_buf[i + 1][j]; j++; }
            ta_buf[i][j] = '\0';
        }
        ta_buf[TA_LINES - 1][0] = '\0';
        ta_cur_line = TA_LINES - 1;
    }
    ta_buf[ta_cur_line][0] = '\0';
}

/* ── Screen löschen ───────────────────────────────────── */
static void _ta_clear(void) {
    for (int i = 0; i < TA_LINES; i++) ta_buf[i][0] = '\0';
    ta_cur_line = 0;
    ta_cur_col  = 0;
}

/* ── Öffentliche Ausgabefunktion (wird als output_func übergeben) ── */
void _ta_puts(const char* s) {
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\n') { _ta_newline(); continue; }
        if (ta_cur_col < TA_COLS) {
            ta_buf[ta_cur_line][ta_cur_col++] = s[i];
            ta_buf[ta_cur_line][ta_cur_col]   = '\0';
        }
    }
}

/* ── Prompt zeichnen ──────────────────────────────────── */
static void _ta_prompt(void) {
    char pbuf[80];
    fs_pwd(pbuf, 80);
    _ta_puts(pbuf);
    _ta_puts(" > ");
    ta_input_start = ta_cur_col;   /* Eingabe-Startposition sperren */
}

/* ── Zeichenhilfen ────────────────────────────────────── */
static int _ta_sw(const char* s, const char* p) {
    for (int i = 0; p[i]; i++) if (s[i] != p[i]) return 0;
    return 1;
}

static char* _ta_find_prompt(char* s) {
    for (int i = 0; s[i]; i++)
        if (s[i] == ' ' && s[i + 1] == '>' && s[i + 2] == ' ') return &s[i];
    return (char*)0;
}

static void _ta_copy_n(char* dst, char* src, int n) {
    for (int i = 0; i < n; i++) dst[i] = src[i];
    dst[n] = '\0';
}

/* ── Kommando ausführen ───────────────────────────────── */
static void _ta_exec(const char* cmd) {
    /* "clear" im Terminal: Buffer leeren statt VGA-Screen */
    if (my_strcmp(cmd, "clear") == 0) {
        _ta_clear();
        return;
    }
    /* Alle anderen Kommandos: Shell-Logik mit _ta_puts als Ausgabe-Callback */
    process_command(cmd, _ta_puts);
}

/* ── Inhalt zeichnen ──────────────────────────────────── */
static void _draw(int x, int y, int w, int h) {
    draw_rect(x, y, w, h, RGB(10, 12, 20));

    for (int i = 0; i < TA_LINES; i++) {
        char* line = ta_buf[i];
        char* sep  = _ta_find_prompt(line);

        if (sep) {
            int  path_len = sep - line;
            char path[80];
            _ta_copy_n(path, line, path_len);

            /* Pfad in Akzentfarbe (grün) */
            draw_string(x + 4, y + 2 + i * TA_FH,
                        path, COLOR_PROMPT, COLOR_TRANSPARENT);

            /* " > " in hellem Grau */
            draw_string(x + 4 + path_len * TA_FW, y + 2 + i * TA_FH,
                        " > ", RGB(200, 200, 200), COLOR_TRANSPARENT);

            /* Eingabe in Weiß */
            draw_string(x + 4 + (path_len + 3) * TA_FW, y + 2 + i * TA_FH,
                        sep + 3, COLOR_INPUT, COLOR_TRANSPARENT);
        } else {
            /* Normale Shell-Ausgabe in hellem Grün */
            draw_string(x + 4, y + 2 + i * TA_FH,
                        line, RGB(180, 220, 180), COLOR_TRANSPARENT);
        }
    }

    /* Cursor */
    draw_rect(x + 4 + ta_cur_col * TA_FW,
              y + 2 + ta_cur_line * TA_FH,
              TA_FW, TA_FH - 2,
              g_theme.accent);
}

/* ── Tastatureingabe ──────────────────────────────────── */
static void _key(char c, unsigned char sc, int ctrl) {
    (void)ctrl; (void)sc;

    if (c == '\n') {
        /* Eingabe aus aktuellem Puffer lesen */
        char cmd[64];
        int  len = ta_cur_col - ta_input_start;
        for (int i = 0; i < len; i++)
            cmd[i] = ta_buf[ta_cur_line][ta_input_start + i];
        cmd[len] = '\0';

        _ta_puts("\n");
        _ta_exec(cmd);
        _ta_prompt();
        gui_render();
        return;
    }

    if (c == '\b') {
        if (ta_cur_col > ta_input_start) {
            ta_cur_col--;
            ta_buf[ta_cur_line][ta_cur_col] = '\0';
            gui_render();
        }
        return;
    }

    if (c >= ' ' && c < 127 && ta_cur_col < TA_COLS - 1) {
        ta_buf[ta_cur_line][ta_cur_col++] = c;
        ta_buf[ta_cur_line][ta_cur_col]   = '\0';
        gui_render();
    }
}

/* ── Fenster öffnen ───────────────────────────────────── */
void app_terminal_open(void) {
    if (win_is_open(ta_id)) { win_set_focus(ta_id); return; }

    _ta_clear();

    _ta_puts("VOS Terminal v0.4.2\n");
    _ta_prompt();

    int wh = 2 + (TA_LINES + 1) * TA_FH + 8;
    ta_id = win_create(30, 25, TA_COLS * TA_FW + 16, wh, "Terminal",
                       WIN_FLAG_DRAGGABLE | WIN_FLAG_CLOSABLE | WIN_FLAG_RESIZABLE);
    win_set_min_size(ta_id, 300, 180);
    win_set_content(ta_id, _draw);
    win_set_key(ta_id, _key);
}