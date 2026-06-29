#include "../kernel.h"
#include "../drivers/screen.h"
#include "../gfx/gfx.h"
#include "../gui/gui.h"
#include "../gui/theme.h"
#include "../gui/window.h"
#include "../gui/apps.h"
#include "../fs/fs.h"
#include "../sys/shutdown.h"
#include "desktopctx.h"

/* ── layout ──────────────────────────────────────────────────────────────── */
#define DC_ITEM_H  20
#define DC_ITEM_W  180
#define DC_CNT     9    /* total rows incl. separators */
#define DC_H       (DC_CNT * DC_ITEM_H + 6)

/* ── menu definition ─────────────────────────────────────────────────────── */
/*
   0  New Folder
   1  New Text File
   2  ── sep ──
   3  Open Files
   4  Settings
   5  ── sep ──
   6  Lock Screen
   7  ── sep ──
   8  Shutdown
*/
static const char* _labels[DC_CNT] = {
    "New Folder", "New Text File",
    NULL,
    "Open Files", "Settings",
    NULL,
    "Lock Screen",
    NULL,
    "Shutdown"
};

/* ── state ───────────────────────────────────────────────────────────────── */
static int _open = 0;
static int _cx   = 0;
static int _cy   = 0;

/* ── helpers ─────────────────────────────────────────────────────────────── */
static void _unique(const char* base, const char* ext, char* out, int olen) {
    for (int n = 1; n <= 99; n++) {
        char t[32]; int bi = 0;
        if (n == 1) {
            while (base[bi] && bi < 20) { t[bi] = base[bi]; bi++; }
            int ei = 0; while (ext[ei] && bi < 30) { t[bi++] = ext[ei++]; }
            t[bi] = '\0';
        } else {
            while (base[bi] && bi < 15) { t[bi] = base[bi]; bi++; }
            t[bi++] = ' ';
            if (n >= 10) t[bi++] = '0' + n / 10;
            t[bi++] = '0' + n % 10;
            int ei = 0; while (ext[ei] && bi < 30) { t[bi++] = ext[ei++]; }
            t[bi] = '\0';
        }
        int found = 0;
        for (int i = 0; i < fs_node_count; i++)
            if (fs_tree[i].type != 0 && fs_tree[i].parent_idx == current_dir_idx
                && my_strcmp(fs_tree[i].name, t) == 0) { found = 1; break; }
        if (!found) {
            int k = 0; while (t[k] && k < olen-1) { out[k] = t[k]; k++; } out[k] = '\0';
            return;
        }
    }
    int k = 0; while (base[k] && k < olen-1) { out[k] = base[k]; k++; } out[k] = '\0';
}

/* ── action ──────────────────────────────────────────────────────────────── */
static void _action(int item) {
    _open = 0;
    switch (item) {
    case 0: { /* New Folder */
        char n[32]; _unique("New Folder", "", n, 32);
        fs_mkdir_in(current_dir_idx, n);
        break;
    }
    case 1: { /* New Text File */
        char n[32]; _unique("untitled", ".txt", n, 32);
        fs_touch_in(current_dir_idx, n);
        break;
    }
    case 3: /* Open Files */
        app_vosdolp_open();
        break;
    case 4: /* Settings */
        app_settings_open();
        break;
    case 6: /* Lock Screen */
        gui_switch_mode(GUI_MODE_LOCKED);
        break;
    case 8: /* Shutdown */
        sys_shutdown();
        break;
    default: break;
    }
    gui_render();
}

/* ── public API ──────────────────────────────────────────────────────────── */

void desktopctx_open(int x, int y) {
    _open = 1;
    _cx   = x;
    _cy   = y;
    if (_cx + DC_ITEM_W > SCREEN_WIDTH)  _cx = SCREEN_WIDTH  - DC_ITEM_W - 2;
    if (_cy + DC_H      > SCREEN_HEIGHT) _cy = SCREEN_HEIGHT - DC_H      - 2;
    gui_render();
}

void desktopctx_close(void) {
    _open = 0;
}

int desktopctx_is_open(void) {
    return _open;
}

void desktopctx_render(void) {
    if (!_open) return;
    int ax = _cx, ay = _cy;

    /* shadow */
    draw_rect(ax+3, ay+3, DC_ITEM_W, DC_H, RGB(0,0,0));
    /* border + background */
    draw_rect(ax,   ay,   DC_ITEM_W, DC_H, RGB(55,60,90));
    draw_rect(ax+1, ay+1, DC_ITEM_W-2, DC_H-2, RGB(26,30,50));

    int iy = ay + 3;
    for (int i = 0; i < DC_CNT; i++) {
        if (_labels[i] == (void*)0) {
            draw_line(ax+6, iy + DC_ITEM_H/2,
                      ax + DC_ITEM_W-6, iy + DC_ITEM_H/2, RGB(55,60,90));
        } else {
            uint32_t fg = (i == 8) ? RGB(220,80,80) : g_theme.text_primary;
            draw_string(ax+10, iy+3, _labels[i], fg, COLOR_TRANSPARENT);
        }
        iy += DC_ITEM_H;
    }
}

int desktopctx_click(int x, int y) {
    if (!_open) return 0;
    if (x >= _cx && x < _cx + DC_ITEM_W && y >= _cy && y < _cy + DC_H) {
        int item = (y - _cy - 3) / DC_ITEM_H;
        if (item >= 0 && item < DC_CNT && _labels[item] != (void*)0)
            _action(item);
        else
            _open = 0;
        return 1;
    }
    _open = 0;
    return 0;
}