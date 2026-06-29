#ifndef GUI_H
#define GUI_H
#include "drivers/screen.h"
#include "gui/notify.h"
#include "gui/theme.h"

#define GUI_MODE_TERMINAL  0
#define GUI_MODE_DESKTOP   1
#define GUI_MODE_LOCKED    2
#define GUI_MODE_SCREENSAVE 3

extern int gui_mode;

#define TASKBAR_HEIGHT  28
#define START_BTN_W     80
#define START_MENU_W    220
#define START_MENU_H    340

/* legacy compat macros → runtime theme */
#define TASKBAR_COLOR   g_theme.taskbar_bot
#define DESKTOP_BG_COLOR g_theme.desktop_bg
#define START_BTN_COLOR  g_theme.start_btn

void gui_init(void);
void gui_render(void);
void gui_tick(void);
void gui_draw_desktop(void);
void gui_draw_taskbar(void);
void gui_handle_click(int x, int y);
void gui_switch_mode(int mode);
void gui_notify_activity(void);

/* start menu */
void gui_start_menu_key(char c);
int  gui_start_menu_is_open(void);
void gui_start_menu_close(void);
#endif
