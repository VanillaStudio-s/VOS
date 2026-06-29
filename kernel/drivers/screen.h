#ifndef SCREEN_H
#define SCREEN_H

#include "kernel.h"

#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768
#define SCREEN_BPP    4
#define SCREEN_PITCH  (SCREEN_WIDTH * SCREEN_BPP)

/* Backbuffer at 2MB — safely past kernel code */
#define BACKBUFFER_ADDR  0x200000

#define RGB(r,g,b)  ((uint32_t)(((r)<<16)|((g)<<8)|(b)))

#define COLOR_BLACK       RGB(0,   0,   0  )
#define COLOR_BLUE        RGB(0,   0,   170)
#define COLOR_GREEN       RGB(0,   170, 0  )
#define COLOR_CYAN        RGB(0,   170, 170)
#define COLOR_RED         RGB(170, 0,   0  )
#define COLOR_MAGENTA     RGB(170, 0,   170)
#define COLOR_BROWN       RGB(170, 85,  0  )
#define COLOR_LIGHT_GRAY  RGB(170, 170, 170)
#define COLOR_DARK_GRAY   RGB(85,  85,  85 )
#define COLOR_LIGHT_BLUE  RGB(85,  85,  255)
#define COLOR_LIGHT_GREEN RGB(85,  255, 85 )
#define COLOR_LIGHT_CYAN  RGB(85,  255, 255)
#define COLOR_LIGHT_RED   RGB(255, 85,  85 )
#define COLOR_PINK        RGB(255, 85,  255)
#define COLOR_YELLOW      RGB(255, 255, 85 )
#define COLOR_WHITE       RGB(255, 255, 255)
#define COLOR_TRANSPARENT 0xFFFFFFFF

extern uint32_t* back_buffer;

uint32_t* screen_get_framebuffer(void);

/* fb_addr comes from multiboot2 info (parsed in kernel.c) */
void screen_init(uint64_t fb_addr);

void screen_set_clip(int x, int y, int w, int h);
void screen_clear_clip(void);

void put_pixel(int x, int y, uint32_t color);
uint32_t get_pixel(int x, int y);

void draw_rect(int x, int y, int w, int h, uint32_t color);
void draw_rect_outline(int x, int y, int w, int h, uint32_t color);
void draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void draw_char(int x, int y, char c, uint32_t color, uint32_t bg);
void draw_string(int x, int y, const char* str, uint32_t color, uint32_t bg);

void clear_screen(uint32_t color);
void screen_flip(void);

void my_clear_screen(void);
void my_putchar(char c);
void my_putchar_color(char c, unsigned char color_idx);
void my_puts(const char* str);
void my_puts_color(const char* str, unsigned char color_idx);
void my_print_int(int num);
int  my_strcmp(const char* s1, const char* s2);
void screen_update(void);

#endif
