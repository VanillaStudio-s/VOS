#ifndef TRAY_H
#define TRAY_H

#include "kernel.h"
#include "cpu/rtc.h"

#define TRAY_MAX_ITEMS   8
#define TRAY_ITEM_W      64
#define TRAY_ITEM_H      28

typedef void (*TrayDrawFn)(int x, int y);
typedef void (*TrayClickFn)(void);

typedef struct {
    TrayDrawFn  draw;
    TrayClickFn click;
    int         active;
} TrayItem;

void tray_init(void);
int  tray_add(TrayDrawFn draw, TrayClickFn click);
void tray_render(void);
void tray_handle_click(int x, int y);
void tray_tick(RTCTime* t);
int  tray_total_width(void);
int  tray_item_x(int idx);

#endif
