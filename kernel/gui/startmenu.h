#ifndef STARTMENU_H
#define STARTMENU_H

/* ── gui/startmenu.h ────────────────────────────────────────────────────────
 *
 *  Standalone start menu.
 *  To add a new app:  call sm_register() from anywhere before gui_init().
 *  Layout can be switched between list and tile at runtime.
 *
 * ─────────────────────────────────────────────────────────────────────────── */

#define SM_MAX_APPS 32

typedef void (*SMOpenFn)(void);

/* Register an app entry.  name ≤ 31 chars, color = tile bg, fn = opener. */
void sm_register(const char* name, unsigned int color, SMOpenFn fn);

/* Lifecycle */
void sm_init(void);    /* registers all built-in apps; call once from gui_init */
void sm_open(void);
void sm_close(void);
int  sm_is_open(void);

/* Rendering / input — called by gui.c */
void sm_render(void);
void sm_key(char c);
int  sm_click(int screen_x, int screen_y);  /* 1 = consumed */

/* Layout */
void sm_set_tile_mode(int tile);  /* 0 = list, 1 = tile */
int  sm_get_tile_mode(void);

#endif