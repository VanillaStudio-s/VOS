#include "kernel.h"
#include "drivers/screen.h"
#include "gfx/gfx.h"
#include "gui/gui.h"
#include "gui/notify.h"

typedef struct {
    char  title[32];
    char  body[64];
    NotifyKind kind;
    int   ttl;    /* ticks remaining */
    int   active;
} Notification;

static Notification _notifs[NOTIFY_MAX];

static const uint32_t _kind_color[4] = {
    /* INFO    */ 0x1E6BB8,
    /* SUCCESS */ 0x1E8A44,
    /* WARN    */ 0xB87A1E,
    /* ERROR   */ 0xB82020,
};

static void my_strncpy(char* dst, const char* src, int max) {
    int i = 0;
    while (i < max-1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

void notify_push(const char* title, const char* body, NotifyKind kind) {
    /* find a free slot, or evict oldest */
    int slot = -1;
    for (int i = 0; i < NOTIFY_MAX; i++)
        if (!_notifs[i].active) { slot = i; break; }
    if (slot < 0) {
        /* evict the one with least ttl */
        int min_ttl = _notifs[0].ttl; slot = 0;
        for (int i = 1; i < NOTIFY_MAX; i++)
            if (_notifs[i].ttl < min_ttl) { min_ttl = _notifs[i].ttl; slot = i; }
    }
    my_strncpy(_notifs[slot].title, title ? title : "", 32);
    my_strncpy(_notifs[slot].body,  body  ? body  : "", 64);
    _notifs[slot].kind   = kind;
    _notifs[slot].ttl    = NOTIFY_LIFETIME;
    _notifs[slot].active = 1;
}

void notify_tick(void) {
    for (int i = 0; i < NOTIFY_MAX; i++) {
        if (!_notifs[i].active) continue;
        if (--_notifs[i].ttl <= 0) _notifs[i].active = 0;
    }
}

int notify_any_active(void) {
    for (int i = 0; i < NOTIFY_MAX; i++)
        if (_notifs[i].active) return 1;
    return 0;
}

void notify_render(void) {
    /* Stack notifications from bottom-right, above taskbar */
    int base_x = SCREEN_WIDTH  - NOTIFY_W - NOTIFY_MARGIN;
    int base_y = SCREEN_HEIGHT - TASKBAR_HEIGHT - NOTIFY_MARGIN;

    int drawn = 0;
    for (int i = 0; i < NOTIFY_MAX; i++) {
        if (!_notifs[i].active) continue;

        int nx = base_x;
        int ny = base_y - drawn * (NOTIFY_ITEM_H + NOTIFY_MARGIN) - NOTIFY_ITEM_H;

        /* shadow */
        gfx_fill_rect_alpha(nx+4, ny+4, NOTIFY_W, NOTIFY_ITEM_H, 0x000000, 120);

        /* background */
        gfx_fill_rounded_rect(nx, ny, NOTIFY_W, NOTIFY_ITEM_H, 6, RGB(30,30,40));
        draw_rect_outline(nx, ny, NOTIFY_W, NOTIFY_ITEM_H, RGB(70,70,90));

        /* left accent bar */
        uint32_t accent = _kind_color[_notifs[i].kind & 3];
        draw_rect(nx, ny, 4, NOTIFY_ITEM_H, accent);

        /* title */
        draw_string(nx+10, ny+8,  _notifs[i].title, COLOR_WHITE, COLOR_TRANSPARENT);
        /* body */
        draw_string(nx+10, ny+26, _notifs[i].body,  RGB(200,200,200), COLOR_TRANSPARENT);

        /* fade bar: progress indicator showing remaining time */
        int bar_w = (NOTIFY_W - 8) * _notifs[i].ttl / NOTIFY_LIFETIME;
        if (bar_w > 0)
            draw_rect(nx+4, ny+NOTIFY_ITEM_H-3, bar_w, 2, accent);

        drawn++;
    }
}
