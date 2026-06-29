#include "kernel.h"
#include "cpu/irq.h"
#include "drivers/screen.h"
#include "drivers/mouse.h"
#include "gui/theme.h"
#include "preassets/cursors.h"

extern uint32_t* back_buffer;

static int mouse_x       = 160;
static int mouse_y       = 100;
static int mouse_buttons = 0;
static int cur_type      = CURSOR_ARROW;
static int cur_theme     = MOUSE_THEME_VOS;

/* ── Cursor blit ──────────────────────────────────────────────────────────── */

typedef struct {
    const unsigned char* data;
    int w, h;
} CursorDef;

/* Returns the right bitmap for the current theme + type */
static CursorDef _get_cursor(void) {
    CursorDef d = {(void*)0, 6, 9};

    if (cur_theme == MOUSE_THEME_WINDOWS) {
        switch (cur_type) {
        case CURSOR_RESIZE:  d.data=(const unsigned char*)win_resize;  d.w=WIN_RESIZE_W;  d.h=WIN_RESIZE_H;  break;
        case CURSOR_WAIT:    d.data=(const unsigned char*)win_wait;    d.w=WIN_WAIT_W;    d.h=WIN_WAIT_H;    break;
        case CURSOR_POINTER: d.data=(const unsigned char*)win_pointer; d.w=WIN_POINTER_W; d.h=WIN_POINTER_H; break;
        case CURSOR_TEXT:    d.data=(const unsigned char*)win_text;    d.w=WIN_TEXT_W;    d.h=WIN_TEXT_H;    break;
        default:             d.data=(const unsigned char*)win_arrow;   d.w=WIN_ARROW_W;   d.h=WIN_ARROW_H;   break;
        }
    } else if (cur_theme == MOUSE_THEME_BREEZE) {
        switch (cur_type) {
        case CURSOR_RESIZE:  d.data=(const unsigned char*)brz_resize;  d.w=BRZ_RESIZE_W;  d.h=BRZ_RESIZE_H;  break;
        case CURSOR_WAIT:    d.data=(const unsigned char*)brz_wait;    d.w=BRZ_WAIT_W;    d.h=BRZ_WAIT_H;    break;
        case CURSOR_POINTER: d.data=(const unsigned char*)brz_pointer; d.w=BRZ_POINTER_W; d.h=BRZ_POINTER_H; break;
        case CURSOR_TEXT:    d.data=(const unsigned char*)brz_text;    d.w=BRZ_TEXT_W;    d.h=BRZ_TEXT_H;    break;
        default:             d.data=(const unsigned char*)brz_arrow;   d.w=BRZ_ARROW_W;   d.h=BRZ_ARROW_H;   break;
        }
    } else {
        /* VOS default */
        switch (cur_type) {
        case CURSOR_RESIZE:  d.data=(const unsigned char*)vos_resize;  d.w=VOS_RESIZE_W;  d.h=VOS_RESIZE_H;  break;
        case CURSOR_WAIT:    d.data=(const unsigned char*)vos_wait;    d.w=VOS_WAIT_W;    d.h=VOS_WAIT_H;    break;
        case CURSOR_POINTER: d.data=(const unsigned char*)vos_pointer; d.w=VOS_POINTER_W; d.h=VOS_POINTER_H; break;
        case CURSOR_TEXT:    d.data=(const unsigned char*)vos_text;    d.w=VOS_TEXT_W;    d.h=VOS_TEXT_H;    break;
        default:             d.data=(const unsigned char*)vos_arrow;   d.w=VOS_ARROW_W;   d.h=VOS_ARROW_H;   break;
        }
    }
    return d;
}

static void cursor_blit(void) {
    if (!back_buffer) return;
    CursorDef cd = _get_cursor();
    if (!cd.data) return;

    uint32_t accent = g_theme.accent;  /* used for value=3 (Breeze tinted pixels) */

    for (int dy = 0; dy < cd.h; dy++) {
        for (int dx = 0; dx < cd.w; dx++) {
            int px = mouse_x + dx, py = mouse_y + dy;
            if ((unsigned)px >= SCREEN_WIDTH || (unsigned)py >= SCREEN_HEIGHT) continue;
            unsigned char b = cd.data[dy * cd.w + dx];
            uint32_t col;
            switch (b) {
            case 1: col = COLOR_BLACK;  break;
            case 2: col = COLOR_WHITE;  break;
            case 3: col = accent;       break;
            default: continue;          /* 0 = transparent */
            }
            back_buffer[py * SCREEN_WIDTH + px] = col;
        }
    }
}

/* ── Public cursor API ───────────────────────────────────────────────────── */
void mouse_set_cursor(int type)  { cur_type  = type;  }
int  mouse_get_cursor(void)      { return cur_type;   }
void mouse_set_theme(int theme)  { cur_theme = theme; }
int  mouse_get_theme(void)       { return cur_theme;  }
void mouse_draw_cursor(void)     { cursor_blit();     }
void mouse_erase_cursor(void)    { /* no-op */        }
void mouse_blit_cursor_to_vga(void) { cursor_blit(); }

/* ── IRQ packet handler ──────────────────────────────────────────────────── */
/*
 * MOUSE FIX: The old warp_guard/warp_streak code stopped the cursor entirely
 * when the physical mouse was moved fast, causing the freeze the user noticed.
 *
 * New approach: smooth speed cap.
 * - Packets with overflow bits set (0xC0) are still rejected (HW glitch).
 * - All valid deltas are clamped to MOUSE_MAX_DELTA — no freezing, no jumping.
 */
#define MOUSE_MAX_DELTA 55

static unsigned char pkt[3];
static int cycle = 0;

static void mouse_irq(Registers* r) {
    (void)r;
    if (!(inb(0x64) & 0x01)) return;

    pkt[cycle++] = inb(0x60);
    if (cycle < 3) return;
    cycle = 0;

    /* validate packet */
    if (!(pkt[0] & 0x08)) return;   /* always-1 bit missing → desync */
    if (pkt[0] & 0xC0)    return;   /* overflow → hardware glitch, skip */

    mouse_buttons = pkt[0] & 0x07;

    int dx = (int)(signed char)pkt[1];
    int dy = (int)(signed char)pkt[2];

    /* smooth clamp — cursor always moves, just not too fast */
    if (dx >  MOUSE_MAX_DELTA) dx =  MOUSE_MAX_DELTA;
    if (dx < -MOUSE_MAX_DELTA) dx = -MOUSE_MAX_DELTA;
    if (dy >  MOUSE_MAX_DELTA) dy =  MOUSE_MAX_DELTA;
    if (dy < -MOUSE_MAX_DELTA) dy = -MOUSE_MAX_DELTA;

    mouse_x += dx;
    mouse_y -= dy;   /* Y axis is inverted */

    if (mouse_x < 0)              mouse_x = 0;
    if (mouse_x >= SCREEN_WIDTH)  mouse_x = SCREEN_WIDTH  - 1;
    if (mouse_y < 0)              mouse_y = 0;
    if (mouse_y >= SCREEN_HEIGHT) mouse_y = SCREEN_HEIGHT - 1;
}

/* ── Init helpers ────────────────────────────────────────────────────────── */
static void mww(void) { int t=100000; while(t--) if(!(inb(0x64)&0x02)) return; }
static void mwr(void) { int t=100000; while(t--) if( inb(0x64)&0x01)  return; }
static void mw(unsigned char d) { mww(); outb(0x64,0xD4); mww(); outb(0x60,d); }
static unsigned char mr(void)   { mwr(); return inb(0x60); }

void mouse_init(void) {
    mww(); outb(0x64, 0xA8);
    mww(); outb(0x64, 0x20); mwr();
    unsigned char s = (inb(0x60) | 0x02) & ~0x20;
    mww(); outb(0x64, 0x60); mww(); outb(0x60, s);
    mw(0xFF); mr(); mr(); mr();
    mw(0xF6); mr();
    mw(0xF4); mr();
    cycle = 0;
    irq_register(12, mouse_irq);
}

int mouse_get_x(void)       { return mouse_x;       }
int mouse_get_y(void)       { return mouse_y;       }
int mouse_get_buttons(void) { return mouse_buttons; }