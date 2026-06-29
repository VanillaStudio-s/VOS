#include "kernel.h"
#include "drivers/screen.h"
#include "cpu/rtc.h"
#include "gfx/gfx.h"
#include "gui/theme.h"
#include "sys/user.h"

extern int active_user_idx;

static char _pw_buf[32];
static int  _pw_len = 0;
static void lockscreen_reset(void);

/* draw two-digit number without draw_string to avoid pulling in full term */
static void _draw_big_2d(int x, int y, unsigned int n, uint32_t fg) {
    char b[3]; b[0]='0'+(n/10)%10; b[1]='0'+n%10; b[2]='\0';
    draw_string(x, y, b, fg, COLOR_TRANSPARENT);
}

void lockscreen_render(void) {
    /* dark blurred overlay */
    gfx_fill_rect_alpha(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGB(5,8,15), 230);

    /* center panel */
    int pw = 280, ph = 180;
    int px = (SCREEN_WIDTH  - pw) / 2;
    int py = (SCREEN_HEIGHT - ph) / 2;
    gfx_fill_rounded_rect(px, py, pw, ph, 10, RGB(20,22,35));
    draw_rect_outline(px, py, pw, ph, g_theme.accent);

    /* clock large */
    RTCTime t; rtc_read(&t);
    int cx = px + pw/2 - 40;
    _draw_big_2d(cx,    py+20, t.hours,   COLOR_WHITE);
    draw_string(cx+16, py+20, ":", COLOR_WHITE, COLOR_TRANSPARENT);
    _draw_big_2d(cx+24, py+20, t.minutes, COLOR_WHITE);
    draw_string(cx+40, py+20, ":", COLOR_WHITE, COLOR_TRANSPARENT);
    _draw_big_2d(cx+48, py+20, t.seconds, COLOR_WHITE);

    /* user label */
    draw_string(px+10, py+60, user_database[active_user_idx],
                g_theme.text_secondary, COLOR_TRANSPARENT);
    draw_string(px+10, py+80, "Enter password to unlock",
                RGB(160,160,175), COLOR_TRANSPARENT);

    /* password input box */
    draw_rect(px+10, py+110, pw-20, 22, RGB(35,38,55));
    draw_rect_outline(px+10, py+110, pw-20, 22, g_theme.accent);

    /* show dots for entered chars */
    for (int i = 0; i < _pw_len && i < 20; i++)
        gfx_fill_circle(px + 18 + i*11, py+121, 4, COLOR_WHITE);

    screen_flip();
}

int lockscreen_input(char c) {
    if (c == '\n') {
        _pw_buf[_pw_len] = '\0';
        int ok = my_strcmp(_pw_buf, pass_database[active_user_idx]) == 0;
        lockscreen_reset();
        return ok ? 1 : -1;   /* 1=unlocked, -1=wrong */
    } else if (c == '\b') {
        if (_pw_len > 0) _pw_len--;
    } else if (_pw_len < 31) {
        _pw_buf[_pw_len++] = c;
    }
    return 0;
}

void lockscreen_reset(void) {
    for (int i = 0; i < 32; i++) _pw_buf[i] = 0;
    _pw_len = 0;
}
