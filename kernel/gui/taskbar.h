#ifndef TASKBAR_H
#define TASKBAR_H

/* ── gui/taskbar.h ───────────────────────────────────────────────────────────
 *
 *  Standalone taskbar module.
 *  gui.c calls taskbar_render() and taskbar_handle_click() — nothing else.
 *
 * ─────────────────────────────────────────────────────────────────────────── */

/* X position where the window-button strip starts
   (after start button + workspace dots) */
#define TASKBAR_WIN_BTN_X  (START_BTN_W + 8 + MAX_WORKSPACES * 18 + 16)

void taskbar_render(void);

/* Returns 1 if the click was inside the taskbar and was fully handled.
   Caller should NOT process the click further if 1 is returned. */
int  taskbar_handle_click(int x, int y);

#endif