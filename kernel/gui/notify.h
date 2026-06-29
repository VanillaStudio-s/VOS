#ifndef NOTIFY_H
#define NOTIFY_H

#include "kernel.h"

#define NOTIFY_MAX       4
#define NOTIFY_W         260
#define NOTIFY_ITEM_H    52
#define NOTIFY_MARGIN    8
#define NOTIFY_LIFETIME  300   /* ticks (~3 seconds at 100 ticks/s) */

typedef enum {
    NOTIFY_INFO    = 0,
    NOTIFY_SUCCESS = 1,
    NOTIFY_WARN    = 2,
    NOTIFY_ERROR   = 3
} NotifyKind;

void notify_push(const char* title, const char* body, NotifyKind kind);
void notify_tick(void);
void notify_render(void);
int  notify_any_active(void);

#endif
