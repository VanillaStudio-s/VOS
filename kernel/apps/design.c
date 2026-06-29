#include "kernel.h"
#include "gui/theme.h"
#include "drivers/mouse.h"
#include "apps/design.h"

/* forward-declared so we don't pull in the full startmenu header here */
extern void sm_set_tile_mode(int v);

/* ── Internal state ──────────────────────────────────────────────────────── */
static uint32_t _desktop_color  = 0;   /* 0 = use theme default */
static int      _win_rounded    = 1;
static int      _win_shadow     = 1;
static int      _win_border     = 1;
static int      _transparency   = 0;
static int      _mouse_theme    = 0;   /* MOUSE_THEME_VOS */
static int      _cursor_type    = 0;   /* CURSOR_ARROW    */
static int      _tile_mode      = 0;   /* list mode       */

/* ── Init ────────────────────────────────────────────────────────────────── */
void design_init(void) {
    _desktop_color = g_theme.desktop_bg;
    _win_rounded   = 1;
    _win_shadow    = 1;
    _win_border    = 1;
    _transparency  = 0;
    _mouse_theme   = MOUSE_THEME_VOS;
    _cursor_type   = CURSOR_ARROW;
    _tile_mode     = 0;
}

/* ── Accent ──────────────────────────────────────────────────────────────── */
void     design_set_accent(uint32_t rgb) { theme_set_accent(rgb); }
uint32_t design_get_accent(void)         { return g_theme.accent; }

/* ── Desktop background ──────────────────────────────────────────────────── */
void design_set_desktop_color(uint32_t rgb) {
    _desktop_color = rgb;
    g_theme.desktop_bg = rgb;
}
uint32_t design_get_desktop_color(void) { return _desktop_color; }

/* ── Window ──────────────────────────────────────────────────────────────── */
void design_set_win_rounded(int v)  { _win_rounded  = v ? 1 : 0; }
int  design_get_win_rounded(void)   { return _win_rounded;  }

void design_set_win_shadow(int v)   { _win_shadow   = v ? 1 : 0; }
int  design_get_win_shadow(void)    { return _win_shadow;   }

void design_set_win_border(int v)   { _win_border   = v ? 1 : 0; }
int  design_get_win_border(void)    { return _win_border;   }

void design_set_transparency(int v) {
    if (v < 0) v = 0;
    if (v > 4) v = 4;
    _transparency = v;
}
int  design_get_transparency(void)  { return _transparency; }

/* ── Mouse ───────────────────────────────────────────────────────────────── */
void design_set_mouse_theme(int theme) {
    _mouse_theme = theme;
    mouse_set_theme(theme);
}
int design_get_mouse_theme(void) { return _mouse_theme; }

void design_set_cursor_type(int type) {
    _cursor_type = type;
    mouse_set_cursor(type);
}
int design_get_cursor_type(void) { return _cursor_type; }

/* ── Start menu ──────────────────────────────────────────────────────────── */
void design_set_tile_mode(int v) {
    _tile_mode = v ? 1 : 0;
    sm_set_tile_mode(_tile_mode);
}
int design_get_tile_mode(void) { return _tile_mode; }