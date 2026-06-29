#ifndef WINDOW_H
#define WINDOW_H
#include "drivers/screen.h"

#define MAX_WINDOWS    8
#define MAX_WORKSPACES 4

#define WIN_FLAG_DRAGGABLE   0x01
#define WIN_FLAG_CLOSABLE    0x02
#define WIN_FLAG_FOCUSED     0x04
#define WIN_FLAG_RESIZABLE   0x08
#define WIN_FLAG_MINIMIZABLE 0x10

typedef void (*WinContentFn)(int x, int y, int w, int h);
typedef void (*WinClickFn)(int rx, int ry);
typedef void (*WinRightClickFn)(int rx, int ry);
typedef void (*WinKeyFn)(char c, unsigned char sc, int ctrl);

typedef struct {
    int x, y, w, h;
    int min_w, min_h;
    char title[32];
    uint32_t bg_color, title_color, border_color;
    unsigned char flags;
    int visible, z_index;
    int workspace;
    int minimized;
    WinContentFn    content_fn;
    WinClickFn      click_fn;
    WinRightClickFn right_click_fn;
    WinKeyFn        key_fn;
} Window;

void win_init(void);
int  win_create(int x, int y, int w, int h, const char* title, unsigned char flags);
void win_set_content(int id, WinContentFn fn);
void win_set_click(int id, WinClickFn fn);
void win_set_right_click(int id, WinRightClickFn fn);
void win_set_key(int id, WinKeyFn fn);
void win_send_key(char c, unsigned char sc, int ctrl);
int  win_get_focused(void);
void win_close(int id);
int  win_is_open(int id);
int  win_is_minimized(int id);
void win_set_focus(int id);
void win_minimize(int id);
void win_restore(int id);
void win_move(int id, int x, int y);
void win_render_all(void);
int  win_handle_mouse(int mx, int my, int buttons);

void win_set_min_size(int id, int min_w, int min_h); 

int  win_get_workspace(void);
void win_switch_workspace(int n);
void win_set_workspace(int id, int ws);

/* taskbar integration */
void win_taskbar_render(int x, int y, int aw, int ah);
int  win_taskbar_click(int mx, int my, int base_x, int base_y, int aw, int ah);

/* query open window title; returns 1 if open, 0 if closed/invalid */
int  win_get_title(int id, char* out, int maxlen);

#endif
