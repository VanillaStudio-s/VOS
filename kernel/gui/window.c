#include "kernel.h"
#include "gui/window.h"
#include "gui/theme.h"
#include "drivers/screen.h"
#include "drivers/mouse.h"
#include "gfx/gfx.h"

static Window pool[MAX_WINDOWS];
static int count = 0, z_gen = 0, focused = -1;
static int drag_id = -1, drag_ox = 0, drag_oy = 0;
static int resize_id = -1, resize_ox = 0, resize_oy = 0, resize_ow = 0, resize_oh = 0;
static int current_ws = 0;

#define TH  24   /* titlebar height */
#define CW  20   /* close/minimize button width */
#define CH  16   /* close/minimize button height */

/* ── init / create ────────────────────────────────────────────────────────── */

void win_init(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        pool[i].visible         = 0;
        pool[i].minimized       = 0;
        pool[i].min_w           = 120;
        pool[i].min_h           =  60;
        pool[i].z_index         = 0;
        pool[i].content_fn      = 0;
        pool[i].click_fn        = 0;
        pool[i].right_click_fn  = 0;
        pool[i].key_fn          = 0;
        pool[i].workspace       = 0;
    }
    count = z_gen = 0; focused = drag_id = resize_id = -1; current_ws = 0;
}

int win_create(int x, int y, int w, int h, const char* title, unsigned char flags) {
    if (count >= MAX_WINDOWS) return -1;
    int id = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) if (!pool[i].visible) { id = i; break; }
    if (id < 0) return -1;
    Window* win = &pool[id];
    win->x = x; win->y = y; win->w = w; win->h = h;
    int i;
    for (i = 0; i < 31 && title[i]; i++) win->title[i] = title[i];
    win->title[i] = '\0';
    win->min_w = 120;
    win->min_h =  60;
    win->bg_color     = g_theme.win_bg;
    win->title_color  = g_theme.text_primary;
    win->border_color = g_theme.win_border_active;
    win->flags        = flags | WIN_FLAG_FOCUSED;
    win->visible      = 1;
    win->minimized    = 0;
    win->z_index      = z_gen++;
    win->content_fn      = 0;
    win->click_fn        = 0;
    win->right_click_fn  = 0;
    win->key_fn          = 0;
    win->workspace       = current_ws;
    count++; win_set_focus(id); return id;
}

/* ── property setters ─────────────────────────────────────────────────────── */

void win_set_content(int id, WinContentFn fn) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    pool[id].content_fn = fn;
}

void win_set_click(int id, WinClickFn fn) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    pool[id].click_fn = fn;
}

void win_set_right_click(int id, WinRightClickFn fn) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    pool[id].right_click_fn = fn;
}

void win_set_key(int id, WinKeyFn fn) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    pool[id].key_fn = fn;
}

void win_set_min_size(int id, int min_w, int min_h) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    pool[id].min_w = min_w;
    pool[id].min_h = min_h;
}

void win_send_key(char c, unsigned char sc, int ctrl) {
    if (focused >= 0 && focused < MAX_WINDOWS &&
        pool[focused].visible && !pool[focused].minimized && pool[focused].key_fn)
        pool[focused].key_fn(c, sc, ctrl);
}

int win_get_focused(void) { return focused; }

void win_close(int id) {
    if (id < 0 || id >= MAX_WINDOWS || !pool[id].visible) return;
    pool[id].visible = 0; pool[id].minimized = 0; count--;
    if (focused == id) focused = -1;
}

int win_is_open(int id) {
    if (id < 0 || id >= MAX_WINDOWS) return 0;
    return pool[id].visible;
}

int win_is_minimized(int id) {
    if (id < 0 || id >= MAX_WINDOWS) return 0;
    return pool[id].minimized;
}

/* ── focus / move ─────────────────────────────────────────────────────────── */

void win_set_focus(int id) {
    if (id < 0 || id >= MAX_WINDOWS || !pool[id].visible || pool[id].minimized) return;
    for (int i = 0; i < MAX_WINDOWS; i++) pool[i].flags &= ~WIN_FLAG_FOCUSED;
    pool[id].flags |= WIN_FLAG_FOCUSED;
    pool[id].z_index = z_gen++;
    focused = id;
}

void win_move(int id, int x, int y) {
    if (id < 0 || id >= MAX_WINDOWS || !pool[id].visible) return;
    pool[id].x = x; pool[id].y = y;
}

/* ── minimize / restore ───────────────────────────────────────────────────── */

void win_minimize(int id) {
    if (id < 0 || id >= MAX_WINDOWS || !pool[id].visible || pool[id].minimized) return;
    pool[id].minimized = 1;
    if (focused == id) {
        focused = -1;
        for (int i = MAX_WINDOWS-1; i >= 0; i--) {
            if (pool[i].visible && !pool[i].minimized && pool[i].workspace == current_ws) {
                win_set_focus(i); break;
            }
        }
    }
}

void win_restore(int id) {
    if (id < 0 || id >= MAX_WINDOWS || !pool[id].visible) return;
    pool[id].minimized = 0;
    win_set_focus(id);
}

/* ── workspace ────────────────────────────────────────────────────────────── */

int  win_get_workspace(void)           { return current_ws; }
void win_set_workspace(int id, int ws) {
    if (id < 0 || id >= MAX_WINDOWS) return;
    pool[id].workspace = ws;
}

void win_switch_workspace(int n) {
    if (n < 0) n = MAX_WORKSPACES - 1;
    if (n >= MAX_WORKSPACES) n = 0;
    current_ws = n;
    focused = -1;
    for (int i = MAX_WINDOWS-1; i >= 0; i--)
        if (pool[i].visible && !pool[i].minimized && pool[i].workspace == current_ws)
            { win_set_focus(i); break; }
}

/* ── drawing ──────────────────────────────────────────────────────────────── */

static void draw_frame(Window* w) {
    int foc = (w->flags & WIN_FLAG_FOCUSED);
    /* shadow */
    draw_rect(w->x+4, w->y+4, w->w, w->h, RGB(10,10,10));
    /* titlebar gradient */
    uint32_t tb = foc ? g_theme.win_title_active : g_theme.win_title_inactive;
    gfx_gradient_rect(w->x, w->y, w->w, TH, tb,
                      foc ? RGB(0,70,140) : RGB(50,50,60), 1);
    draw_string(w->x+8, w->y+4, w->title, g_theme.text_primary, COLOR_TRANSPARENT);
    /* close button */
    if (w->flags & WIN_FLAG_CLOSABLE) {
        int bx = w->x+w->w-CW-4, by = w->y+4;
        gfx_fill_rounded_rect(bx, by, CW, CH, 3, RGB(200,50,50));
        draw_string(bx+6, by+2, "X", COLOR_WHITE, COLOR_TRANSPARENT);
    }
    /* minimize button */
    if (w->flags & WIN_FLAG_MINIMIZABLE) {
        int mbx = w->x+w->w-2*(CW+4), mby = w->y+4;
        gfx_fill_rounded_rect(mbx, mby, CW, CH, 3, RGB(60,110,200));
        draw_string(mbx+6, mby+2, "_", COLOR_WHITE, COLOR_TRANSPARENT);
    }
    /* client area */
    draw_rect(w->x, w->y+TH, w->w, w->h-TH, g_theme.win_bg);
    /* border */
    uint32_t bc = foc ? g_theme.win_border_active : g_theme.win_border_inactive;
    draw_rect_outline(w->x, w->y, w->w, w->h, bc);
    /* resize gripper (bottom-right corner, 6-dot triangle pattern) */
    if (w->flags & WIN_FLAG_RESIZABLE) {
        uint32_t gc = RGB(70,74,100);
        int gx = w->x + w->w, gy = w->y + w->h;
        draw_rect(gx-4,  gy-4,  2, 2, gc);
        draw_rect(gx-8,  gy-4,  2, 2, gc);
        draw_rect(gx-4,  gy-8,  2, 2, gc);
        draw_rect(gx-12, gy-4,  2, 2, gc);
        draw_rect(gx-8,  gy-8,  2, 2, gc);
        draw_rect(gx-4,  gy-12, 2, 2, gc);
    }
    /* content callback — clipped to client area so resized windows can't bleed */
    if (w->content_fn) {
        screen_set_clip(w->x+1, w->y+TH, w->w-2, w->h-TH-1);
        w->content_fn(w->x, w->y + TH, w->w, w->h - TH);
        screen_clear_clip();
    }
    
}

void win_render_all(void) {
    /* sort an index array by z_index — does NOT move pool entries, preserving IDs */
    int order[MAX_WINDOWS];
    for (int i = 0; i < MAX_WINDOWS; i++) order[i] = i;
    for (int i = 0; i < MAX_WINDOWS-1; i++)
        for (int j = 0; j < MAX_WINDOWS-i-1; j++)
            if (pool[order[j]].z_index > pool[order[j+1]].z_index) {
                int tmp = order[j]; order[j] = order[j+1]; order[j+1] = tmp;
            }
    for (int i = 0; i < MAX_WINDOWS; i++) {
        int id = order[i];
        if (pool[id].visible && !pool[id].minimized && pool[id].workspace == current_ws)
            draw_frame(&pool[id]);
    }
}

/* ── mouse input ──────────────────────────────────────────────────────────── */

int win_handle_mouse(int mx, int my, int buttons) {
    static int last_btn = 0;
    int fresh       = (buttons & 1) && !(last_btn & 1);
    int fresh_right = (buttons & 2) && !(last_btn & 2);
    last_btn = buttons;

    /* Z-sortierter Index: order[0] = oberstes Fenster (höchster z_index) */
    int order[MAX_WINDOWS];
    for (int i = 0; i < MAX_WINDOWS; i++) order[i] = i;
    for (int i = 0; i < MAX_WINDOWS-1; i++)
        for (int j = 0; j < MAX_WINDOWS-i-1; j++)
            if (pool[order[j]].z_index < pool[order[j+1]].z_index) {
                int tmp = order[j]; order[j] = order[j+1]; order[j+1] = tmp;
            }

    /* Cursor-Form: Resize-Cursor im Resize-Bereich, sonst Pfeil */
    {
        int want_resize = (resize_id >= 0);
        if (!want_resize) {
            for (int k = 0; k < MAX_WINDOWS; k++) {
                Window* w = &pool[order[k]];
                if (!w->visible || w->minimized || w->workspace != current_ws) continue;
                if (!(w->flags & WIN_FLAG_RESIZABLE)) continue;
                if (mx >= w->x+w->w-14 && mx < w->x+w->w &&
                    my >= w->y+w->h-14 && my < w->y+w->h) {
                    want_resize = 1; break;
                }
            }
        }
        mouse_set_cursor(want_resize ? CURSOR_RESIZE : CURSOR_ARROW);
    }

    /* Rechtsklick — oberstes Fenster zuerst */
    if (fresh_right) {
        for (int k = 0; k < MAX_WINDOWS; k++) {
            int i = order[k];
            Window* w = &pool[i];
            if (!w->visible || w->minimized || w->workspace != current_ws) continue;
            if (mx >= w->x && mx < w->x+w->w &&
                my >= w->y+TH && my < w->y+w->h) {
                win_set_focus(i);
                if (w->right_click_fn)
                    w->right_click_fn(mx - w->x, my - (w->y + TH));
                return 1;
            }
        }
    }

    if (!(buttons & 1)) { drag_id = -1; resize_id = -1; return 0; }

    /* Aktives Drag */
    if (drag_id >= 0)   { win_move(drag_id, mx-drag_ox, my-drag_oy); return 1; }

    /* Aktives Resize */
    if (resize_id >= 0) {
        int nw = resize_ow + (mx - resize_ox);
        int nh = resize_oh + (my - resize_oy);
        if (nw < pool[resize_id].min_w) nw = pool[resize_id].min_w;
        if (nh < pool[resize_id].min_h) nh = pool[resize_id].min_h;
        pool[resize_id].w = nw;
        pool[resize_id].h = nh;
        return 1;
    }

    if (fresh) {
        /* Schließen / Minimieren — oberstes Fenster zuerst */
        for (int k = 0; k < MAX_WINDOWS; k++) {
            int i = order[k];
            Window* w = &pool[i];
            if (!w->visible || w->minimized || w->workspace != current_ws) continue;
            if (w->flags & WIN_FLAG_CLOSABLE) {
                int bx = w->x+w->w-CW-4, by = w->y+4;
                if (mx>=bx && mx<=bx+CW && my>=by && my<=by+CH) {
                    w->visible = 0; w->minimized = 0; count--;
                    if (focused == i) focused = -1;
                    return 1;
                }
            }
            if (w->flags & WIN_FLAG_MINIMIZABLE) {
                int mbx = w->x+w->w-2*(CW+4), mby = w->y+4;
                if (mx>=mbx && mx<=mbx+CW && my>=mby && my<=mby+CH) {
                    win_minimize(i); return 1;
                }
            }
        }
        /* Resize-Handles */
        for (int k = 0; k < MAX_WINDOWS; k++) {
            int i = order[k];
            Window* w = &pool[i];
            if (!w->visible || w->minimized || w->workspace != current_ws) continue;
            if (!(w->flags & WIN_FLAG_RESIZABLE)) continue;
            if (mx >= w->x+w->w-14 && mx < w->x+w->w &&
                my >= w->y+w->h-14 && my < w->y+w->h) {
                resize_id = i; resize_ox = mx; resize_oy = my;
                resize_ow = w->w; resize_oh = w->h;
                win_set_focus(i);
                return 1;
            }
        }
    }

    /* Titelleiste (Drag/Fokus) und Client-Bereich (Klick) — oberstes Fenster zuerst.
     * win_set_focus() nur beim Fresh-Click: verhindert z_gen-Overflow bei gehaltenem Button
     * und ungewollte Z-Order-Änderungen beim Halten. */
    for (int k = 0; k < MAX_WINDOWS; k++) {
        int i = order[k];
        Window* w = &pool[i];
        if (!w->visible || w->minimized || w->workspace != current_ws) continue;
        if (mx>=w->x && mx<w->x+w->w && my>=w->y && my<w->y+TH) {
            if (fresh) {
                win_set_focus(i);
                if (w->flags & WIN_FLAG_DRAGGABLE)
                    { drag_id=i; drag_ox=mx-w->x; drag_oy=my-w->y; }
            }
            return 1;
        }
        if (mx>=w->x && mx<w->x+w->w && my>=w->y && my<w->y+w->h) {
            if (fresh) {
                win_set_focus(i);
                if (w->click_fn && my >= w->y + TH)
                    w->click_fn(mx - w->x, my - (w->y + TH));
            }
            return 1;
        }
    }
    return 0;
}

/* ── taskbar integration ──────────────────────────────────────────────────── */

#define TBAR_BTN_W   110
#define TBAR_BTN_H    20
#define TBAR_BTN_GAP   4

void win_taskbar_render(int x, int y, int aw, int ah) {
    (void)ah;
    int bx = x;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!pool[i].visible) continue;
        if (pool[i].workspace != current_ws) continue;
        if (bx + TBAR_BTN_W > x + aw) break;

        int is_foc = (i == focused);
        int is_min = pool[i].minimized;

        uint32_t bc = is_foc ? g_theme.accent
                    : is_min ? RGB(45,48,70)
                             : RGB(38,42,58);
        gfx_fill_rounded_rect(bx, y, TBAR_BTN_W, TBAR_BTN_H, 3, bc);

        if (is_foc)
            draw_line(bx+3, y+TBAR_BTN_H-2, bx+TBAR_BTN_W-3, y+TBAR_BTN_H-2, COLOR_WHITE);

        /* truncated title: up to 11 chars, ".." if longer */
        char buf[14];
        int j = 0;
        while (j < 11 && pool[i].title[j]) { buf[j] = pool[i].title[j]; j++; }
        if (pool[i].title[j]) { buf[j++] = '.'; buf[j++] = '.'; }
        buf[j] = '\0';

        uint32_t fg = is_min ? RGB(150,150,180) : COLOR_WHITE;
        draw_string(bx+6, y+6, buf, fg, COLOR_TRANSPARENT);

        bx += TBAR_BTN_W + TBAR_BTN_GAP;
    }
}

int win_get_title(int id, char* out, int maxlen) {
    if (id < 0 || id >= MAX_WINDOWS || !pool[id].visible) return 0;
    int i = 0;
    while (pool[id].title[i] && i < maxlen-1) { out[i] = pool[id].title[i]; i++; }
    out[i] = '\0';
    return 1;
}

int win_taskbar_click(int mx, int my, int base_x, int base_y, int aw, int ah) {
    (void)ah;
    int bx = base_x;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!pool[i].visible) continue;
        if (pool[i].workspace != current_ws) continue;
        if (bx + TBAR_BTN_W > base_x + aw) break;
        if (mx >= bx && mx < bx + TBAR_BTN_W &&
            my >= base_y && my < base_y + TBAR_BTN_H) {
            if (pool[i].minimized) win_restore(i);
            else                   win_set_focus(i);
            return 1;
        }
        bx += TBAR_BTN_W + TBAR_BTN_GAP;
    }
    return 0;
}