#ifndef DESKTOPCTX_H
#define DESKTOPCTX_H

/* ── Desktop right-click context menu ───────────────────────────────────────
 *
 *  Integration: in gui.c's gui_render(), just before screen_flip(), add:
 *      if (desktopctx_is_open()) desktopctx_render();
 *
 * ─────────────────────────────────────────────────────────────────────────── */

void desktopctx_open(int screen_x, int screen_y);
void desktopctx_close(void);
void desktopctx_render(void);
int  desktopctx_click(int screen_x, int screen_y); /* 1 = consumed */
int  desktopctx_is_open(void);

#endif