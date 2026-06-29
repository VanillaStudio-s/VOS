/* ═══════════════════════════════════════════════════════════════════
   settings.c  –  VOS Settings App  (fully expanded, v0.4.x)
   ═══════════════════════════════════════════════════════════════════ */
#include "kernel.h"
#include "drivers/screen.h"
#include "drivers/keyboard.h"
#include "gfx/gfx.h"
#include "gui/gui.h"
#include "gui/theme.h"
#include "gui/window.h"
#include "gui/notify.h"
#include "sys/user.h"
#include "apps/welcome.h"
#include "apps/settings.h"
#include "net/net.h"
#include "net/dhcp.h"
#include "net/rtl8139.h"
#include "net/pcnet.h"

/* ── Layout ─────────────────────────────────────────────────────── */
#define SIDEBAR_W       120
#define SECTION_H        34
#define NUM_SECTIONS      6
#define P                 8    /* general content padding */

/* ── Section IDs ─────────────────────────────────────────────────── */
#define SEC_ACCOUNT     0
#define SEC_NETWORK     1
#define SEC_LANGUAGE    2
#define SEC_APPEARANCE  3
#define SEC_AUTOSTART   4
#define SEC_ABOUT       5

/* ── Appearance sub-tab IDs ──────────────────────────────────────── */
#define NUM_APP_SUBS    5
#define APP_COLOR       0
#define APP_WINDOW      1
#define APP_MOUSE       2
#define APP_BG          3
#define APP_TRANSP      4

/* ═══════════════════════════════════════════════════════════════════
   State
   ═══════════════════════════════════════════════════════════════════ */
static int _settings_win     = -1;
static int _settings_section = SEC_APPEARANCE;
static int _content_w        = 320;   /* updated every draw; used by click */

static const char *_section_names[NUM_SECTIONS] = {
    "Account", "Network", "Language", "Appearance", "Auto-Start", "About"
};
static const char *_app_sub_names[NUM_APP_SUBS] = {
    "Color", "Window", "Mouse", "Background", "Transp."
};

/* palettes */
static const uint32_t _accents[6] = {
    0x0078D7, 0x107C10, 0x881798, 0xD13438, 0xCA5010, 0x038387
};
static const uint32_t _bg_colors[8] = {
    0x1A1A2E, 0x16213E, 0x0F3460, 0x1B262C,
    0x2C1654, 0x0D0D0D, 0x1A2634, 0x212121
};

/* per-section state */
static int _autostart_welcome_ui = 1;
static int _app_sub              = APP_COLOR;
static int _bg_sel               = 0;
static int _win_rounded          = 1;
static int _win_shadow           = 1;
static int _win_border           = 1;
static int _cursor_style         = 0;   /* 0=Arrow  1=Cross  2=Dot */
static int _transparency         = 0;   /* 0..4 */

/* ═══════════════════════════════════════════════════════════════════
   Public getters  (read by GUI / GFX systems)
   ═══════════════════════════════════════════════════════════════════ */
int      settings_get_win_rounded  (void) { return _win_rounded;      }
int      settings_get_win_shadow   (void) { return _win_shadow;       }
int      settings_get_win_border   (void) { return _win_border;       }
int      settings_get_transparency (void) { return _transparency;     }
uint32_t settings_get_bg_color     (void) { return _bg_colors[_bg_sel]; }
int      settings_get_cursor_style (void) { return _cursor_style;     }

/* ═══════════════════════════════════════════════════════════════════
   Helpers
   ═══════════════════════════════════════════════════════════════════ */

static void _u8str(uint8_t v, char *buf) {
    if (!v) { buf[0] = '0'; buf[1] = '\0'; return; }
    char t[4]; int i = 0;
    while (v) { t[i++] = (char)('0' + v % 10); v /= 10; }
    int j;
    for (j = 0; j < i; j++) buf[j] = t[i - 1 - j];
    buf[j] = '\0';
}

static void _fmt_ip(IpAddr ip, char *out) {
    int pos = 0;
    for (int i = 0; i < 4; i++) {
        char t[4]; _u8str(ip.b[i], t);
        for (int k = 0; t[k]; k++) out[pos++] = t[k];
        if (i < 3) out[pos++] = '.';
    }
    out[pos] = '\0';
}

static void _fmt_mac(MacAddr m, char *out) {
    static const char H[] = "0123456789ABCDEF";
    int pos = 0;
    for (int i = 0; i < 6; i++) {
        out[pos++] = H[m.b[i] >> 4];
        out[pos++] = H[m.b[i] & 0xF];
        if (i < 5) out[pos++] = ':';
    }
    out[pos] = '\0';
}

/* section header: title + rule */
static void _hdr(int x, int y, int w, const char *title) {
    draw_string(x + P, y + 6, title, g_theme.accent_light, COLOR_TRANSPARENT);
    draw_line(x + P, y + 20, x + w - P, y + 20, g_theme.accent);
}

/* key–value row */
static void _kv(int x, int y, const char *k, const char *v, uint32_t vc) {
    draw_string(x, y, k, g_theme.text_secondary, COLOR_TRANSPARENT);
    draw_string(x + 112, y, v, vc, COLOR_TRANSPARENT);
}

/* separator */
static void _sep(int x, int y, int w) {
    draw_line(x + P, y, x + w - P, y, RGB(50, 55, 75));
}

/* toggle pill */
static void _toggle(int x, int y, int bw, int on) {
    gfx_fill_rounded_rect(x, y, bw, 18, 4, on ? g_theme.accent : RGB(55, 58, 78));
    draw_string(x + (bw - 24) / 2, y + 2,
                on ? " ON" : "OFF", COLOR_WHITE, COLOR_TRANSPARENT);
}

/* ═══════════════════════════════════════════════════════════════════
   Section: Account
   ═══════════════════════════════════════════════════════════════════ */
static void _draw_section_account(int x, int y, int w) {
    _hdr(x, y, w, "Account");
    int lx = x + P;
    const char *uname = user_database[active_user_idx];

    /* avatar */
    gfx_fill_circle(lx + 22, y + 50, 21, g_theme.accent);
    char ini[2] = { uname[0] ? uname[0] : '?', '\0' };
    draw_string(lx + 16, y + 43, ini, COLOR_WHITE, COLOR_TRANSPARENT);

    /* info block */
    _kv(lx + 52, y + 34, "Username:", uname,           g_theme.text_primary);
    _kv(lx + 52, y + 50, "Role:",     "Administrator", g_theme.text_primary);
    _kv(lx + 52, y + 66, "Session:",  "Active",        RGB(80, 200, 80));

    _sep(x, y + 86, w);

    /* security */
    draw_string(lx, y + 95, "Security", g_theme.accent_light, COLOR_TRANSPARENT);
    _kv(lx, y + 112, "Password:", "••••••••", g_theme.text_primary);

    gfx_fill_rounded_rect(lx, y + 128, 148, 18, 4, RGB(50, 52, 72));
    draw_string(lx + 8, y + 130, "Change Password", RGB(175, 180, 210), COLOR_TRANSPARENT);
    draw_string(lx + 156, y + 130, "(v0.5)", RGB(100, 100, 130), COLOR_TRANSPARENT);

    _sep(x, y + 156, w);

    /* session meta */
    char idx_s[4]; _u8str((uint8_t)active_user_idx, idx_s);
    _kv(lx, y + 164, "User Index:",   idx_s,      g_theme.text_primary);
    _kv(lx, y + 180, "Login Method:", "Password", g_theme.text_primary);
    _kv(lx, y + 196, "Permissions:",  "Full",     g_theme.text_primary);
}

/* ═══════════════════════════════════════════════════════════════════
   Section: Network   ← fixed: reads actual net_* globals + driver IOs
   ═══════════════════════════════════════════════════════════════════ */
static void _draw_section_network(int x, int y, int w) {
    _hdr(x, y, w, "Network");
    int lx = x + P;

    /* ── Status ── */
    const char *status_s; uint32_t status_c;
    if (!net_up) {
        status_s = "No NIC detected";   status_c = RGB(220, 70, 70);
    } else if (!net_has_ip) {
        status_s = "DHCP pending...";   status_c = RGB(210, 170, 50);
    } else {
        status_s = "Connected";         status_c = RGB(70, 210, 70);
    }
    _kv(lx, y + 28, "Status:", status_s, status_c);

    /* ── Driver ── */
    const char *drv_s = "None";
    if (net_up) {
        if      (rtl8139_io) drv_s = "RTL8139";
        else if (pcnet_io)   drv_s = "PCnet-FAST III";
    }
    _kv(lx, y + 44, "Driver:", drv_s, g_theme.text_primary);

    /* ── MAC ── */
    char mac_s[20] = "---";
    if (net_up) _fmt_mac(net_mac, mac_s);
    _kv(lx, y + 60, "MAC:", mac_s, g_theme.text_primary);

    _sep(x, y + 78, w);

    /* ── IP block ── */
    char ip_s[20]   = "---";
    char mask_s[20] = "---";
    char gw_s[20]   = "---";
    char dns_s[20]  = "---";
    uint32_t ipc = net_has_ip ? g_theme.text_primary : RGB(110, 110, 135);
    if (net_has_ip) {
        _fmt_ip(net_ip,      ip_s);
        _fmt_ip(net_mask,    mask_s);
        _fmt_ip(net_gw,      gw_s);
        _fmt_ip(net_dns_srv, dns_s);
    }
    _kv(lx, y +  86, "IP Address:",  ip_s,    ipc);
    _kv(lx, y + 102, "Subnet Mask:", mask_s,  ipc);
    _kv(lx, y + 118, "Gateway:",     gw_s,    ipc);
    _kv(lx, y + 134, "DNS Server:",  dns_s,   ipc);

    _sep(x, y + 152, w);

    /* ── Reconnect button ── */
    uint32_t btn_c = net_up ? g_theme.accent : RGB(55, 58, 78);
    gfx_fill_rounded_rect(lx, y + 160, 96, 20, 4, btn_c);
    draw_string(lx + 10, y + 163, "Reconnect", COLOR_WHITE, COLOR_TRANSPARENT);

    const char *hint =
        !net_up     ? "No NIC  –  check VM / hardware config" :
        !net_has_ip ? "DHCP running...  click to retry"       :
                      "Click to renew DHCP lease";
    draw_string(lx + 104, y + 163, hint, RGB(110, 110, 135), COLOR_TRANSPARENT);
}

/* ═══════════════════════════════════════════════════════════════════
   Section: Language
   ═══════════════════════════════════════════════════════════════════ */
static void _draw_section_language(int x, int y, int w) {
    _hdr(x, y, w, "Language");
    int lx = x + P;

    draw_string(lx, y + 28, "Keyboard Layout:", g_theme.text_secondary, COLOR_TRANSPARENT);
    gfx_fill_rounded_rect(lx,      y + 44, 60, 18, 4,
                          current_layout == 0 ? g_theme.accent : RGB(55,58,78));
    draw_string(lx + 14,  y + 46, "EN", COLOR_WHITE, COLOR_TRANSPARENT);
    gfx_fill_rounded_rect(lx + 70, y + 44, 60, 18, 4,
                          current_layout == 1 ? g_theme.accent : RGB(55,58,78));
    draw_string(lx + 84,  y + 46, "DE", COLOR_WHITE, COLOR_TRANSPARENT);

    _sep(x, y + 74, w);

    _kv(lx, y +  82, "System Locale:",  "English (en_US)", g_theme.text_primary);
    _kv(lx, y +  98, "Date Format:",    "YYYY-MM-DD",      g_theme.text_primary);
    _kv(lx, y + 114, "Time Format:",    "24-hour",         g_theme.text_primary);
    _kv(lx, y + 130, "Decimal Sep.:",   ". (dot)",         g_theme.text_primary);
    _kv(lx, y + 146, "Encoding:",       "UTF-8 / ASCII",   g_theme.text_primary);

    _sep(x, y + 164, w);
    draw_string(lx, y + 172, "Full locale support planned for v0.5",
                RGB(100, 100, 130), COLOR_TRANSPARENT);
}

/* ═══════════════════════════════════════════════════════════════════
   Appearance: sub-tab bar
   ═══════════════════════════════════════════════════════════════════ */
static void _draw_app_subtabs(int x, int y, int w) {
    int tw = (w - 16) / NUM_APP_SUBS;
    for (int i = 0; i < NUM_APP_SUBS; i++) {
        int tx = x + 8 + i * tw;
        uint32_t c = (i == _app_sub) ? g_theme.accent : RGB(40, 43, 62);
        gfx_fill_rounded_rect(tx, y + 2, tw - 2, 18, 3, c);
        draw_string(tx + 5, y + 4, _app_sub_names[i], COLOR_WHITE, COLOR_TRANSPARENT);
    }
}

/* ── Color ── */
static void _draw_app_color(int x, int y, int w) {
    int lx = x + P;

    /* theme toggle */
    draw_string(lx, y + 2, "Theme:", g_theme.text_secondary, COLOR_TRANSPARENT);
    gfx_fill_rounded_rect(lx + 70, y - 2, 178, 18, 4, g_theme.accent);
    draw_string(lx + 76, y,
        g_theme_mode == THEME_DARK ? "  Dark    [Switch to Light]"
                                   : "  Light   [Switch to Dark ]",
        COLOR_WHITE, COLOR_TRANSPARENT);

    /* accent circles */
    draw_string(lx, y + 28, "Accent Color:", g_theme.text_secondary, COLOR_TRANSPARENT);
    for (int i = 0; i < 6; i++) {
        int ax = lx + i * 28;
        gfx_fill_circle(ax + 10, y + 50, 10, _accents[i]);
        if (_accents[i] == g_theme.accent)
            draw_rect_outline(ax, y + 40, 20, 20, COLOR_WHITE);
    }

    _sep(x, y + 68, w);

    /* preview strip */
    draw_string(lx, y + 76, "Color Preview:", g_theme.text_secondary, COLOR_TRANSPARENT);
    gfx_fill_rounded_rect(lx,       y + 92, 58, 22, 4, g_theme.accent);
    gfx_fill_rounded_rect(lx + 64,  y + 92, 58, 22, 4, g_theme.win_bg);
    gfx_fill_rounded_rect(lx + 128, y + 92, 58, 22, 4, g_theme.desktop_bg);
    draw_string(lx +  8,  y + 96, "Accent",  COLOR_WHITE,          COLOR_TRANSPARENT);
    draw_string(lx + 72,  y + 96, "Surface", g_theme.text_primary, COLOR_TRANSPARENT);
    draw_string(lx + 136, y + 96, "BG",      g_theme.text_primary, COLOR_TRANSPARENT);
}

/* ── Window ── */
static void _draw_app_window(int x, int y, int w) {
    int lx = x + P;

    draw_string(lx, y +  2, "Rounded Corners:", g_theme.text_secondary, COLOR_TRANSPARENT);
    _toggle(lx + 150, y -  2, 60, _win_rounded);

    draw_string(lx, y + 26, "Drop Shadow:",     g_theme.text_secondary, COLOR_TRANSPARENT);
    _toggle(lx + 150, y + 22, 60, _win_shadow);

    draw_string(lx, y + 50, "Window Border:",   g_theme.text_secondary, COLOR_TRANSPARENT);
    _toggle(lx + 150, y + 46, 60, _win_border);

    _sep(x, y + 74, w);

    /* live mini preview */
    draw_string(lx, y + 82, "Preview:", g_theme.text_secondary, COLOR_TRANSPARENT);
    int px = lx, py = y + 98;
    int pr = _win_rounded ? 5 : 0;
    gfx_fill_rounded_rect(px, py, 154, 72, pr, g_theme.win_bg);
    if (_win_border) draw_rect_outline(px, py, 154, 72, g_theme.accent);
    gfx_fill_rounded_rect(px + 1, py + 1, 152, 18, pr, g_theme.accent);
    draw_string(px + 6,   py +  4, "My Window",     COLOR_WHITE,          COLOR_TRANSPARENT);
    gfx_fill_circle(px + 144, py + 9, 6, RGB(220, 55, 55));
    draw_string(px + 6,   py + 26, "Content area",  g_theme.text_secondary, COLOR_TRANSPARENT);
    draw_string(px + 6,   py + 44, "Lorem ipsum...", g_theme.text_secondary, COLOR_TRANSPARENT);
}

/* ── Mouse ── */
static void _draw_app_mouse(int x, int y, int w) {
    int lx = x + P;
    static const char *cn[] = { "Arrow", "Cross", "Dot" };

    draw_string(lx, y, "Cursor Style:", g_theme.text_secondary, COLOR_TRANSPARENT);
    for (int i = 0; i < 3; i++) {
        int bx = lx + i * 72;
        gfx_fill_rounded_rect(bx, y + 16, 66, 18, 4,
            i == _cursor_style ? g_theme.accent : RGB(55,58,78));
        draw_string(bx + 10, y + 18, cn[i], COLOR_WHITE, COLOR_TRANSPARENT);
    }

    _sep(x, y + 44, w);

    _kv(lx, y + 52, "Speed:",        "Default     (v0.5)", g_theme.text_primary);
    _kv(lx, y + 68, "Left-Handed:",  "No          (v0.5)", RGB(100,100,130));
    _kv(lx, y + 84, "Acceleration:", "Linear      (v0.5)", RGB(100,100,130));
    _kv(lx, y +100, "Double-Click:", "400 ms      (v0.5)", RGB(100,100,130));

    _sep(x, y + 118, w);
    draw_string(lx, y + 126, "Full mouse config planned for v0.5",
                RGB(100,100,130), COLOR_TRANSPARENT);
}

/* ── Background ── */
static void _draw_app_bg(int x, int y, int w) {
    int lx = x + P;

    draw_string(lx, y, "Desktop Color:", g_theme.text_secondary, COLOR_TRANSPARENT);
    int sw = 30, gap = 5;
    for (int i = 0; i < 8; i++) {
        int bx = lx + (i % 4) * (sw + gap);
        int by = y + 16 + (i / 4) * (sw + gap);
        gfx_fill_rounded_rect(bx, by, sw, sw, 4, _bg_colors[i]);
        if (i == _bg_sel)
            draw_rect_outline(bx - 2, by - 2, sw + 4, sw + 4, COLOR_WHITE);
    }

    _sep(x, y + 96, w);

    draw_string(lx, y + 104, "Wallpaper Images:", g_theme.text_secondary, COLOR_TRANSPARENT);
    gfx_fill_rounded_rect(lx, y + 120, 168, 18, 4, RGB(45,48,68));
    draw_string(lx + 6, y + 122, "Browse...  (not available yet)",
                RGB(140,142,165), COLOR_TRANSPARENT);

    draw_string(lx, y + 152, "Custom color will apply on next boot.",
                RGB(100,100,130), COLOR_TRANSPARENT);
}

/* ── Transparency ── */
static void _draw_app_transparency(int x, int y, int w) {
    int lx = x + P;
    static const char *lvls[] = { "Off", "Low", "Med", "High", "Max" };

    draw_string(lx, y, "Window Transparency:", g_theme.text_secondary, COLOR_TRANSPARENT);
    for (int i = 0; i < 5; i++) {
        int bx = lx + i * 54;
        gfx_fill_rounded_rect(bx, y + 16, 48, 18, 4,
            i == _transparency ? g_theme.accent : RGB(55,58,78));
        draw_string(bx + 7, y + 18, lvls[i], COLOR_WHITE, COLOR_TRANSPARENT);
    }

    _sep(x, y + 44, w);

    /* preview */
    draw_string(lx, y + 52, "Preview:", g_theme.text_secondary, COLOR_TRANSPARENT);
    gfx_fill_rounded_rect(lx, y + 68, 140, 76, 4, RGB(22, 24, 38));
    draw_string(lx + 6, y + 86, "Desktop / BG", RGB(70, 75, 110), COLOR_TRANSPARENT);
    /* overlay brightness shifts with level */
    uint8_t br = (uint8_t)(38 + _transparency * 18);
    gfx_fill_rounded_rect(lx + 14, y + 74, 112, 64, 4, RGB(br, (uint8_t)(br+2), (uint8_t)(br+20)));
    draw_string(lx + 20, y +  88, "Window",  COLOR_WHITE,          COLOR_TRANSPARENT);
    draw_string(lx + 20, y + 104, "Content", g_theme.text_secondary, COLOR_TRANSPARENT);

    _sep(x, y + 154, w);
    draw_string(lx, y + 162, "HW compositing planned for v0.6",
                RGB(100,100,130), COLOR_TRANSPARENT);
}

/* ── Appearance dispatcher ── */
static void _draw_section_appearance(int x, int y, int w) {
    _hdr(x, y, w, "Appearance");
    _draw_app_subtabs(x, y + 22, w);
    int cy = y + 48;
    switch (_app_sub) {
        case APP_COLOR:  _draw_app_color (x, cy, w); break;
        case APP_WINDOW: _draw_app_window(x, cy, w); break;
        case APP_MOUSE:  _draw_app_mouse (x, cy, w); break;
        case APP_BG:     _draw_app_bg    (x, cy, w); break;
        case APP_TRANSP: _draw_app_transparency(x, cy, w); break;
    }
}

/* ═══════════════════════════════════════════════════════════════════
   Section: Auto-Start
   ═══════════════════════════════════════════════════════════════════ */
static void _draw_section_autostart(int x, int y, int w) {
    _hdr(x, y, w, "Auto-Start");
    int lx = x + P;

    draw_string(lx, y + 32, "Welcome App:", g_theme.text_secondary, COLOR_TRANSPARENT);
    _toggle(lx + 126, y + 28, 62, _autostart_welcome_ui);

    _sep(x, y + 58, w);
    draw_string(lx, y + 66, "* Toggles the Welcome window on each boot.",
                RGB(100,100,130), COLOR_TRANSPARENT);
    draw_string(lx, y + 82, "  Changes apply after the next restart.",
                RGB(100,100,130), COLOR_TRANSPARENT);

    _sep(x, y + 102, w);
    draw_string(lx, y + 110, "More auto-start entries planned for v0.5",
                RGB(100,100,130), COLOR_TRANSPARENT);
}

/* ═══════════════════════════════════════════════════════════════════
   Section: About
   ═══════════════════════════════════════════════════════════════════ */
static void _draw_section_about(int x, int y, int w) {
    _hdr(x, y, w, "About  VanillaOS");
    int lx = x + P;

    /* logo block */
    gfx_fill_rounded_rect(lx, y + 28, 42, 42, 6, g_theme.accent);
    draw_string(lx + 4,  y + 38, "VOS",    COLOR_WHITE,         COLOR_TRANSPARENT);
    draw_string(lx + 50, y + 30, "VanillaOS", g_theme.text_primary, COLOR_TRANSPARENT);
    draw_string(lx + 50, y + 46, "v0.4.1",    g_theme.accent_light, COLOR_TRANSPARENT);

    _sep(x, y + 78, w);

    _kv(lx, y +  86, "Build Date:",    "2026-06-05",          g_theme.text_primary);
    _kv(lx, y + 102, "Architecture:",  "x86_64 bare-metal",   g_theme.text_primary);
    _kv(lx, y + 118, "Kernel Type:",   "Monolithic / custom", g_theme.text_primary);
    _kv(lx, y + 134, "NIC Support:",   "RTL8139 + PCnet",     g_theme.text_primary);
    _kv(lx, y + 150, "Display:",       "VESA 800x600x32bpp",  g_theme.text_primary);
    _kv(lx, y + 166, "Built by:",      "merdo",               g_theme.text_primary);
    _kv(lx, y + 182, "License:",       "Open Source / hobby", g_theme.text_primary);

    _sep(x, y + 200, w);
    draw_string(lx, y + 208, "github.com/merdo/vos",
                RGB(80, 140, 220), COLOR_TRANSPARENT);
}

/* ═══════════════════════════════════════════════════════════════════
   Main draw callback
   ═══════════════════════════════════════════════════════════════════ */
static void _draw_settings(int x, int y, int w, int h) {
    _content_w = w - SIDEBAR_W;

    /* sidebar */
    draw_rect(x, y, SIDEBAR_W, h, RGB(20, 22, 34));
    draw_line(x + SIDEBAR_W, y, x + SIDEBAR_W, y + h, RGB(44, 48, 68));

    for (int i = 0; i < NUM_SECTIONS; i++) {
        int sy = y + i * SECTION_H + 4;

        if (i == _settings_section)
            gfx_fill_rounded_rect(x + 4, sy, SIDEBAR_W - 8, SECTION_H - 4, 4,
                                  g_theme.accent);

        /* network status dot */
        if (i == SEC_NETWORK) {
            uint32_t dc = !net_up     ? RGB(200, 60, 60)
                        : !net_has_ip ? RGB(210, 165, 40)
                        :               RGB(60,  200, 60);
            gfx_fill_circle(x + SIDEBAR_W - 11, sy + (SECTION_H - 4) / 2, 4, dc);
        }

        uint32_t fg = (i == _settings_section) ? COLOR_WHITE : g_theme.text_secondary;
        draw_string(x + 12, sy + 9, _section_names[i], fg, COLOR_TRANSPARENT);
    }

    int cx = x + SIDEBAR_W, cw = _content_w;
    switch (_settings_section) {
        case SEC_ACCOUNT:    _draw_section_account   (cx, y, cw); break;
        case SEC_NETWORK:    _draw_section_network   (cx, y, cw); break;
        case SEC_LANGUAGE:   _draw_section_language  (cx, y, cw); break;
        case SEC_APPEARANCE: _draw_section_appearance(cx, y, cw); break;
        case SEC_AUTOSTART:  _draw_section_autostart (cx, y, cw); break;
        case SEC_ABOUT:      _draw_section_about     (cx, y, cw); break;
    }
}

/* ═══════════════════════════════════════════════════════════════════
   Click handler
   ═══════════════════════════════════════════════════════════════════ */
void app_settings_handle_click(int rx, int ry) {
    /* ── Sidebar ── */
    if (rx < SIDEBAR_W) {
        int s = ry / SECTION_H;
        if (s >= 0 && s < NUM_SECTIONS) _settings_section = s;
        return;
    }

    int crx = rx - SIDEBAR_W;   /* x relative to content area */

    switch (_settings_section) {

    /* ─────────────── Network ─────────────── */
    case SEC_NETWORK:
        /* Reconnect button: ry 160-180, crx 8-104 */
        if (ry >= 160 && ry < 180 && crx >= P && crx < 104 && net_up) {
            notify_push("Network", "Sending DHCP request...", NOTIFY_INFO);
            int r = dhcp_discover();
            if (r > 0)
                notify_push("Network", "IP obtained!", NOTIFY_INFO);
            else if (r == 0)
                notify_push("Network", "DHCP timeout — no response", NOTIFY_WARN);
            else
                notify_push("Network", "TX failed — NIC not sending", NOTIFY_WARN);
        }
        break;

    /* ─────────────── Language ─────────────── */
    case SEC_LANGUAGE:
        if (ry >= 44 && ry < 62) {
            if (crx >= P && crx < P + 60) {
                keyboard_lang_selector(0);
                notify_push("Settings", "Keyboard layout: EN", NOTIFY_INFO);
            } else if (crx >= P + 70 && crx < P + 130) {
                keyboard_lang_selector(1);
                notify_push("Settings", "Keyboard layout: DE", NOTIFY_INFO);
            }
        }
        break;

    /* ─────────────── Appearance ─────────────── */
    case SEC_APPEARANCE:
        /* sub-tab bar: ry 24-42 */
        if (ry >= 24 && ry < 44) {
            int tw = (_content_w - 16) / NUM_APP_SUBS;
            int i  = (crx - 8) / tw;
            if (i >= 0 && i < NUM_APP_SUBS) _app_sub = i;
            return;
        }
        /* sub-content starts at ry=48 */
        {
            int sy = ry - 48;   /* y relative to sub-section top */
            switch (_app_sub) {

            case APP_COLOR:
                /* theme toggle: sy -2..16  crx 78..248 */
                if (sy >= -2 && sy < 16 && crx >= P + 70 && crx < P + 248) {
                    theme_toggle();
                    notify_push("Settings", "Theme switched!", NOTIFY_INFO);
                }
                /* accent circles: sy 40..60 */
                if (sy >= 40 && sy < 60) {
                    for (int i = 0; i < 6; i++) {
                        int ax = P + i * 28;
                        if (crx >= ax && crx < ax + 20) {
                            theme_set_accent(_accents[i]);
                            notify_push("Settings", "Accent changed!", NOTIFY_INFO);
                        }
                    }
                }
                break;

            case APP_WINDOW:
                if (sy >= -2 && sy < 16 && crx >= P + 150)
                    { _win_rounded = !_win_rounded; notify_push("Settings", _win_rounded ? "Rounded: ON" : "Rounded: OFF", NOTIFY_INFO); }
                if (sy >= 22 && sy < 40 && crx >= P + 150)
                    { _win_shadow  = !_win_shadow;  notify_push("Settings", _win_shadow  ? "Shadow: ON"  : "Shadow: OFF",  NOTIFY_INFO); }
                if (sy >= 46 && sy < 64 && crx >= P + 150)
                    { _win_border  = !_win_border;  notify_push("Settings", _win_border  ? "Border: ON"  : "Border: OFF",  NOTIFY_INFO); }
                break;

            case APP_MOUSE:
                if (sy >= 16 && sy < 34) {
                    for (int i = 0; i < 3; i++) {
                        int bx = P + i * 72;
                        if (crx >= bx && crx < bx + 66) {
                            _cursor_style = i;
                            notify_push("Settings", "Cursor style changed!", NOTIFY_INFO);
                        }
                    }
                }
                break;

            case APP_BG: {
                int sw = 30, gap = 5;
                for (int i = 0; i < 8; i++) {
                    int bx = P + (i % 4) * (sw + gap);
                    int by = 16 + (i / 4) * (sw + gap);
                    if (crx >= bx && crx < bx + sw && sy >= by && sy < by + sw) {
                        _bg_sel = i;
                        notify_push("Settings", "Background changed!", NOTIFY_INFO);
                    }
                }
                break;
            }

            case APP_TRANSP:
                if (sy >= 16 && sy < 34) {
                    for (int i = 0; i < 5; i++) {
                        int bx = P + i * 54;
                        if (crx >= bx && crx < bx + 48) {
                            _transparency = i;
                            notify_push("Settings", "Transparency changed!", NOTIFY_INFO);
                        }
                    }
                }
                break;
            }
        }
        break;

    /* ─────────────── Auto-Start ─────────────── */
    case SEC_AUTOSTART:
        if (ry >= 28 && ry < 46 && crx >= P + 126 && crx < P + 188) {
            _autostart_welcome_ui = !_autostart_welcome_ui;
            app_autostart_welcome = _autostart_welcome_ui;
            notify_push("Auto-Start",
                        _autostart_welcome_ui ? "Welcome App: ON" : "Welcome App: OFF",
                        NOTIFY_INFO);
        }
        break;

    default: break;
    }
}

/* ═══════════════════════════════════════════════════════════════════
   Open
   ═══════════════════════════════════════════════════════════════════ */
void app_settings_open(void) {
    if (win_is_open(_settings_win)) { win_set_focus(_settings_win); return; }
    _settings_win = win_create(70, 35, 460, 330, "Settings",
        WIN_FLAG_DRAGGABLE | WIN_FLAG_CLOSABLE | WIN_FLAG_MINIMIZABLE | WIN_FLAG_RESIZABLE);
    win_set_min_size(_settings_win, 320, 230);
    if (_settings_win >= 0) {
        win_set_content(_settings_win, _draw_settings);
        win_set_click  (_settings_win, app_settings_handle_click);
    }
}