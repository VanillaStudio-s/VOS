#include "kernel.h"
#include "drivers/screen.h"
#include "gfx/gfx.h"
#include "gui/gui.h"
#include "gui/theme.h"
#include "gui/window.h"
#include "gui/notify.h"
#include "mem/heap.h"
#include "fs/fs.h"
#include "apps/texteditor.h"
#include "apps/vosdolp.h"

/* ── layout ──────────────────────────────────────────── */
#define SB_W        162
#define HDR_H        22
#define SEARCH_H     20
#define FTR_H        22
#define ROW_H        17
#define SB_SEC_H     15
#define SB_ROW_H     18
#define WIN_W       760
#define WIN_H       360
#define EFF_HDR     (HDR_H + SEARCH_H)
#define BOT_H       (SB_SEC_H + SB_ROW_H + 4 + SB_SEC_H + SB_ROW_H)

/* main context menu  (12 rows incl. 4 separators) */
#define CTX_CNT      12
#define CTX_ITEM_H   18
#define CTX_ITEM_W   162
#define CTX_H        (CTX_CNT * CTX_ITEM_H + 6)

/* "New >" sub-menu */
#define CTX_SUB_CNT  3
#define CTX_SUB_W    150
#define CTX_SUB_H    (CTX_SUB_CNT * CTX_ITEM_H + 6)

/* ── state ───────────────────────────────────────────── */
static int dp_id   = -1;
static int nav_dir = 0;
static int nav_cur = 0;
static int nav_scr = 0;
static int _win_w  = WIN_W;
static int _win_h  = WIN_H;
static int _sb_sel = 0;

#define MAX_PINNED 10
typedef struct { char label[24]; int dir_idx; uint32_t icon_col; } Pinned;
static Pinned _pins[MAX_PINNED];
static int    _pin_cnt  = 0;
static int    _home_dir = 0;

/* search */
static char _search[28];
static int  _search_len = 0;
static int  _search_on  = 0;

/* clipboard  (0 = cut / move,  1 = copy) */
static int _clip_idx  = -1;
static int _clip_copy = 0;

/* context menu */
static int _ctx_open     = 0;
static int _ctx_x        = 0;
static int _ctx_y        = 0;
static int _ctx_sub_open = 0;
static int _ctx_sub_x    = 0;
static int _ctx_sub_y    = 0;

/* rename mode */
static int  _rename_mode = 0;
static char _rename_buf[32];
static int  _rename_len  = 0;
static int  _rename_idx  = -1;

/* ── helpers ─────────────────────────────────────────── */
static int _ci_eq(char a, char b) {
    if (a>='A'&&a<='Z') a+=32;
    if (b>='A'&&b<='Z') b+=32;
    return a==b;
}

static int _match(const char* name) {
    if (!_search_len) return 1;
    for (int i = 0; name[i]; i++) {
        int j = 0;
        while (j < _search_len && _ci_eq(name[i+j], _search[j])) j++;
        if (j == _search_len) return 1;
    }
    return 0;
}

static int _children(int dir, int* out, int maxout) {
    int cnt = 0;
    for (int i = 0; i < fs_node_count && cnt < maxout; i++)
        if (fs_tree[i].type != 0 && fs_tree[i].parent_idx == dir && i != 0)
            if (_match(fs_tree[i].name))
                out[cnt++] = i;
    return cnt;
}

static void _u32str(uint32_t n, char* b) {
    if (!n) { b[0]='0'; b[1]='\0'; return; }
    char t[12]; int i = 0;
    while (n && i < 10) { t[i++] = '0' + (int)(n % 10); n /= 10; }
    int j = 0; for (int k = i-1; k >= 0; k--) b[j++] = t[k]; b[j] = '\0';
}

static void _lbl(char* dst, const char* src) {
    int i = 0; while (src[i] && i < 23) { dst[i]=src[i]; i++; } dst[i]='\0';
}

static int _is_ancestor(int dir, int of_dir) {
    int idx = of_dir;
    for (int s = 0; s < MAX_NODES; s++) {
        if (idx == dir) return 1;
        if (idx == 0) break;
        idx = fs_tree[idx].parent_idx;
    }
    return 0;
}

static void _unique_name(const char* base, const char* ext, char* out, int olen) {
    for (int n = 1; n <= 99; n++) {
        char t[32]; int bi = 0;
        if (n == 1) {
            while (base[bi] && bi < 20) { t[bi]=base[bi]; bi++; }
            int ei=0; while (ext[ei] && bi<30) { t[bi++]=ext[ei++]; }
            t[bi]='\0';
        } else {
            while (base[bi] && bi < 15) { t[bi]=base[bi]; bi++; }
            t[bi++]=' ';
            if (n>=10) t[bi++]='0'+n/10;
            t[bi++]='0'+n%10;
            int ei=0; while (ext[ei] && bi<30) { t[bi++]=ext[ei++]; }
            t[bi]='\0';
        }
        int found = 0;
        for (int i = 0; i < fs_node_count; i++)
            if (fs_tree[i].type!=0 && fs_tree[i].parent_idx==nav_dir
                && my_strcmp(fs_tree[i].name,t)==0) { found=1; break; }
        if (!found) {
            int k=0; while (t[k] && k<olen-1) { out[k]=t[k]; k++; } out[k]='\0'; return;
        }
    }
    int k=0; while (base[k] && k<olen-1) { out[k]=base[k]; k++; } out[k]='\0';
}

/* ── file extension helper ───────────────────────────── */
static int _has_ext(const char* name, const char* ext) {
    int ni = 0; while (name[ni]) ni++;
    int ei = 0; while (ext[ei])  ei++;
    if (ni < ei) return 0;
    for (int i = 0; i < ei; i++)
        if (name[ni - ei + i] != ext[i]) return 0;
    return 1;
}

/* ── open item: dir = navigate, file = launch app ────── */
static void _open_item(int ni) {
    if (ni < 0) return;
    if (fs_tree[ni].type == NODE_TYPE_DIR) {
        nav_dir = ni; nav_cur = 0; nav_scr = 0;
        _search_len = 0; _search[0] = '\0'; _search_on = 0;
    } else {
        const char* nm = fs_tree[ni].name;
        if (_has_ext(nm, ".txt") || _has_ext(nm, ".log") || _has_ext(nm, ".cfg")) {
            current_dir_idx = nav_dir;
            app_texteditor_open(nm);
        } else {
            notify_push("vosdolp", "No app for this file type", NOTIFY_WARN);
        }
    }
}

/* ── forward declaration ─────────────────────────────── */
static void _delete(void);

/* ── paste move ──────────────────────────────────────── */
static void _paste_move(void) {
    if (_clip_idx < 0) { notify_push("vosdolp","Nothing to paste",NOTIFY_WARN); return; }
    if (fs_tree[_clip_idx].type == 0) { _clip_idx=-1; return; }
    if (fs_tree[_clip_idx].parent_idx == nav_dir)
        { notify_push("vosdolp","Already here",NOTIFY_WARN); return; }
    if (fs_tree[_clip_idx].type == NODE_TYPE_DIR && _is_ancestor(_clip_idx, nav_dir))
        { notify_push("vosdolp","Can't move into self",NOTIFY_ERROR); return; }
    for (int i = 0; i < fs_node_count; i++)
        if (fs_tree[i].type != 0 && fs_tree[i].parent_idx == nav_dir && i != _clip_idx
            && my_strcmp(fs_tree[i].name, fs_tree[_clip_idx].name) == 0)
            { notify_push("vosdolp","Name collision",NOTIFY_ERROR); return; }
    fs_tree[_clip_idx].parent_idx = nav_dir;
    _clip_idx = -1;
    if (nav_cur > 0) nav_cur--;
    notify_push("vosdolp","Moved",NOTIFY_SUCCESS);
}

/* ── paste copy ──────────────────────────────────────── */
static void _paste_copy(void) {
    if (_clip_idx < 0 || fs_tree[_clip_idx].type == 0)
        { notify_push("vosdolp","Nothing to copy",NOTIFY_WARN); return; }
    char newname[32]; _unique_name(fs_tree[_clip_idx].name, "", newname, 32);
    if (fs_tree[_clip_idx].type == NODE_TYPE_DIR) {
        fs_mkdir_in(nav_dir, newname);
        notify_push("vosdolp","Dir copied (empty)",NOTIFY_SUCCESS);
    } else {
        fs_touch_in(nav_dir, newname);
        int ni = -1;
        for (int i = fs_node_count-1; i >= 0; i--)
            if (fs_tree[i].type == NODE_TYPE_FILE && fs_tree[i].parent_idx == nav_dir
                && my_strcmp(fs_tree[i].name, newname) == 0) { ni = i; break; }
        if (ni >= 0 && fs_tree[_clip_idx].content && fs_tree[_clip_idx].content_len > 0) {
            int len = fs_tree[_clip_idx].content_len;
            fs_tree[ni].content = (char*)kmalloc(len + 1);
            if (fs_tree[ni].content) {
                for (int j = 0; j < len; j++)
                    fs_tree[ni].content[j] = fs_tree[_clip_idx].content[j];
                fs_tree[ni].content[len] = '\0';
                fs_tree[ni].content_len  = len;
            }
        }
        notify_push("vosdolp","Copied",NOTIFY_SUCCESS);
    }
}

/* ── get selected node index (-1 if none / up-row) ──── */
static int _sel_ni(void) {
    int items[64];
    int cnt    = _children(nav_dir, items, 64);
    int has_up = (nav_dir != 0) && !_search_len;
    int total  = cnt + (has_up ? 1 : 0);
    if (nav_cur >= total || (has_up && nav_cur == 0)) return -1;
    return has_up ? items[nav_cur-1] : items[nav_cur];
}

/* ── sub-menu actions (New ►) ────────────────────────── */
static void _ctx_sub_action(int item) {
    _ctx_open = 0; _ctx_sub_open = 0;
    switch (item) {
    case 0: { char n[32]; _unique_name("New Folder","",n,32);
              fs_mkdir_in(nav_dir, n);
              notify_push("vosdolp","Folder created",NOTIFY_SUCCESS); break; }
    case 1: { char n[32]; _unique_name("untitled",".txt",n,32);
              fs_touch_in(nav_dir, n);
              notify_push("vosdolp","Text file created",NOTIFY_SUCCESS); break; }
    case 2: { char n[32]; _unique_name("untitled",".bin",n,32);
              fs_touch_in(nav_dir, n);
              notify_push("vosdolp","File created",NOTIFY_SUCCESS); break; }
    default: break;
    }
    gui_render();
}

/* ── main ctx menu actions ───────────────────────────── */
/*   menu layout:
       0  Open        (file-type aware)
       1  ----------- sep
       2  Copy        ^C
       3  Cut         ^X
       4  Paste       ^V
       5  ----------- sep
       6  Rename      F2
       7  Properties
       8  ----------- sep
       9  New >
      10  ----------- sep
      11  Delete      F8              */
static void _ctx_action(int item) {
    int ni = _sel_ni();

    switch (item) {

    case 0: /* Open */
        if (ni < 0) { notify_push("vosdolp","Nothing selected",NOTIFY_WARN); break; }
        _open_item(ni);
        break;

    /* 1 = separator */

    case 2: /* Copy */
        if (ni < 0) { notify_push("vosdolp","Nothing selected",NOTIFY_WARN); break; }
        _clip_idx = ni; _clip_copy = 1;
        notify_push("vosdolp","Copied to clipboard (^V)",NOTIFY_INFO);
        break;

    case 3: /* Cut */
        if (ni < 0) { notify_push("vosdolp","Nothing selected",NOTIFY_WARN); break; }
        _clip_idx = ni; _clip_copy = 0;
        notify_push("vosdolp","Cut (^V to paste)",NOTIFY_INFO);
        break;

    case 4: /* Paste */
        if (_clip_copy) _paste_copy(); else _paste_move();
        break;

    /* 5 = separator */

    case 6: /* Rename */
        if (ni < 0) { notify_push("vosdolp","Nothing selected",NOTIFY_WARN); break; }
        _rename_idx = ni; _rename_len = 0;
        for (int j = 0; fs_tree[ni].name[j] && j < 31; j++)
            { _rename_buf[j] = fs_tree[ni].name[j]; _rename_len++; }
        _rename_buf[_rename_len] = '\0';
        _rename_mode = 1;
        break;

    case 7: /* Properties */
        if (ni < 0) { notify_push("vosdolp","Nothing selected",NOTIFY_WARN); break; }
        {
            char msg[52]; int mi = 0;
            if (fs_tree[ni].type == NODE_TYPE_DIR) {
                static const char pfx[] = "Dir | Items: ";
                for (int j = 0; pfx[j]; j++) msg[mi++] = pfx[j];
                int ch[64]; char ns[8];
                _u32str((uint32_t)_children(ni, ch, 64), ns);
                for (int j = 0; ns[j]; j++) msg[mi++] = ns[j];
            } else {
                static const char pfx[] = "File | Size: ";
                for (int j = 0; pfx[j]; j++) msg[mi++] = pfx[j];
                char ns[8]; _u32str((uint32_t)fs_tree[ni].content_len, ns);
                for (int j = 0; ns[j]; j++) msg[mi++] = ns[j];
                msg[mi++]=' '; msg[mi++]='B';
                /* show detected type */
                const char* nm = fs_tree[ni].name;
                const char* tp = "binary";
                if (_has_ext(nm,".txt")||_has_ext(nm,".log")||_has_ext(nm,".cfg")) tp = "text";
                else if (_has_ext(nm,".bmp")||_has_ext(nm,".png")) tp = "image";
                msg[mi++]=' '; msg[mi++]='(';
                for (int k=0; tp[k]; k++) msg[mi++]=tp[k];
                msg[mi++]=')';
            }
            msg[mi]='\0';
            notify_push("vosdolp", msg, NOTIFY_INFO);
        }
        break;

    /* 8 = separator */

    case 9: /* New > — toggle sub-menu, keep ctx open */
        {
            int sy = _ctx_y + 3 + 9 * CTX_ITEM_H;
            _ctx_sub_x = _ctx_x + CTX_ITEM_W;
            _ctx_sub_y = sy;
            if (_ctx_sub_x + CTX_SUB_W > _win_w) _ctx_sub_x = _ctx_x - CTX_SUB_W;
            if (_ctx_sub_y + CTX_SUB_H > _win_h) _ctx_sub_y = _win_h - CTX_SUB_H;
            _ctx_sub_open = !_ctx_sub_open;
        }
        gui_render(); return;   /* don't close main menu */

    /* 10 = separator */

    case 11: /* Delete */
        _delete(); break;

    default: break;
    }

    _ctx_open = 0; _ctx_sub_open = 0;
    gui_render();
}

/* ── sidebar hit-test ────────────────────────────────── */
static int _sb_hit(int ry, int* out_dir) {
    int y = SB_SEC_H;
    for (int i = 0; i < _pin_cnt; i++) {
        if (ry >= y && ry < y + SB_ROW_H) {
            if (out_dir) *out_dir = _pins[i].dir_idx;
            return i;
        }
        y += SB_ROW_H;
    }
    int drv_y = _win_h - BOT_H + SB_SEC_H;
    if (ry >= drv_y && ry < drv_y + SB_ROW_H) {
        if (out_dir) *out_dir = 0;
        return _pin_cnt;
    }
    return -1;
}

/* ── sidebar draw ────────────────────────────────────── */
static void _draw_sb(int sx, int sy, int sw, int sh) {
    draw_rect(sx, sy, sw, sh, RGB(19, 22, 36));
    draw_rect(sx, sy, sw, SB_SEC_H, RGB(13, 15, 26));
    draw_string(sx+5, sy+2, "PINNED", RGB(65,80,115), COLOR_TRANSPARENT);
    int y = sy + SB_SEC_H;
    for (int i = 0; i < _pin_cnt; i++) {
        int sel = (_sb_sel == i);
        draw_rect(sx, y, sw, SB_ROW_H, sel ? g_theme.accent : RGB(19,22,36));
        draw_rect(sx+6, y+4, 10, 10, _pins[i].icon_col);
        draw_string(sx+20, y+2, _pins[i].label,
                    sel ? COLOR_WHITE : g_theme.text_primary, COLOR_TRANSPARENT);
        y += SB_ROW_H;
    }
    int by = sy + sh - BOT_H;
    draw_line(sx+4, by-3, sx+sw-4, by-3, RGB(35,40,60));
    draw_rect(sx, by, sw, SB_SEC_H, RGB(13,15,26));
    draw_string(sx+5, by+2, "DRIVES", RGB(65,80,115), COLOR_TRANSPARENT);
    by += SB_SEC_H;
    int drv_sel = (_sb_sel == _pin_cnt);
    draw_rect(sx, by, sw, SB_ROW_H, drv_sel ? g_theme.accent : RGB(19,22,36));
    draw_rect(sx+6,  by+3, 10, 7, RGB(50,100,170));
    draw_rect(sx+8,  by+8,  6, 2, RGB(80,130,200));
    draw_string(sx+20, by+2, "RAM Disk  /",
                drv_sel ? COLOR_WHITE : g_theme.text_primary, COLOR_TRANSPARENT);
    by += SB_ROW_H + 4;
    draw_rect(sx, by, sw, SB_SEC_H, RGB(13,15,26));
    draw_string(sx+5, by+2, "NETWORK", RGB(65,80,115), COLOR_TRANSPARENT);
    by += SB_SEC_H;
    draw_string(sx+20, by+2, "(none)", RGB(42,52,72), COLOR_TRANSPARENT);
}

/* ── sub-menu draw ───────────────────────────────────── */
static void _draw_ctx_sub(int ax, int ay) {
    draw_rect(ax+2, ay+2, CTX_SUB_W, CTX_SUB_H, RGB(0,0,0));
    draw_rect(ax,   ay,   CTX_SUB_W, CTX_SUB_H, RGB(55,60,90));
    draw_rect(ax+1, ay+1, CTX_SUB_W-2, CTX_SUB_H-2, RGB(30,35,58));
    static const char* sl[CTX_SUB_CNT] = {
        "New Folder", "New Text (.txt)", "New File (.bin)"
    };
    int iy = ay + 3;
    for (int i = 0; i < CTX_SUB_CNT; i++) {
        draw_string(ax+8, iy+2, sl[i], g_theme.text_primary, COLOR_TRANSPARENT);
        iy += CTX_ITEM_H;
    }
}

/* ── main context menu draw ──────────────────────────── */
static void _draw_ctx(int ax, int ay) {
    draw_rect(ax+2, ay+2, CTX_ITEM_W, CTX_H, RGB(0,0,0));
    draw_rect(ax,   ay,   CTX_ITEM_W, CTX_H, RGB(55,60,90));
    draw_rect(ax+1, ay+1, CTX_ITEM_W-2, CTX_H-2, RGB(26,30,50));

    static const char* labels[CTX_CNT] = {
        "Open",
        NULL,                       /* 1 sep */
        "Copy", "Cut", "Paste",
        NULL,                       /* 5 sep */
        "Rename", "Properties",
        NULL,                       /* 8 sep */
        "New >",
        NULL,                       /* 10 sep */
        "Delete"
    };
    static const char* hints[CTX_CNT] = {
        NULL, NULL, "^C", "^X", "^V", NULL, "F2", NULL, NULL, NULL, NULL, "F8"
    };

    int iy = ay + 3;
    for (int i = 0; i < CTX_CNT; i++) {
        if (i == 1 || i == 5 || i == 8 || i == 10) {
            draw_line(ax+6, iy+CTX_ITEM_H/2, ax+CTX_ITEM_W-6,
                      iy+CTX_ITEM_H/2, RGB(55,60,90));
        } else {
            if (i == 9 && _ctx_sub_open)
                draw_rect(ax+1, iy, CTX_ITEM_W-2, CTX_ITEM_H, RGB(36,44,72));
            uint32_t fg = (i == 11) ? RGB(220,80,80) : g_theme.text_primary;
            /* dim "Open" if nothing selected */
            if (i == 0 && _sel_ni() < 0) fg = RGB(70,75,100);
            draw_string(ax+8, iy+2, labels[i], fg, COLOR_TRANSPARENT);
            if (hints[i])
                draw_string(ax+CTX_ITEM_W-28, iy+2, hints[i], RGB(80,90,120), COLOR_TRANSPARENT);
        }
        iy += CTX_ITEM_H;
    }
}

/* ── main area draw ──────────────────────────────────── */
static void _draw_main(int mx, int my, int mw, int mh) {
    /* path bar */
    draw_rect(mx, my, mw, HDR_H, RGB(14,17,30));
    char pbuf[96]; fs_get_path(nav_dir, pbuf, 96);
    if (_clip_idx >= 0) {
        draw_string(mx+8, my+4, pbuf, RGB(120,180,255), COLOR_TRANSPARENT);
        const char* tag = _clip_copy ? "[CPY]" : "[CUT]";
        draw_string(mx+mw-54, my+4, tag, RGB(210,140,50), COLOR_TRANSPARENT);
    } else {
        draw_string(mx+8, my+4, pbuf, g_theme.text_primary, COLOR_TRANSPARENT);
    }

    /* search bar */
    int sry = my + HDR_H;
    draw_rect(mx, sry, mw, SEARCH_H, RGB(10,12,22));
    draw_string(mx+5, sry+2, "Find:", RGB(70,90,130), COLOR_TRANSPARENT);
    int bx = mx+44, bw = mw-50;
    uint32_t box_bg = _search_on ? RGB(22,32,68) : RGB(14,17,32);
    draw_rect(bx, sry+2, bw, 16, box_bg);
    draw_rect(bx, sry+2, bw, 1, RGB(40,50,80));
    draw_rect(bx, sry+17, bw, 1, RGB(40,50,80));
    if (_search_len > 0) {
        draw_string(bx+3, sry+4, _search, COLOR_WHITE, COLOR_TRANSPARENT);
        if (_search_on)
            draw_rect(bx+3+_search_len*8, sry+4, 1, 10, COLOR_WHITE);
    } else {
        draw_string(bx+3, sry+4, "type to filter...", RGB(42,52,72), COLOR_TRANSPARENT);
        if (_search_on)
            draw_rect(bx+3, sry+4, 1, 10, COLOR_WHITE);
    }

    /* file list */
    int items[64];
    int cnt    = _children(nav_dir, items, 64);
    int has_up = (nav_dir != 0) && !_search_len;
    int total  = cnt + (has_up ? 1 : 0);
    int vis    = (mh - EFF_HDR - FTR_H) / ROW_H;

    for (int vi = 0; vi < vis; vi++) {
        int idx = nav_scr + vi;
        if (idx >= total) break;
        int ry  = my + EFF_HDR + vi * ROW_H;
        int sel = (idx == nav_cur);

        uint32_t row_bg = sel ? g_theme.accent
                              : (vi%2==0) ? RGB(18,21,36) : RGB(15,18,32);
        draw_rect(mx, ry, mw-6, ROW_H, row_bg);
        uint32_t fg = sel ? COLOR_WHITE : g_theme.text_primary;

        if (idx == 0 && has_up) {
            draw_string(mx+6,  ry+1, "[..]", RGB(100,120,160), COLOR_TRANSPARENT);
            draw_string(mx+38, ry+1, "..",   RGB(100,120,160), COLOR_TRANSPARENT);
        } else {
            int ni     = has_up ? items[idx-1] : items[idx];
            int is_dir = (fs_tree[ni].type == NODE_TYPE_DIR);
            int is_cut = (_clip_copy == 0 && _clip_idx == ni);

            if (is_dir) {
                draw_rect(mx+6, ry+4, 12, 9, RGB(190,160,40));
                draw_rect(mx+6, ry+2,  5, 3, RGB(190,160,40));
            } else {
                draw_rect(mx+6,  ry+2, 10, 13, RGB(145,165,205));
                draw_rect(mx+13, ry+2,  3,  3, RGB(115,135,175));
            }

            if (_rename_mode && ni == _rename_idx) {
                int rw = mw - 84;
                draw_rect(mx+22, ry, rw, ROW_H, RGB(18,34,70));
                draw_rect(mx+22, ry,   rw, 1, RGB(60,90,160));
                draw_rect(mx+22, ry+ROW_H-1, rw, 1, RGB(60,90,160));
                draw_string(mx+24, ry+1, _rename_buf, COLOR_WHITE, COLOR_TRANSPARENT);
                draw_rect(mx+24+_rename_len*8, ry+2, 1, ROW_H-4, RGB(160,200,255));
            } else {
                uint32_t nfg = is_cut ? RGB(200,130,50) : fg;
                draw_string(mx+22, ry+1, fs_tree[ni].name, nfg, COLOR_TRANSPARENT);
                if (!is_dir && fs_tree[ni].content_len > 0) {
                    char sz[12]; _u32str((uint32_t)fs_tree[ni].content_len, sz);
                    draw_string(mx+mw-54, ry+1, sz, g_theme.text_secondary, COLOR_TRANSPARENT);
                    draw_string(mx+mw-20, ry+1, "B", g_theme.text_secondary, COLOR_TRANSPARENT);
                } else if (is_dir) {
                    int ch[4]; int cc = _children(ni, ch, 4);
                    if (cc > 0) {
                        char ns[6]; _u32str((uint32_t)cc, ns);
                        draw_string(mx+mw-38, ry+1, ns,  g_theme.text_secondary, COLOR_TRANSPARENT);
                        draw_string(mx+mw-20, ry+1, "it", g_theme.text_secondary, COLOR_TRANSPARENT);
                    }
                }
            }
        }
    }

    /* scrollbar */
    if (total > vis) {
        int sbx = mx+mw-5, sbo = my+EFF_HDR;
        int sbh = mh-EFF_HDR-FTR_H;
        int th  = sbh*vis/total, ty = sbh*nav_scr/total;
        draw_rect(sbx, sbo, 5, sbh, RGB(22,25,40));
        draw_rect(sbx, sbo+ty, 5, th, RGB(72,82,120));
    }

    /* footer */
    int fy = my+mh-FTR_H;
    draw_rect(mx, fy, mw, FTR_H, RGB(13,15,26));
    if (_rename_mode) {
        draw_string(mx+4, fy+4,
                    "Rename: type new name | Enter=OK  Esc=Cancel",
                    RGB(200,170,80), COLOR_TRANSPARENT);
    } else {
        draw_string(mx+4, fy+4,
                    "F2:Ren  F8:Del  F10:Close  ^C:Cpy  ^X:Cut  ^V:Paste  RClick:Menu",
                    RGB(90,105,140), COLOR_TRANSPARENT);
    }
}

/* ── top-level draw ──────────────────────────────────── */
static void _draw(int x, int y, int w, int h) {
    _win_w = w; _win_h = h;
    _draw_sb(x, y, SB_W, h);
    draw_line(x+SB_W, y, x+SB_W, y+h, RGB(36,41,62));
    _draw_main(x+SB_W+1, y, w-SB_W-1, h);
    if (_ctx_open)     _draw_ctx    (x+_ctx_x,     y+_ctx_y);
    if (_ctx_sub_open) _draw_ctx_sub(x+_ctx_sub_x, y+_ctx_sub_y);
}

/* ── navigate into selected ──────────────────────────── */
static void _enter_sel(void) {
    int items[64];
    int cnt    = _children(nav_dir, items, 64);
    int has_up = (nav_dir != 0) && !_search_len;
    int total  = cnt + (has_up ? 1 : 0);
    if (nav_cur >= total) return;
    if (has_up && nav_cur == 0) {
        nav_dir = fs_tree[nav_dir].parent_idx;
        nav_cur = 0; nav_scr = 0;
        _search_len=0; _search[0]='\0';
    } else {
        int ni = has_up ? items[nav_cur-1] : items[nav_cur];
        _open_item(ni);
    }
}

/* ── delete selected ─────────────────────────────────── */
static void _delete(void) {
    int items[64];
    int cnt    = _children(nav_dir, items, 64);
    int has_up = (nav_dir != 0) && !_search_len;
    int total  = cnt + (has_up ? 1 : 0);
    if (has_up && nav_cur == 0) return;
    if (nav_cur >= total) return;
    int ni = has_up ? items[nav_cur-1] : items[nav_cur];
    for (int i = 1; i < fs_node_count; i++)
        if (fs_tree[i].type != 0 && fs_tree[i].parent_idx == ni)
            { notify_push("vosdolp","Dir not empty",NOTIFY_ERROR); return; }
    if (ni == _clip_idx) _clip_idx = -1;
    if (fs_tree[ni].content) { kfree(fs_tree[ni].content); fs_tree[ni].content=(char*)0; }
    fs_tree[ni].content_len=0; fs_tree[ni].type=0; fs_tree[ni].name[0]='\0';
    if (nav_cur > 0) nav_cur--;
    notify_push("vosdolp","Deleted",NOTIFY_SUCCESS);
}

/* ── keyboard ────────────────────────────────────────── */
static void _key(char c, unsigned char sc, int ctrl) {

    /* ── rename mode ── */
    if (_rename_mode) {
        if (sc == 0x01) { _rename_mode=0; _rename_idx=-1; gui_render(); return; } /* Esc */
        if (c == '\n') {  /* Enter = confirm */
            if (_rename_len > 0 && _rename_idx >= 0) {
                int ok = 1;
                for (int i = 0; i < fs_node_count; i++)
                    if (fs_tree[i].type != 0 && fs_tree[i].parent_idx == nav_dir
                        && i != _rename_idx
                        && my_strcmp(fs_tree[i].name, _rename_buf) == 0) { ok=0; break; }
                if (!ok) { notify_push("vosdolp","Name taken",NOTIFY_ERROR); }
                else {
                    int j = 0;
                    while (_rename_buf[j] && j < 31)
                        { fs_tree[_rename_idx].name[j]=_rename_buf[j]; j++; }
                    fs_tree[_rename_idx].name[j] = '\0';
                    notify_push("vosdolp","Renamed",NOTIFY_SUCCESS);
                }
            }
            _rename_mode=0; _rename_idx=-1; gui_render(); return;
        }
        if (sc == 0x0E && _rename_len > 0)
            { _rename_buf[--_rename_len]='\0'; gui_render(); return; }
        if (c >= 0x20 && c <= 0x7E && _rename_len < 31)
            { _rename_buf[_rename_len++]=c; _rename_buf[_rename_len]='\0'; gui_render(); return; }
        return;
    }

    if (sc == 0x44) { win_close(dp_id); dp_id=-1; gui_render(); return; } /* F10 */
    if (sc == 0x42 && !_search_on) { _delete(); gui_render(); return; }   /* F8  */

    /* F2 = Rename */
    if (sc == 0x3C && !_search_on) {
        int ni = _sel_ni();
        if (ni >= 0) {
            _rename_idx=ni; _rename_len=0;
            for (int j = 0; fs_tree[ni].name[j] && j < 31; j++)
                { _rename_buf[j]=fs_tree[ni].name[j]; _rename_len++; }
            _rename_buf[_rename_len]='\0';
            _rename_mode=1;
        }
        gui_render(); return;
    }

    /* Ctrl+C = copy */
    if (ctrl && (c=='c'||c=='C')) {
        int ni = _sel_ni();
        if (ni >= 0) { _clip_idx=ni; _clip_copy=1; notify_push("vosdolp","Copied (^V paste)",NOTIFY_INFO); }
        gui_render(); return;
    }
    /* Ctrl+X = cut */
    if (ctrl && (c=='x'||c=='X')) {
        int ni = _sel_ni();
        if (ni >= 0) { _clip_idx=ni; _clip_copy=0; notify_push("vosdolp","Cut (^V to paste)",NOTIFY_INFO); }
        gui_render(); return;
    }
    /* Ctrl+V = paste */
    if (ctrl && (c=='v'||c=='V')) {
        if (_clip_copy) _paste_copy(); else _paste_move();
        gui_render(); return;
    }

    /* Escape */
    if (sc == 0x01) {
        if (_ctx_sub_open) { _ctx_sub_open=0; gui_render(); return; }
        if (_ctx_open)     { _ctx_open=0; _ctx_sub_open=0; gui_render(); return; }
        if (_search_len > 0 || _search_on) {
            _search_len=0; _search[0]='\0'; _search_on=0;
            nav_cur=0; nav_scr=0; gui_render(); return;
        }
    }

    /* search mode input */
    if (_search_on) {
        if (sc == 0x0E && _search_len > 0)
            { _search[--_search_len]='\0'; nav_cur=0; nav_scr=0; gui_render(); return; }
        if (c == '\n') { _search_on=0; gui_render(); return; }
        if (c >= 0x20 && c <= 0x7E && _search_len < 27) {
            _search[_search_len++]=c; _search[_search_len]='\0';
            nav_cur=0; nav_scr=0; gui_render(); return;
        }
    }

    /* navigation */
    int items[64];
    int cnt   = _children(nav_dir, items, 64);
    int has_u = (nav_dir != 0) && !_search_len;
    int total = cnt + (has_u ? 1 : 0);
    int vis   = (_win_h - EFF_HDR - FTR_H) / ROW_H;

    if (c==0 && sc==0x48) { /* up */
        if (nav_cur > 0) nav_cur--;
        if (nav_cur < nav_scr) nav_scr = nav_cur;
        gui_render(); return;
    }
    if (c==0 && sc==0x50) { /* down */
        if (nav_cur < total-1) nav_cur++;
        if (nav_cur >= nav_scr+vis) nav_scr = nav_cur-vis+1;
        gui_render(); return;
    }
    if (c=='\n')           { _enter_sel(); gui_render(); return; }
    if (c==0 && sc==0x4D) { _enter_sel(); gui_render(); return; }
    if (c==0 && sc==0x4B) {
        if (nav_dir != 0) {
            nav_dir=fs_tree[nav_dir].parent_idx; nav_cur=0; nav_scr=0;
            _search_len=0; _search[0]='\0'; _search_on=0;
        }
        gui_render(); return;
    }
}

/* ── left click ──────────────────────────────────────── */
static void _click(int rx, int ry) {

    /* sub-menu hit test first */
    if (_ctx_sub_open) {
        if (rx>=_ctx_sub_x && rx<_ctx_sub_x+CTX_SUB_W &&
            ry>=_ctx_sub_y && ry<_ctx_sub_y+CTX_SUB_H) {
            int item = (ry - _ctx_sub_y - 3) / CTX_ITEM_H;
            if (item >= 0 && item < CTX_SUB_CNT) _ctx_sub_action(item);
            else { _ctx_open=0; _ctx_sub_open=0; gui_render(); }
            return;
        }
        /* click outside sub-menu: fall through to main menu check */
    }

    /* main context menu */
    if (_ctx_open) {
        if (rx>=_ctx_x && rx<_ctx_x+CTX_ITEM_W && ry>=_ctx_y && ry<_ctx_y+CTX_H) {
            int item = (ry - _ctx_y - 3) / CTX_ITEM_H;
            if (item == 9) {
                _ctx_action(9); return;   /* toggle sub-menu, keep ctx open */
            }
            _ctx_sub_open = 0;
            if (item>=0 && item<CTX_CNT && item!=1 && item!=5 && item!=8 && item!=10)
                _ctx_action(item);
            else { _ctx_open=0; gui_render(); }
        } else {
            _ctx_open=0; _ctx_sub_open=0; gui_render();
        }
        return;
    }

    /* click cancels rename */
    if (_rename_mode) { _rename_mode=0; _rename_idx=-1; gui_render(); return; }

    /* sidebar */
    if (rx < SB_W) {
        int dir;
        int si = _sb_hit(ry, &dir);
        if (si >= 0) {
            _sb_sel=si; nav_dir=dir; nav_cur=0; nav_scr=0;
            _search_len=0; _search[0]='\0'; _search_on=0;
        }
        gui_render(); return;
    }

    /* search bar click */
    if (ry >= HDR_H && ry < EFF_HDR) { _search_on=1; gui_render(); return; }
    _search_on=0;

    if (ry < EFF_HDR || ry >= _win_h-FTR_H) { gui_render(); return; }

    int vi     = (ry - EFF_HDR) / ROW_H;
    int actual = vi + nav_scr;
    int items[64];
    int cnt    = _children(nav_dir, items, 64);
    int has_up = (nav_dir != 0) && !_search_len;
    int total  = cnt + (has_up ? 1 : 0);
    if (actual >= total) { gui_render(); return; }

    if (nav_cur == actual) _enter_sel();
    else                   nav_cur = actual;
    gui_render();
}

/* ── right click ─────────────────────────────────────── */
static void _right_click(int rx, int ry) {
    _rename_mode=0; _rename_idx=-1; _search_on=0;
    if (rx > SB_W && ry >= EFF_HDR && ry < _win_h-FTR_H) {
        int vi     = (ry - EFF_HDR) / ROW_H;
        int actual = vi + nav_scr;
        int items[64];
        int cnt    = _children(nav_dir, items, 64);
        int has_up = (nav_dir != 0) && !_search_len;
        int total  = cnt + (has_up ? 1 : 0);
        if (actual < total) nav_cur = actual;
    }
    _ctx_open=1; _ctx_sub_open=0;
    _ctx_x=rx; _ctx_y=ry;
    if (_ctx_x + CTX_ITEM_W > _win_w) _ctx_x = _win_w - CTX_ITEM_W;
    if (_ctx_y + CTX_H      > _win_h) _ctx_y = _win_h - CTX_H;
    gui_render();
}

/* ── open ────────────────────────────────────────────── */
void app_vosdolp_open(void) {
    if (win_is_open(dp_id)) { win_set_focus(dp_id); return; }

    _home_dir = fs_get_home_idx();
    nav_dir   = _home_dir;
    nav_cur=0; nav_scr=0; _sb_sel=0; _pin_cnt=0;
    _clip_idx=-1; _clip_copy=0;
    _ctx_open=0; _ctx_sub_open=0;
    _rename_mode=0; _rename_idx=-1;
    _search_len=0; _search[0]='\0'; _search_on=0;

    _lbl(_pins[_pin_cnt].label, "Home");
    _pins[_pin_cnt].dir_idx  = _home_dir;
    _pins[_pin_cnt].icon_col = RGB(225,185,40); _pin_cnt++;

    _lbl(_pins[_pin_cnt].label, "/ Root");
    _pins[_pin_cnt].dir_idx  = 0;
    _pins[_pin_cnt].icon_col = RGB(175,75,75); _pin_cnt++;

    static const char* subs[] = { "Desktop","Documents","Downloads","Music","Video","Picture" };
    static const uint32_t cols[] = {
        RGB(70,140,215), RGB(75,175,115), RGB(195,115,45),
        RGB(175,75,195), RGB(65,165,205), RGB(185,135,55)
    };
    for (int s = 0; s < 6 && _pin_cnt < MAX_PINNED; s++) {
        for (int i = 1; i < fs_node_count; i++) {
            if (fs_tree[i].type == NODE_TYPE_DIR && fs_tree[i].parent_idx == _home_dir
                && my_strcmp(fs_tree[i].name, subs[s]) == 0) {
                _lbl(_pins[_pin_cnt].label, subs[s]);
                _pins[_pin_cnt].dir_idx  = i;
                _pins[_pin_cnt].icon_col = cols[s];
                _pin_cnt++; break;
            }
        }
    }

    dp_id = win_create(20, 20, WIN_W, WIN_H+24, "vosdolp - Files",
                       WIN_FLAG_DRAGGABLE|WIN_FLAG_CLOSABLE|WIN_FLAG_RESIZABLE);
    win_set_min_size(dp_id, 260, 160);
    win_set_content(dp_id, _draw);
    win_set_key(dp_id, _key);
    win_set_click(dp_id, _click);
    win_set_right_click(dp_id, _right_click);
}