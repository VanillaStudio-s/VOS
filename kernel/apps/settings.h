#ifndef SETTINGS_H
#define SETTINGS_H
#include "kernel.h"

/* Open / handle */
void     app_settings_open(void);
void     app_settings_handle_click(int x, int y);

/* Appearance getters — read by window manager, GFX, cursor driver */
int      settings_get_win_rounded  (void);   /* 1 = rounded corners  */
int      settings_get_win_shadow   (void);   /* 1 = drop shadow      */
int      settings_get_win_border   (void);   /* 1 = accent border    */
int      settings_get_transparency (void);   /* 0=off  1-4=level     */
uint32_t settings_get_bg_color     (void);   /* desktop BG ARGB      */
int      settings_get_cursor_style (void);   /* 0=Arrow 1=Cross 2=Dot*/

#endif