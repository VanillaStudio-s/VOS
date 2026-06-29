#ifndef THEME_H
#define THEME_H
#include "kernel.h"


// TERMINAL
#define COLOR_PROMPT   RGB(80, 200, 120)  // LIME
#define COLOR_INPUT    RGB(255, 255, 255) // WHITE
#define COLOR_ERROR    RGB(255, 80, 80)   // RED
#define COLOR_SUCCESS  RGB(100, 200, 255) // BLUE


typedef struct {
    uint32_t desktop_bg;
    uint32_t taskbar_top;
    uint32_t taskbar_bot;
    uint32_t taskbar_border;
    uint32_t start_btn;
    uint32_t start_btn_hover;
    uint32_t win_bg;
    uint32_t win_title_active;
    uint32_t win_title_inactive;
    uint32_t win_border_active;
    uint32_t win_border_inactive;
    uint32_t text_primary;
    uint32_t text_secondary;
    uint32_t accent;
    uint32_t accent_light;
} Theme;

extern Theme g_theme;

typedef enum { THEME_DARK = 0, THEME_LIGHT = 1 } ThemeMode;
extern ThemeMode g_theme_mode;

void theme_apply_dark(void);
void theme_apply_light(void);
void theme_set_accent(uint32_t color);
void theme_toggle(void);

#endif
