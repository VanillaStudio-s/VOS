#include "kernel.h"
#include "drivers/screen.h"
#include "gfx/gfx.h"
#include "gui/gui.h"
#include "gui/tray.h"
#include "gui/theme.h"
#include "net/net.h"
#include "net/net_init.h"
#include "net/dhcp.h"
#include "gui/net_panel.h"

static int _open = 0;

#define NP_W   240
#define NP_H   170

static int _np_x(void) { return SCREEN_WIDTH  - NP_W - 4; }
static int _np_y(void) { return SCREEN_HEIGHT - TASKBAR_HEIGHT - NP_H - 4; }

/* ── IP-to-string helper ── */
static void _ip2str(IpAddr ip, char *buf) {
    int pos = 0;
    for (int i = 0; i < 4; i++) {
        if (i) buf[pos++] = '.';
        uint8_t v = ip.b[i];
        if (v >= 100) buf[pos++] = (char)('0' + v / 100);
        if (v >=  10) buf[pos++] = (char)('0' + (v / 10) % 10);
        buf[pos++] = (char)('0' + v % 10);
    }
    buf[pos] = '\0';
}

/* ── Tray icon: 4 signal bars ── */
void net_tray_draw(int x, int y) {
    int cx  = x + TRAY_ITEM_W / 2 - 10;
    int bot = y + TASKBAR_HEIGHT - 6;
    int bh[4] = { 4, 7, 10, 13 };

    for (int i = 0; i < 4; i++) {
        uint32_t bc;
        if (!net_up)
            bc = RGB(55, 55, 65);
        else if (!net_has_ip)
            bc = (i < 2) ? g_theme.accent : RGB(55, 55, 65);
        else
            bc = g_theme.accent;
        draw_rect(cx + i * 5, bot - bh[i], 4, bh[i], bc);
    }

    if (!net_up) {
        draw_line(cx,      bot - 14, cx + 18, bot,      RGB(210, 55, 55));
        draw_line(cx + 18, bot - 14, cx,      bot,      RGB(210, 55, 55));
    } else if (!net_has_ip) {
        gfx_fill_circle(cx + 19, bot - 14, 3, RGB(230, 170, 30));
    }
}

void net_tray_click(void)         { _open = 1; }
int  net_panel_is_open(void)      { return _open; }
void net_panel_close(void)        { _open = 0; }
void net_panel_init(void)         { _open = 0; }

/* ── Panel rendering ── */
void net_panel_render(void) {
    if (!_open) return;
    int px = _np_x(), py = _np_y();

    /* shadow */
    draw_rect(px + 3, py + 3, NP_W, NP_H, RGB(4, 4, 8));
    /* body */
    gfx_fill_rounded_rect(px, py, NP_W, NP_H, 6, RGB(22, 24, 36));
    draw_rect_outline(px, py, NP_W, NP_H, RGB(65, 75, 115));

    /* header gradient */
    gfx_gradient_rect(px, py, NP_W, 28, g_theme.accent, g_theme.win_title_active, 1);
    draw_string(px + 10, py + 6, "Network", g_theme.text_primary, COLOR_TRANSPARENT);
    draw_string(px + NP_W - 18, py + 6, "x", RGB(220, 220, 220), COLOR_TRANSPARENT);

    /* ── Ethernet entry ── */
    int iy = py + 34;
    uint32_t ic = net_up ? g_theme.accent : RGB(100, 100, 110);

    /* mini network-layers icon */
    draw_rect_outline(px + 12, iy,      16, 14, ic);
    draw_line(px + 14, iy +  3, px + 26, iy +  3, ic);
    draw_line(px + 14, iy +  7, px + 26, iy +  7, ic);
    draw_line(px + 14, iy + 11, px + 26, iy + 11, ic);

    draw_string(px + 34, iy, "Ethernet", g_theme.text_primary, COLOR_TRANSPARENT);

    /* status dot + text */
    int sy = iy + 18;
    if (net_up) {
        if (net_has_ip) {
            gfx_fill_circle(px + 37, sy + 7, 4, RGB(60, 210, 80));
            draw_string(px + 46, sy, "Connected", RGB(60, 210, 80), COLOR_TRANSPARENT);
        } else {
            gfx_fill_circle(px + 37, sy + 7, 4, RGB(230, 170, 30));
            draw_string(px + 46, sy, "No IP  (DHCP failed)", RGB(230, 170, 30), COLOR_TRANSPARENT);
        }
    } else {
        gfx_fill_circle(px + 37, sy + 7, 4, RGB(210, 55, 55));
        draw_string(px + 46, sy, "No adapter found", RGB(210, 55, 55), COLOR_TRANSPARENT);
    }

    /* separator */
    draw_line(px + 8, py + 70, px + NP_W - 8, py + 70, RGB(45, 50, 80));

    /* ── Detail rows or Reconnect button ── */
    if (net_up && net_has_ip) {
        static char ips[4][16];
        _ip2str(net_ip,      ips[0]);
        _ip2str(net_mask,    ips[1]);
        _ip2str(net_gw,      ips[2]);
        _ip2str(net_dns_srv, ips[3]);

        static const char *lbls[4] = { "IP  :", "Mask:", "GW  :", "DNS :" };
        int lx = px + 14;
        for (int i = 0; i < 4; i++) {
            int ly = py + 78 + i * 18;
            draw_string(lx,      ly, lbls[i], RGB(130, 145, 185), COLOR_TRANSPARENT);
            draw_string(lx + 48, ly, ips[i],  g_theme.text_primary, COLOR_TRANSPARENT);
        }
    } else if (net_up && !net_has_ip) {
        int bx = px + 14, by2 = py + 80;
        gfx_fill_rounded_rect(bx, by2, NP_W - 28, 28, 4, g_theme.accent);
        /* "Reconnect" = 9 chars * 8px = 72px; center in (NP_W-28)=212px */
        draw_string(bx + (NP_W - 28) / 2 - 36, by2 + 6, "Reconnect",
                    COLOR_WHITE, COLOR_TRANSPARENT);
    }
}

/* ── Panel click handler (only handles clicks INSIDE the panel) ── */
int net_panel_handle_click(int mx, int my) {
    if (!_open) return 0;
    int px = _np_x(), py = _np_y();

    if (mx < px || mx >= px + NP_W || my < py || my >= py + NP_H)
        return 0; /* outside panel */

    /* close X button */
    if (mx >= px + NP_W - 22 && my < py + 28) {
        _open = 0;
        return 1;
    }

    /* Reconnect button */
    if (net_up && !net_has_ip) {
        int bx = px + 14, by2 = py + 80;
        if (mx >= bx && mx < bx + NP_W - 28 && my >= by2 && my < by2 + 28) {
            dhcp_discover();
            return 1;
        }
    }

    return 1; /* click inside panel, consumed */
}
