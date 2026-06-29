#include "kernel.h"
#include "drivers/screen.h"
#include "cpu/rtc.h"
#include "gui/gui.h"
#include "gui/tray.h"
#include "gui/net_panel.h"

static TrayItem _items[TRAY_MAX_ITEMS];
static int      _count = 0;
static RTCTime  _clock_time;

/* ── built-in: clock ─────────────────────────────────────────────────────── */
static void _draw_2d(int x, int y, unsigned int n, uint32_t fg) {
    char b[3];
    b[0] = '0' + (n / 10) % 10;
    b[1] = '0' + n % 10;
    b[2] = '\0';
    draw_string(x, y, b, fg, COLOR_TRANSPARENT);
}

static void _tray_draw_clock(int x, int y) {
    int ty = y + 6;
    _draw_2d(x,    ty, _clock_time.hours,   COLOR_WHITE);
    draw_string(x+16, ty, ":", COLOR_WHITE, COLOR_TRANSPARENT);
    _draw_2d(x+24, ty, _clock_time.minutes, COLOR_WHITE);
    draw_string(x+40, ty, ":", COLOR_WHITE, COLOR_TRANSPARENT);
    _draw_2d(x+48, ty, _clock_time.seconds, COLOR_WHITE);
}

static void _tray_click_clock(void) { /* future: open calendar */ }

/* ── built-in: keyboard layout indicator ───────────────────────────────── */
extern int current_layout;

static void _tray_draw_lang(int x, int y) {
    const char* lbl = current_layout == 1 ? "DE" : "EN";
    draw_string(x + 8, y + 6, lbl, RGB(180,220,255), COLOR_TRANSPARENT);
}

static void _tray_click_lang(void) { /* future: toggle layout */ }

/* ── public API ─────────────────────────────────────────────────────────── */
void tray_init(void) {
    _count = 0;
    tray_add(net_tray_draw,    net_tray_click);    /* net icon (leftmost) */
    tray_add(_tray_draw_lang,  _tray_click_lang);
    tray_add(_tray_draw_clock, _tray_click_clock); /* clock (rightmost) */
}

int tray_item_x(int idx) {
    int x = SCREEN_WIDTH - 4;
    for (int i = _count - 1; i >= 0; i--) {
        if (_items[i].active) x -= TRAY_ITEM_W;
        if (i == idx) return x;
    }
    return -1;
}

int tray_add(TrayDrawFn draw, TrayClickFn click) {
    if (_count >= TRAY_MAX_ITEMS) return -1;
    _items[_count].draw   = draw;
    _items[_count].click  = click;
    _items[_count].active = 1;
    return _count++;
}

int tray_total_width(void) {
    int w = 0;
    for (int i = 0; i < _count; i++)
        if (_items[i].active) w += TRAY_ITEM_W;
    return w;
}

void tray_render(void) {
    int x = SCREEN_WIDTH - 4;
    for (int i = _count - 1; i >= 0; i--) {
        if (!_items[i].active) continue;
        x -= TRAY_ITEM_W;
        int ty = SCREEN_HEIGHT - TASKBAR_HEIGHT;
        if (_items[i].draw) _items[i].draw(x, ty);
    }
}

void tray_handle_click(int mx, int my) {
    int x = SCREEN_WIDTH - 4;
    for (int i = _count - 1; i >= 0; i--) {
        if (!_items[i].active) continue;
        x -= TRAY_ITEM_W;
        int ty = SCREEN_HEIGHT - TASKBAR_HEIGHT;
        if (mx >= x && mx < x + TRAY_ITEM_W &&
            my >= ty && my < ty + TASKBAR_HEIGHT) {
            if (_items[i].click) _items[i].click();
            return;
        }
    }
}

void tray_tick(RTCTime* t) {
    _clock_time = *t;
}
