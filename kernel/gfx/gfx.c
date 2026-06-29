#include "kernel.h"
#include "drivers/screen.h"
#include "gfx/gfx.h"

uint32_t gfx_blend(uint32_t dst, uint32_t src, uint8_t alpha) {
    uint32_t a  = alpha;
    uint32_t ia = 255 - a;
    uint8_t r = (uint8_t)(((src >> 16 & 0xFF) * a + (dst >> 16 & 0xFF) * ia) >> 8);
    uint8_t g = (uint8_t)(((src >>  8 & 0xFF) * a + (dst >>  8 & 0xFF) * ia) >> 8);
    uint8_t b = (uint8_t)(((src       & 0xFF) * a + (dst       & 0xFF) * ia) >> 8);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void gfx_fill_circle(int cx, int cy, int r, uint32_t color) {
    int x = 0, y = r, d = 1 - r;
    while (x <= y) {
        for (int i = cx-y; i <= cx+y; i++) { put_pixel(i, cy+x, color); put_pixel(i, cy-x, color); }
        for (int i = cx-x; i <= cx+x; i++) { put_pixel(i, cy+y, color); put_pixel(i, cy-y, color); }
        if (d < 0) d += 2*x+3; else { d += 2*(x-y)+5; y--; }
        x++;
    }
}

void gfx_draw_circle(int cx, int cy, int r, uint32_t color) {
    int x = 0, y = r, d = 1 - r;
    while (x <= y) {
        put_pixel(cx+x, cy+y, color); put_pixel(cx-x, cy+y, color);
        put_pixel(cx+x, cy-y, color); put_pixel(cx-x, cy-y, color);
        put_pixel(cx+y, cy+x, color); put_pixel(cx-y, cy+x, color);
        put_pixel(cx+y, cy-x, color); put_pixel(cx-y, cy-x, color);
        if (d < 0) d += 2*x+3; else { d += 2*(x-y)+5; y--; }
        x++;
    }
}

void gfx_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    /* center rectangle */
    draw_rect(x+r, y,   w-2*r, h,   color);
    /* left/right strips */
    draw_rect(x,   y+r, r,     h-2*r, color);
    draw_rect(x+w-r, y+r, r,   h-2*r, color);
    /* four corners */
    gfx_fill_circle(x+r,     y+r,     r, color);
    gfx_fill_circle(x+w-1-r, y+r,     r, color);
    gfx_fill_circle(x+r,     y+h-1-r, r, color);
    gfx_fill_circle(x+w-1-r, y+h-1-r, r, color);
}

void gfx_draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
    /* straight edges */
    draw_line(x+r, y,       x+w-1-r, y,       color);
    draw_line(x+r, y+h-1,   x+w-1-r, y+h-1,   color);
    draw_line(x,   y+r,     x,       y+h-1-r, color);
    draw_line(x+w-1, y+r,   x+w-1,   y+h-1-r, color);
    /* corners */
    gfx_draw_circle(x+r,     y+r,     r, color);
    gfx_draw_circle(x+w-1-r, y+r,     r, color);
    gfx_draw_circle(x+r,     y+h-1-r, r, color);
    gfx_draw_circle(x+w-1-r, y+h-1-r, r, color);
}

void gfx_fill_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    for (int dy = 0; dy < h; dy++)
        for (int dx = 0; dx < w; dx++) {
            uint32_t dst = get_pixel(x+dx, y+dy);
            put_pixel(x+dx, y+dy, gfx_blend(dst, color, alpha));
        }
}

void gfx_gradient_rect(int x, int y, int w, int h,
                        uint32_t c1, uint32_t c2, int vertical) {
    int steps = vertical ? h : w;
    if (!steps) return;
    for (int i = 0; i < steps; i++) {
        uint8_t t = (uint8_t)((i * 255) / (steps - 1 > 0 ? steps-1 : 1));
        uint32_t c = gfx_blend(c1, c2, t);
        if (vertical) draw_rect(x, y+i, w, 1, c);
        else          draw_rect(x+i, y, 1, h, c);
    }
}
