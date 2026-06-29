#ifndef GFX_H
#define GFX_H

#include "kernel.h"

/* Alpha blend two 0xRRGGBB colors. alpha 0=transparent, 255=opaque */
uint32_t gfx_blend(uint32_t dst, uint32_t src, uint8_t alpha);

/* Filled & outline circles */
void gfx_fill_circle(int cx, int cy, int r, uint32_t color);
void gfx_draw_circle(int cx, int cy, int r, uint32_t color);

/* Rounded rectangle (fill + optional outline) */
void gfx_fill_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);
void gfx_draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color);

/* Translucent filled rectangle */
void gfx_fill_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha);

/* Vertical or horizontal gradient fill */
void gfx_gradient_rect(int x, int y, int w, int h,
                        uint32_t c1, uint32_t c2, int vertical);

#endif
