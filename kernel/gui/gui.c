#include "kernel.h"
#include "cpu/rtc.h"
#include "drivers/screen.h"
#include "drivers/mouse.h"
#include "gfx/gfx.h"
#include "sys/user.h"
#include "gui/gui.h"
#include "gui/theme.h"
#include "gui/tray.h"
#include "gui/notify.h"
#include "gui/window.h"
#include "gui/screensaver.h"
#include "gui/startmenu.h"
#include "gui/taskbar.h"
#include "gui/desktopctx.h"
#include "gui/apps.h"
#include "gui/net_panel.h"
#include "apps/design.h"
#include "sys/shutdown.h"

int gui_mode = GUI_MODE_DESKTOP;
static RTCTime _rtc_cache;

/* ── Idle / screensaver / auto-lock ──────────────────────────────────────── */
static unsigned int _idle_ticks = 0;
#define SCREENSAVER_TIMEOUT  3000
#define LOCK_TIMEOUT         6000

void gui_notify_activity(void) {
    _idle_ticks = 0;
    if (gui_mode == GUI_MODE_SCREENSAVE)
        gui_switch_mode(GUI_MODE_DESKTOP);
}

void gui_tick(void) {
    static unsigned int tc = 0;
    notify_tick();
    _idle_ticks++;
    if (_idle_ticks == SCREENSAVER_TIMEOUT && gui_mode == GUI_MODE_DESKTOP)
        gui_switch_mode(GUI_MODE_SCREENSAVE);
    if (_idle_ticks == LOCK_TIMEOUT && gui_mode != GUI_MODE_LOCKED)
        gui_switch_mode(GUI_MODE_LOCKED);

    if (++tc < 100) return;
    tc = 0;
    rtc_read(&_rtc_cache);
    tray_tick(&_rtc_cache);
}

/* ── Desktop background ──────────────────────────────────────────────────── */
void gui_draw_desktop(void) {
    draw_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - TASKBAR_HEIGHT,
              g_theme.desktop_bg);
}

/* ── Render ──────────────────────────────────────────────────────────────── */
void gui_render(void) {
    if (gui_mode != GUI_MODE_DESKTOP) return;
    gui_draw_desktop();
    win_render_all();
    taskbar_render();                              /* ← standalone taskbar   */
    sm_render();                                   /* ← standalone startmenu */
    notify_render();
    net_panel_render();
    if (desktopctx_is_open()) desktopctx_render(); /* ← right-click overlay  */
    mouse_draw_cursor();
    screen_flip();
}

/* ── Click handling ──────────────────────────────────────────────────────── */
void gui_handle_click(int x, int y) {
    gui_notify_activity();

    /* network panel overlay */
    if (net_panel_is_open() && y < SCREEN_HEIGHT - TASKBAR_HEIGHT) {
        if (!net_panel_handle_click(x, y)) net_panel_close();
        gui_render();
        return;
    }

    /* taskbar eats its own clicks */
    if (taskbar_handle_click(x, y)) return;

    /* start menu eats its own clicks */
    if (sm_is_open()) {
        sm_click(x, y);
        return;
    }
}

/* ── Init ────────────────────────────────────────────────────────────────── */
void gui_init(void) {
    theme_apply_dark();
    design_init();
    rtc_read(&_rtc_cache);
    tray_init();
    tray_tick(&_rtc_cache);
    sm_init();
    gui_mode = GUI_MODE_DESKTOP;
    notify_push("VOS", "Welcome to VOS!", NOTIFY_SUCCESS);
    gui_render();
}

/* ── Start menu bridge (kernel.c uses these) ─────────────────────────────── */
void gui_start_menu_close(void)   { sm_close();     }
int  gui_start_menu_is_open(void) { return sm_is_open(); }
void gui_start_menu_key(char c)   { sm_key(c);      }

/* ── Mode switch ─────────────────────────────────────────────────────────── */
void gui_switch_mode(int mode) {
    sm_close();
    desktopctx_close();
    gui_mode = mode;

    if (mode == GUI_MODE_DESKTOP) {
        _idle_ticks = 0;
        gui_render();
    } else if (mode == GUI_MODE_SCREENSAVE) {
        screensaver_init();
    }
    /* GUI_MODE_LOCKED: lockscreen_render() is called from kernel.c's main loop */
}