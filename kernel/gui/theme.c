#include "kernel.h"
#include "drivers/screen.h"
#include "gui/theme.h"



Theme     g_theme;
ThemeMode g_theme_mode = THEME_DARK;

void theme_apply_dark(void) {
    g_theme_mode = THEME_DARK;
    g_theme.desktop_bg          = RGB( 10,  80, 160);
    g_theme.taskbar_top         = RGB( 20,  20,  30);
    g_theme.taskbar_bot         = RGB( 40,  50,  70);
    g_theme.taskbar_border      = RGB( 60,  80, 120);
    g_theme.start_btn           = RGB(  0, 100, 200);
    g_theme.start_btn_hover     = RGB(  0, 130, 230);
    g_theme.win_bg              = RGB( 30,  30,  42);
    g_theme.win_title_active    = RGB(  0, 100, 200);
    g_theme.win_title_inactive  = RGB( 70,  70,  80);
    g_theme.win_border_active   = RGB( 80, 120, 200);
    g_theme.win_border_inactive = RGB( 60,  60,  80);
    g_theme.text_primary        = RGB(255, 255, 255);
    g_theme.text_secondary      = RGB(180, 180, 180);
    g_theme.accent              = RGB(  0, 120, 215);
    g_theme.accent_light        = RGB( 64, 168, 255);
}

void theme_apply_light(void) {
    g_theme_mode = THEME_LIGHT;
    g_theme.desktop_bg          = RGB(200, 220, 240);
    g_theme.taskbar_top         = RGB(235, 235, 240);
    g_theme.taskbar_bot         = RGB(210, 215, 225);
    g_theme.taskbar_border      = RGB(160, 170, 190);
    g_theme.start_btn           = RGB(  0, 120, 215);
    g_theme.start_btn_hover     = RGB(  0, 150, 255);
    g_theme.win_bg              = RGB(245, 245, 248);
    g_theme.win_title_active    = RGB(  0, 120, 215);
    g_theme.win_title_inactive  = RGB(180, 185, 195);
    g_theme.win_border_active   = RGB(  0, 120, 215);
    g_theme.win_border_inactive = RGB(180, 185, 195);
    g_theme.text_primary        = RGB( 20,  20,  20);
    g_theme.text_secondary      = RGB( 90,  90,  90);
    g_theme.accent              = RGB(  0, 120, 215);
    g_theme.accent_light        = RGB( 64, 168, 255);
}

void theme_set_accent(uint32_t color) {
    g_theme.accent       = color;
    uint8_t r = (uint8_t)((color >> 16) & 0xFF);
    uint8_t g = (uint8_t)((color >>  8) & 0xFF);
    uint8_t b = (uint8_t)( color        & 0xFF);
    /* lighter variant: blend toward white */
    g_theme.accent_light = RGB(
        (uint8_t)(r + (255-r)/2),
        (uint8_t)(g + (255-g)/2),
        (uint8_t)(b + (255-b)/2)
    );
    /* also update start button */
    g_theme.start_btn       = color;
    g_theme.start_btn_hover = g_theme.accent_light;
    g_theme.win_border_active   = color;
    g_theme.win_title_active    = color;
}

void theme_toggle(void) {
    if (g_theme_mode == THEME_DARK) theme_apply_light();
    else                            theme_apply_dark();
}
