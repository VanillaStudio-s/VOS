#include "kernel.h"
#include "drivers/screen.h"
#include "gfx/gfx.h"
#include "sys/user.h"
#include "sys/system.h"
#include "sys/shutdown.h"
#include "gui/gui.h"
#include "gui/theme.h"
#include "gui/startmenu.h"
#include "gui/apps.h"

/* ── App registry ────────────────────────────────────────────────────────── */
typedef struct {
    char      name[32];
    uint32_t  color;
    SMOpenFn  fn;
} SMApp;

static SMApp _apps[SM_MAX_APPS];
static int   _app_cnt  = 0;

/* ── UI state ────────────────────────────────────────────────────────────── */
static int  _open       = 0;
static int  _tile       = 0;     /* 0=list  1=tile  */
static int  _power_sub  = 0;

static char _search[24];
static int  _slen = 0;

/* ── Layout constants (matches START_MENU_W / START_MENU_H from gui.h) ──── */
#define SMW   220
#define SMH   340
#define SM_MX  4
#define SM_MY  (SCREEN_HEIGHT - TASKBAR_HEIGHT - SMH)

/* app area: below header (28) + search (28) + sep (6) + above bottom bar (38) */
#define SM_HEADER_H  28
#define SM_SEARCH_H  28
#define SM_SEP_H      6
#define SM_BOT_H     38
#define SM_APP_Y     (SM_MY + SM_HEADER_H + SM_SEARCH_H + SM_SEP_H)
#define SM_APP_H     (SMH - SM_HEADER_H - SM_SEARCH_H - SM_SEP_H - SM_BOT_H)

/* tile constants */
#define TILE_SIZE   60
#define TILE_GAP     8
#define TILE_COLS    3
#define TILE_ROW_H  (TILE_SIZE + 14)   /* tile + name label */

/* ── Helpers ─────────────────────────────────────────────────────────────── */
static int _tolower(char c) { return (c>='A'&&c<='Z') ? c+32 : c; }

static int _contains(const char* hay, const char* needle) {
    if (!needle[0]) return 1;
    for (int i = 0; hay[i]; i++) {
        int j = 0;
        while (needle[j] && hay[i+j] && _tolower(hay[i+j]) == _tolower(needle[j])) j++;
        if (!needle[j]) return 1;
    }
    return 0;
}

static void _str8(const char* src, char* dst) {
    int i = 0;
    while (src[i] && i < 8) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void sm_register(const char* name, unsigned int color, SMOpenFn fn) {
    if (_app_cnt >= SM_MAX_APPS) return;
    int k = 0;
    while (name[k] && k < 31) { _apps[_app_cnt].name[k] = name[k]; k++; }
    _apps[_app_cnt].name[k] = '\0';
    _apps[_app_cnt].color   = color;
    _apps[_app_cnt].fn      = fn;
    _app_cnt++;
}

/* ── Wrappers for apps that take parameters ──────────────────────────────── */
static void _open_texteditor(void) { app_texteditor_open((void*)0); }

/* ── Built-in app registration ───────────────────────────────────────────── */
/*  To add a new app: add one sm_register() line here.  That's it.           */
void sm_init(void) {
    _app_cnt = 0;
    _open    = 0;
    _tile    = 0;
    _power_sub = 0;
    _slen    = 0;
    _search[0] = '\0';

    sm_register("Settings",       RGB(  0,120,215), app_settings_open);
    sm_register("Calculator",     RGB(  0,160,100), app_calculator_open);
    sm_register("System Monitor", RGB(160, 80,  0), app_sysmon_open);
    sm_register("Text Editor",    RGB( 80,  0,160), _open_texteditor);
    sm_register("Terminal",       RGB( 30, 30, 30), app_terminal_open);
    sm_register("Files",          RGB(  0,100,180), app_vosdolp_open);
    /* ── Add new apps here: ─────────────────────────────────────────────── */
    /* sm_register("My App", RGB(r,g,b), app_myapp_open);                    */
}

void sm_open(void)    { _open = 1; _power_sub = 0; }
void sm_close(void)   { _open = 0; _slen = 0; _search[0] = '\0'; _power_sub = 0; }
int  sm_is_open(void) { return _open; }

void sm_set_tile_mode(int tile) { _tile = tile ? 1 : 0; }
int  sm_get_tile_mode(void)     { return _tile; }

/* ── Rendering ───────────────────────────────────────────────────────────── */

static void _draw_list(void) {
    int ay = SM_APP_Y;
    int shown = 0;
    for (int i = 0; i < _app_cnt; i++) {
        if (_slen > 0 && !_contains(_apps[i].name, _search)) continue;
        int iy = ay + shown * 28;
        if (iy + 28 > SM_MY + SMH - SM_BOT_H) break;

        /* colored dot */
        gfx_fill_circle(SM_MX + 18, iy + 14, 6, _apps[i].color);
        draw_string(SM_MX + 30, iy + 8, _apps[i].name, g_theme.text_primary, COLOR_TRANSPARENT);
        shown++;
    }
    if (shown == 0)
        draw_string(SM_MX + 14, ay + 8, "No results", RGB(65,70,100), COLOR_TRANSPARENT);
}

static void _draw_tiles(void) {
    int ax = SM_MX + TILE_GAP;
    int ay = SM_APP_Y + 4;
    int col = 0, row = 0;
    int col_w = (SMW - TILE_GAP * (TILE_COLS + 1)) / TILE_COLS;

    for (int i = 0; i < _app_cnt; i++) {
        if (_slen > 0 && !_contains(_apps[i].name, _search)) continue;

        int tx = ax + col * (col_w + TILE_GAP);
        int ty = ay + row * TILE_ROW_H;
        if (ty + TILE_ROW_H > SM_MY + SMH - SM_BOT_H) break;

        /* tile background */
        gfx_fill_rounded_rect(tx, ty, TILE_SIZE, TILE_SIZE, 6, _apps[i].color);

        /* first letter, large and centered */
        char lc[2] = { _apps[i].name[0], '\0' };
        draw_string(tx + TILE_SIZE/2 - 3, ty + TILE_SIZE/2 - 7, lc, COLOR_WHITE, COLOR_TRANSPARENT);

        /* short name below tile */
        char sn[9]; _str8(_apps[i].name, sn);
        draw_string(tx + 2, ty + TILE_SIZE + 2, sn, g_theme.text_secondary, COLOR_TRANSPARENT);

        col++;
        if (col >= TILE_COLS) { col = 0; row++; }
    }
}

void sm_render(void) {
    if (!_open) return;
    int mx = SM_MX, my = SM_MY;

    /* shadow + body */
    draw_rect(mx+4, my+4, SMW, SMH, RGB(4,4,8));
    draw_rect(mx, my, SMW, SMH, RGB(22,24,36));
    draw_rect_outline(mx, my, SMW, SMH, RGB(65,75,115));

    /* header gradient */
    gfx_gradient_rect(mx, my, SMW, SM_HEADER_H, g_theme.accent, g_theme.win_title_active, 1);
    draw_string(mx+10, my+8, "VOS", g_theme.text_primary, COLOR_TRANSPARENT);

    /* list/tile toggle button in header */
    const char* mode_lbl = _tile ? "List" : "Tile";
    int toggle_x = mx + SMW - 34;
    gfx_fill_rounded_rect(toggle_x, my+5, 28, 18, 3, RGB(0,0,0));
    draw_rect_outline(toggle_x, my+5, 28, 18, RGB(180,200,255));
    draw_string(toggle_x+4, my+8, mode_lbl, COLOR_WHITE, COLOR_TRANSPARENT);

    /* search bar */
    gfx_fill_rounded_rect(mx+8, my+SM_HEADER_H+4, SMW-16, 22, 3, RGB(14,16,26));
    draw_rect_outline(mx+8, my+SM_HEADER_H+4, SMW-16, 22, RGB(75,85,125));
    if (_slen == 0)
        draw_string(mx+14, my+SM_HEADER_H+9, "Search...", RGB(65,70,100), COLOR_TRANSPARENT);
    else
        draw_string(mx+14, my+SM_HEADER_H+9, _search, g_theme.text_primary, COLOR_TRANSPARENT);

    /* separator */
    draw_line(mx+6, SM_APP_Y-2, mx+SMW-6, SM_APP_Y-2, RGB(42,48,72));

    /* app area */
    if (_power_sub) {
        /* power submenu */
        const char* lbl[3] = {"  Restart","  Shutdown","  Logout"};
        uint32_t    clr[3] = {RGB(40,90,180), RGB(160,40,40), RGB(60,70,100)};
        for (int i = 0; i < 3; i++) {
            gfx_fill_rounded_rect(mx+8, SM_APP_Y+i*44, SMW-16, 36, 4, clr[i]);
            draw_string(mx+14, SM_APP_Y+i*44+12, lbl[i], COLOR_WHITE, COLOR_TRANSPARENT);
        }
    } else if (_tile) {
        _draw_tiles();
    } else {
        _draw_list();
    }

    /* bottom separator */
    int bar_y = my + SMH - SM_BOT_H;
    draw_line(mx+6, bar_y, mx+SMW-6, bar_y, RGB(42,48,72));

    /* bottom bar: user avatar + name + power button */
    gfx_fill_circle(mx+20, bar_y+19, 12, g_theme.accent);
    char ui[2] = { user_database[active_user_idx][0], '\0' };
    if (ui[0] >= 'a' && ui[0] <= 'z') ui[0] -= 32;
    draw_string(mx+16, bar_y+12, ui, COLOR_WHITE, COLOR_TRANSPARENT);
    draw_string(mx+36, bar_y+13, user_database[active_user_idx],
                g_theme.text_secondary, COLOR_TRANSPARENT);

    /* power button */
    int pw_cx = mx + SMW - 22, pw_cy = bar_y + 19;
    uint32_t pw_bg = _power_sub ? RGB(160,40,40) : RGB(48,54,82);
    gfx_fill_circle(pw_cx, pw_cy, 14, pw_bg);
    gfx_draw_circle(pw_cx, pw_cy, 9, RGB(190,205,240));
    draw_rect(pw_cx-1, pw_cy-14, 3, 7, pw_bg);
    draw_line(pw_cx, pw_cy-14, pw_cx, pw_cy-5, RGB(190,205,240));
}

/* ── Click handling ──────────────────────────────────────────────────────── */

int sm_click(int sx, int sy) {
    if (!_open) return 0;
    int mx = SM_MX, my = SM_MY;

    /* outside: close */
    if (sx < mx || sx > mx+SMW || sy < my || sy > my+SMH) {
        sm_close(); gui_render(); return 1;
    }

    int bar_y = my + SMH - SM_BOT_H;

    /* power button */
    int pw_cx = mx + SMW - 22;
    if (sx >= pw_cx-15 && sx <= pw_cx+15 && sy >= bar_y) {
        _power_sub = !_power_sub; gui_render(); return 1;
    }

    /* header: list/tile toggle */
    int toggle_x = mx + SMW - 34;
    if (sy >= my+5 && sy <= my+23 && sx >= toggle_x && sx <= toggle_x+28) {
        _tile = !_tile; gui_render(); return 1;
    }

    /* power submenu */
    if (_power_sub) {
        for (int i = 0; i < 3; i++) {
            if (sy >= SM_APP_Y+i*44 && sy < SM_APP_Y+i*44+36) {
                sm_close();
                if      (i == 0) sys_restart();
                else if (i == 1) sys_shutdown();
                else             { sys_logout(); gui_render(); }
                return 1;
            }
        }
        return 1;
    }

    /* list mode app click */
    if (!_tile) {
        int shown = 0;
        for (int i = 0; i < _app_cnt; i++) {
            if (_slen > 0 && !_contains(_apps[i].name, _search)) continue;
            int iy = SM_APP_Y + shown * 28;
            if (iy + 28 > my + SMH - SM_BOT_H) break;
            if (sy >= iy && sy < iy+28) {
                sm_close();
                if (_apps[i].fn) _apps[i].fn();
                gui_render();
                return 1;
            }
            shown++;
        }
    }

    /* tile mode app click */
    if (_tile) {
        int ax = SM_MX + TILE_GAP;
        int ay = SM_APP_Y + 4;
        int col_w = (SMW - TILE_GAP * (TILE_COLS + 1)) / TILE_COLS;
        int col = 0, row = 0;
        for (int i = 0; i < _app_cnt; i++) {
            if (_slen > 0 && !_contains(_apps[i].name, _search)) continue;
            int tx = ax + col * (col_w + TILE_GAP);
            int ty = ay + row * TILE_ROW_H;
            if (ty + TILE_ROW_H > my + SMH - SM_BOT_H) break;
            if (sx >= tx && sx < tx+TILE_SIZE && sy >= ty && sy < ty+TILE_SIZE) {
                sm_close();
                if (_apps[i].fn) _apps[i].fn();
                gui_render();
                return 1;
            }
            col++;
            if (col >= TILE_COLS) { col = 0; row++; }
        }
    }

    return 1;  /* click inside menu but no app hit — still consumed */
}

/* ── Key handler (search) ────────────────────────────────────────────────── */
void sm_key(char c) {
    if (c == 27)  { sm_close(); return; }
    if (c == '\b') { if (_slen > 0) _search[--_slen] = '\0'; return; }
    if (c >= ' ' && c < 127 && _slen < 23) {
        _search[_slen++] = c;
        _search[_slen]   = '\0';
    }
}