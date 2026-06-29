#include "kernel.h"
#include "drivers/screen.h"
#include "gfx/gfx.h"
#include "gui/gui.h"
#include "gui/theme.h"
#include "gui/tray.h"
#include "gui/window.h"
#include "gui/startmenu.h"
#include "gui/net_panel.h"
#include "gui/taskbar.h"

/* ── Render ──────────────────────────────────────────────────────────────── */
void taskbar_render(void) {
    int y = SCREEN_HEIGHT - TASKBAR_HEIGHT;

    /* gradient background + top border */
    gfx_gradient_rect(0, y, SCREEN_WIDTH, TASKBAR_HEIGHT,
                      g_theme.taskbar_top, g_theme.taskbar_bot, 1);
    draw_line(0, y, SCREEN_WIDTH - 1, y, g_theme.taskbar_border);

    /* ── Start button ─────────────────────────────────────────────────── */
    uint32_t sbg = sm_is_open() ? g_theme.start_btn_hover : g_theme.start_btn;
    gfx_fill_rounded_rect(4, y+4, START_BTN_W, TASKBAR_HEIGHT-8, 4, sbg);
    draw_string(12, y+6, "[ VOS ]", g_theme.text_primary, COLOR_TRANSPARENT);

    /* ── Workspace dots ───────────────────────────────────────────────── */
    int ws = win_get_workspace();
    for (int i = 0; i < MAX_WORKSPACES; i++) {
        int dx = START_BTN_W + 16 + i * 18;
        uint32_t dc = (i == ws) ? g_theme.accent : RGB(80,80,100);
        gfx_fill_circle(dx, y + TASKBAR_HEIGHT/2, 5, dc);
    }

    /* ── Window taskbar buttons ───────────────────────────────────────── */
    int tw    = tray_total_width();
    int btn_w = SCREEN_WIDTH - tw - 8 - TASKBAR_WIN_BTN_X;
    win_taskbar_render(TASKBAR_WIN_BTN_X, y+4, btn_w, TASKBAR_HEIGHT-8);

    /* ── Tray (clock, lang, net) ──────────────────────────────────────── */
    tray_render();
}

/* ── Click handling ──────────────────────────────────────────────────────── */
int taskbar_handle_click(int x, int y) {
    int ty = SCREEN_HEIGHT - TASKBAR_HEIGHT;
    if (y < ty) return 0;   /* not in taskbar */

    int panel_was_open = net_panel_is_open();
    net_panel_close();

    /* ── Start button ─────────────────────────────────────────────────── */
    if (x >= 4 && x <= 4 + START_BTN_W) {
        if (sm_is_open()) sm_close(); else sm_open();
        gui_render();
        return 1;
    }

    /* ── Workspace dots ───────────────────────────────────────────────── */
    for (int i = 0; i < MAX_WORKSPACES; i++) {
        int dx = START_BTN_W + 16 + i * 18;
        if (x >= dx-7 && x <= dx+7) {
            if (i != win_get_workspace()) win_switch_workspace(i);
            sm_close();
            gui_render();
            return 1;
        }
    }

    /* ── Window taskbar buttons ───────────────────────────────────────── */
    int tw    = tray_total_width();
    int btn_w = SCREEN_WIDTH - tw - 8 - TASKBAR_WIN_BTN_X;
    if (x >= TASKBAR_WIN_BTN_X && x < TASKBAR_WIN_BTN_X + btn_w) {
        if (win_taskbar_click(x, y, TASKBAR_WIN_BTN_X, ty+4, btn_w, TASKBAR_HEIGHT-8)) {
            sm_close();
            gui_render();
            return 1;
        }
    }

    /* ── Tray ─────────────────────────────────────────────────────────── */
    int net_tx = tray_item_x(0);
    int on_net = (x >= net_tx && x < net_tx + TRAY_ITEM_W);
    /* suppress net icon click if panel was already open (prevents instant re-open) */
    if (!panel_was_open || !on_net)
        tray_handle_click(x, y);

    sm_close();
    gui_render();
    return 1;
}