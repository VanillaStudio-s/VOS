#include "kernel.h"
#include "drivers/screen.h"
#include "gfx/gfx.h"
#include "gui/gui.h"
#include "gui/theme.h"
#include "gui/window.h"
#include "gui/notify.h"
#include "apps/welcome.h"

int app_autostart_welcome = 1;

static int _welcome_win = -1;
static int _welcome_tab = 0;
static int _update_done = 0;

#define TAB_H  26
#define TAB_W  110

static void _draw_tab_bar(int x, int y, int w) {
    static const char* labels[] = { "Changelog", "Roadmap", "Updates" };
    for (int i = 0; i < 3; i++) {
        int tx = x + i * TAB_W;
        uint32_t bg = (i == _welcome_tab) ? g_theme.accent : RGB(40,44,60);
        gfx_fill_rounded_rect(tx + 2, y + 2, TAB_W - 4, TAB_H - 4, 4, bg);
        draw_string(tx + 14, y + 7, labels[i],
                    (i == _welcome_tab) ? COLOR_WHITE : RGB(180,180,200),
                    COLOR_TRANSPARENT);
    }
    draw_line(x, y + TAB_H - 1, x + w - 1, y + TAB_H - 1, g_theme.accent);
}

static void _draw_changelog(int x, int y) {
    gfx_gradient_rect(x, y, 480, 32, g_theme.accent, RGB(0,60,120), 1);
    draw_string(x+12, y+9, "VOS v0.5.6 — What's New", COLOR_WHITE, COLOR_TRANSPARENT);
    int ly = y + 40;
    static const char* items[] = {
        " + Calculator  (GUI app, mouse-only)",
        " + System Monitor  (CPU / RAM / uptime)",
        " + Text Editor  (edit <file> + GUI window)",
        " + Terminal App  (GUI terminal window)",
        " + vosdolp  (dual-panel file manager)",
        " + New Start Menu  (search + power submenu)",
        " + scrot, cp, mv, rm  (new shell commands)",
        " + Command history  (Up/Down arrow keys)",
        " + Tab-completion  (filename auto-complete)",
        " ",
    };
    for (int i = 0; i < 10; i++) {
        draw_string(x+8, ly, items[i], RGB(100,220,100), COLOR_TRANSPARENT);
        ly += 17;
    }
}

static void _draw_roadmap(int x, int y) {
    static const char* v04[] = {
        " [x]  vosdolp  (File Manager, dual-panel)",
        " [x]  System Monitor  (CPU % + RAM graphs)",
        " [x]  Text Editor  (edit <file> in terminal)",
        " [x]  Calculator  (GUI app)",
        " [x]  Screenshot Tool  (scrot -> BMP)",
        " [x]  Terminal App  (GUI wrapper)",
        " [x]  Linux-style paths  (/home/user/...)",
        " [x]  App reorganisation  (kernel/apps/)",
        " [x]  Start Menu overhaul  (search + power)",
    };
    static const char* v05[] = {
        " [x]  NIC Driver  (RTL8139 / virtio-net)",
        " [x]  IPv4 + ICMP  (ping works)",
        " [x]  TCP/UDP + DHCP + DNS",
    };
    static const char* v06[] = {
        " CURRENTLTY: VOS APPS AND OVERHAUL",
        " [ ] VOS getting more Settings and more apps",
        " [ ] .vosapp is gonna existing",
        " [ ] .appimage from Linux will be supported 100%",
        " [ ] Terminal App Installation is gonna be the BIG THING",
        " [ ] first integration of .exe execution"
    };
    static const char* v07[] = {
        " VOS RELEASE AND AUDIO",
        " [ ] Audio all types, Stero and Mono",
        " [ ] Audio Settings",
        " [ ] Notification Sound",
        " [ ] VOS first ALPHA-Release"
    };
    static const char* v08[] = {
        " VOS RELEASE AND OVERHAUL",
        " [ ] VALORANT SUPPORT",
        " [ ] ANTI-CHEAT SUPPORT (FOR ESPORTS LIKE R6, VALO, DokiDoki and Jerkmate)",
        " [ ] VOS first ALPHA-Release"
    };
    static const char* v11[] = {
        " [ ]  Preemptive Round-Robin Scheduler",
        " [ ]  Virtual Memory  (per-process paging)",
        " [ ]  ELF Loader + Syscall Interface",
    };

    int cy = y + 4;

    draw_string(x+8, cy, "v0.4 — Built-in Apps", g_theme.accent_light, COLOR_TRANSPARENT);
    draw_string(x+320, cy, "FINISHED", RGB(120,120,140), COLOR_TRANSPARENT);
    cy += 16; draw_line(x+8, cy, x+462, cy, g_theme.accent); cy += 5;
    for (int i = 0; i < 9; i++) {
        draw_string(x+16, cy, v04[i], g_theme.text_secondary, COLOR_TRANSPARENT);
        cy += 14;
    }
    cy += 4;

    draw_string(x+8, cy, "v0.5 — Network Stack", g_theme.accent_light, COLOR_TRANSPARENT);
    draw_string(x+320, cy, "FINISHED", RGB(120,120,140), COLOR_TRANSPARENT);
    cy += 16; draw_line(x+8, cy, x+462, cy, g_theme.accent); cy += 5;
    for (int i = 0; i < 3; i++) {
        draw_string(x+16, cy, v05[i], g_theme.text_secondary, COLOR_TRANSPARENT);
        cy += 14;
    }
    cy += 4;
    draw_string(x+8, cy, "v0.6 — VOS Apps and Overhaul", g_theme.accent_light, COLOR_TRANSPARENT);
    draw_string(x+320, cy, "LATE 2026", RGB(120,120,140), COLOR_TRANSPARENT);
    cy += 16; draw_line(x+8, cy, x+462, cy, g_theme.accent); cy += 5;
    for (int i = 0; i < 3; i++) {
        draw_string(x+16, cy, v06[i], g_theme.text_secondary, COLOR_TRANSPARENT);
        cy += 14;
    }
    cy += 4;
    
    draw_string(x+8, cy, "v0.7 — FIRST ALPHA + VOS AUDIO", g_theme.accent_light, COLOR_TRANSPARENT);
    draw_string(x+320, cy, "EARLY 2027", RGB(120,120,140), COLOR_TRANSPARENT);
    cy += 16; draw_line(x+8, cy, x+462, cy, g_theme.accent); cy += 5;
    for (int i = 0; i < 3; i++) {
        draw_string(x+16, cy, v07[i], g_theme.text_secondary, COLOR_TRANSPARENT);
        cy += 14;
    }
    cy += 4;
    
    draw_string(x+8, cy, "v0.8 — MORE", g_theme.accent_light, COLOR_TRANSPARENT);
    draw_string(x+320, cy, "EARLY 2027", RGB(120,120,140), COLOR_TRANSPARENT);
    cy += 16; draw_line(x+8, cy, x+462, cy, g_theme.accent); cy += 5;
    for (int i = 0; i < 3; i++) {
        draw_string(x+16, cy, v07[i], g_theme.text_secondary, COLOR_TRANSPARENT);
        cy += 14;
    }
    cy += 4;
    
    draw_string(x+8, cy, "v0.9 — Multitasking", g_theme.accent_light, COLOR_TRANSPARENT);
    draw_string(x+320, cy, "MID 2027", RGB(120,120,140), COLOR_TRANSPARENT);
    cy += 16; draw_line(x+8, cy, x+462, cy, g_theme.accent); cy += 5;
    for (int i = 0; i < 3; i++) {
        draw_string(x+16, cy, v07[i], g_theme.text_secondary, COLOR_TRANSPARENT);
        cy += 14;
    }
    cy += 4;

    draw_string(x+8, cy, "v1.0 — RELEASE", g_theme.accent_light, COLOR_TRANSPARENT);
    draw_string(x+320, cy, "EARLY 2028", RGB(120,120,140), COLOR_TRANSPARENT);
    cy += 16; draw_line(x+8, cy, x+462, cy, g_theme.accent); cy += 5;
    for (int i = 0; i < 3; i++) {
        draw_string(x+16, cy, v07[i], g_theme.text_secondary, COLOR_TRANSPARENT);
        cy += 14;
    }
    cy += 4;
    
    draw_string(x+8, cy, "v1.1 — ", g_theme.accent_light, COLOR_TRANSPARENT);
    draw_string(x+320, cy, "EARLY 2028", RGB(120,120,140), COLOR_TRANSPARENT);
    cy += 16; draw_line(x+8, cy, x+462, cy, g_theme.accent); cy += 5;
    for (int i = 0; i < 3; i++) {
        draw_string(x+16, cy, v11[i], g_theme.text_secondary, COLOR_TRANSPARENT);
        cy += 14;
    }
}

static void _draw_updates(int x, int y) {
    draw_string(x+8,   y+6,  "System Version:", g_theme.text_secondary, COLOR_TRANSPARENT);
    draw_string(x+145, y+6,  "VOS v0.4",         g_theme.text_primary,   COLOR_TRANSPARENT);
    draw_string(x+8,   y+22, "Build Date:",      g_theme.text_secondary, COLOR_TRANSPARENT);
    draw_string(x+145, y+22, "2026-06-05",       g_theme.text_primary,   COLOR_TRANSPARENT);
    draw_string(x+8,   y+38, "Kernel:",          g_theme.text_secondary, COLOR_TRANSPARENT);
    draw_string(x+145, y+38, "x86_64  bare-metal", g_theme.text_primary, COLOR_TRANSPARENT);

    draw_line(x+8, y+54, x+462, y+54, RGB(60,65,85));

    uint32_t btn_col = _update_done ? RGB(60,80,60) : g_theme.accent;
    gfx_fill_rounded_rect(x+8, y+60, 160, 22, 4, btn_col);
    draw_string(x+18, y+64,
                _update_done ? "Up to date!" : "Check for Updates",
                COLOR_WHITE, COLOR_TRANSPARENT);
    if (_update_done)
        draw_string(x+178, y+64, "No updates found.", RGB(100,220,100), COLOR_TRANSPARENT);

    draw_line(x+8, y+90, x+462, y+90, RGB(60,65,85));
    draw_string(x+8, y+98, "Installed apps:", g_theme.accent_light, COLOR_TRANSPARENT);

    static const char* app_names[] = {
        "VOS Shell", "Settings App", "Welcome App", "File System", "VOS Dolphine"
    };
    for (int i = 0; i < 4; i++) {
        int ay = y + 116 + i * 18;
        draw_string(x+16,  ay, app_names[i],  g_theme.text_primary,   COLOR_TRANSPARENT);
        draw_string(x+210, ay, "v0.5.6",       g_theme.text_secondary, COLOR_TRANSPARENT);
        draw_string(x+265, ay, "[up to date]", RGB(100,200,100),       COLOR_TRANSPARENT);
    }
}

static void _draw_welcome(int x, int y, int w, int h) {
    (void)h;
    _draw_tab_bar(x, y, w);
    int cy = y + TAB_H + 2;
    if      (_welcome_tab == 0) _draw_changelog(x, cy);
    else if (_welcome_tab == 1) _draw_roadmap(x, cy);
    else                        _draw_updates(x, cy);
}

static void _click_welcome(int rx, int ry) {
    if (ry < TAB_H) {
        int t = rx / TAB_W;
        if (t >= 0 && t < 3) {
            _welcome_tab = t;
            _update_done = 0;
        }
        return;
    }
    if (_welcome_tab == 2) {
        int btn_y = TAB_H + 2 + 60;
        if (ry >= btn_y && ry < btn_y + 22 && rx >= 8 && rx < 168) {
            _update_done = 1;
            notify_push("Updates", "VOS v0.5.6 is up to date!", NOTIFY_SUCCESS);
        }
    }
}

void app_welcome_open(void) {
    if (win_is_open(_welcome_win)) { win_set_focus(_welcome_win); return; }
    _welcome_tab = 0;
    _update_done = 0;
    _welcome_win = win_create(80, 25, 480, 340,
                              "Welcome-App",
                              WIN_FLAG_DRAGGABLE | WIN_FLAG_CLOSABLE | WIN_FLAG_MINIMIZABLE | WIN_FLAG_RESIZABLE);

    win_set_min_size(_welcome_win, 340, 220);
    if (_welcome_win >= 0) {
        win_set_content(_welcome_win, _draw_welcome);
        win_set_click(_welcome_win, _click_welcome);
    }
}
