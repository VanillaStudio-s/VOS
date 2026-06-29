#include "kernel.h"
#include "drivers/screen.h"
#include "gfx/gfx.h"
#include "gui/gui.h"
#include "gui/theme.h"
#include "gui/window.h"
#include "gui/notify.h"
#include "mem/heap.h"
#include "cpu/timer.h"
#include "fs/fs.h"
#include "apps/sysmon.h"

/* ── layout ──────────────────────────────────────────── */
#define SM_WIN_W   420
#define SM_WIN_H   444     /* 420 content + 24 titlebar */
#define SM_TAB_H    24     /* tab bar height */
#define SM_TAB_W   110     /* width of each tab button */
#define SM_PAD       8
#define SM_ROW      18     /* row height in process list */
#define SM_SEC_H    15     /* section header height */
#define SM_BTN_W    38     /* "End" button width */
#define SM_BTN_H    14     /* "End" button height */
#define SM_BAR_H    14     /* progress bar height */

/* ── system processes (always running, protected) ────── */
#define KPROC_CNT 8
static const char* kp_name[KPROC_CNT] = {
    "kernel","irq","timer","keyboard","mouse","gui","fs","shell"
};
static const char* kp_desc[KPROC_CNT] = {
    "Core Kernel","IRQ Handler","System Timer",
    "Keyboard Driver","Mouse Driver","GUI Manager",
    "RAM Filesystem","Command Shell"
};

/* ── state ───────────────────────────────────────────── */
static int sm_id  = -1;
static int _tab   = 0;    /* 0=Overview  1=Processes */
static int _sm_w  = SM_WIN_W;

/* ── number helpers ──────────────────────────────────── */
static void _u64str(uint64_t n, char* b) {
    if (!n) { b[0]='0'; b[1]='\0'; return; }
    char t[22]; int i=0;
    while (n && i<20) { t[i++]='0'+(int)(n%10); n/=10; }
    int j=0; for(int k=i-1;k>=0;k--) b[j++]=t[k]; b[j]='\0';
}

static void _smartsize(uint64_t bytes, char* b) {
    if (bytes >= 1024ULL*1024ULL) {
        _u64str(bytes/(1024ULL*1024ULL), b);
        int i=0; while(b[i]) i++;
        b[i++]=' '; b[i++]='M'; b[i++]='B'; b[i]='\0';
    } else if (bytes >= 1024) {
        _u64str(bytes/1024, b);
        int i=0; while(b[i]) i++;
        b[i++]=' '; b[i++]='K'; b[i++]='B'; b[i]='\0';
    } else {
        _u64str(bytes, b);
        int i=0; while(b[i]) i++;
        b[i++]=' '; b[i++]='B'; b[i]='\0';
    }
}

static void _u32str2(uint32_t n, char* b) {
    _u64str((uint64_t)n, b);
}

/* concatenate: dst += src (up to room=maxlen-strlen(dst)) */
static void _cat(char* dst, const char* src) {
    int i=0; while(dst[i]) i++;
    int j=0; while(src[j]) dst[i++]=src[j++]; dst[i]='\0';
}

/* ── bar draw ─────────────────────────────────────────── */
static void _bar(int x, int y, int w, int h, int pct, uint32_t clr) {
    draw_rect(x, y, w, h, RGB(12,15,26));
    draw_rect_outline(x, y, w, h, RGB(45,55,85));
    int fill = (w-2)*pct/100;
    if (fill < 0) fill = 0;
    if (fill > w-2) fill = w-2;
    if (fill > 0) draw_rect(x+1, y+1, fill, h-2, clr);
}

/* ── section label ────────────────────────────────────── */
static void _sec(int x, int y, int w, const char* label) {
    draw_rect(x, y, w, SM_SEC_H, RGB(13,16,28));
    draw_string(x+SM_PAD, y+2, label, g_theme.accent, COLOR_TRANSPARENT);
}

/* ── tab bar ──────────────────────────────────────────── */
static void _draw_tabs(int x, int y, int w) {
    draw_rect(x, y, w, SM_TAB_H, RGB(15,18,30));
    draw_line(x, y+SM_TAB_H-1, x+w, y+SM_TAB_H-1, RGB(45,55,85));
    static const char* labels[2] = {"Overview","Processes"};
    for (int i = 0; i < 2; i++) {
        int tx = x + i * SM_TAB_W;
        int active = (_tab == i);
        uint32_t bg = active ? g_theme.accent : RGB(20,24,38);
        draw_rect(tx, y, SM_TAB_W, SM_TAB_H-1, bg);
        draw_string(tx+8, y+5, labels[i],
                    active ? COLOR_WHITE : g_theme.text_secondary, COLOR_TRANSPARENT);
        if (!active)
            draw_line(tx+SM_TAB_W-1, y, tx+SM_TAB_W-1, y+SM_TAB_H-2, RGB(45,55,85));
    }
}

/* ── Overview tab ─────────────────────────────────────── */
static void _draw_overview(int x, int y, int w, int h) {
    (void)h;
    int p = SM_PAD;
    int cy = y + 4;

    /* ── Physical RAM ── */
    _sec(x, cy, w, "Physical RAM"); cy += SM_SEC_H;
    if (sys_phys_ram_kb > 0) {
        /* "used" = kernel est. + heap + framebuffer */
        uint64_t used_kb  = 8192 + (uint64_t)(heap_used_bytes()/1024)
                           + 3072;   /* 1024*768*4 / 1024 = 3072 KB fb */
        if (used_kb > sys_phys_ram_kb) used_kb = sys_phys_ram_kb;
        int pct = (int)(used_kb * 100 / sys_phys_ram_kb);
        _bar(x+p, cy, w-2*p, SM_BAR_H, pct, RGB(80,160,255)); cy += SM_BAR_H+4;
        char us[20], ts[20];
        _smartsize(used_kb*1024, us);
        _smartsize(sys_phys_ram_kb*1024, ts);
        char ln[64]; ln[0]='\0'; _cat(ln,"Used: "); _cat(ln,us);
        _cat(ln,"  /  Total: "); _cat(ln,ts);
        draw_string(x+p, cy, ln, g_theme.text_secondary, COLOR_TRANSPARENT); cy+=16;
    } else {
        draw_string(x+p, cy, "No GRUB memory info", RGB(120,60,60), COLOR_TRANSPARENT);
        cy += 16;
    }
    cy += 6;

    /* ── Heap (kernel allocator) ── */
    _sec(x, cy, w, "Heap Allocator  (4 MB region)"); cy += SM_SEC_H;
    uint64_t hused = heap_used_bytes();
    uint64_t hfree = heap_free_bytes();
    uint64_t htotal = (uint64_t)HEAP_SIZE;
    int hpct = (htotal > 0) ? (int)(hused*100/htotal) : 0;
    _bar(x+p, cy, w-2*p, SM_BAR_H, hpct, RGB(70,200,70)); cy += SM_BAR_H+4;
    {
        char us[20], fs[20];
        _smartsize(hused, us); _smartsize(hfree, fs);
        char ln[64]; ln[0]='\0';
        _cat(ln,"Used: "); _cat(ln,us);
        _cat(ln,"   Free: "); _cat(ln,fs);
        draw_string(x+p, cy, ln, g_theme.text_secondary, COLOR_TRANSPARENT); cy+=16;
    }
    cy += 6;

    /* ── CPU ── */
    _sec(x, cy, w, "CPU"); cy += SM_SEC_H;
    _bar(x+p, cy, w-2*p, SM_BAR_H, 2, RGB(100,130,255)); cy += SM_BAR_H+4;
    draw_string(x+p, cy, "x86_64  ~2 % (idle estimate)",
                g_theme.text_secondary, COLOR_TRANSPARENT); cy += 16;
    cy += 6;

    /* ── Uptime ── */
    _sec(x, cy, w, "Uptime"); cy += SM_SEC_H;
    {
        uint32_t ticks = timer_get_ticks();
        uint32_t secs  = ticks / 100;
        uint32_t mins  = secs / 60; secs %= 60;
        uint32_t hrs   = mins / 60; mins %= 60;
        char buf[40]; buf[0]='\0';
        char ns[12];
        if (hrs > 0) { _u32str2(hrs,ns); _cat(buf,ns); _cat(buf,"h  "); }
        _u32str2(mins,ns); _cat(buf,ns); _cat(buf,"m  ");
        _u32str2(secs,ns); _cat(buf,ns); _cat(buf,"s");
        draw_string(x+p, cy, buf, g_theme.text_primary, COLOR_TRANSPARENT); cy+=16;
    }
    cy += 6;

    /* ── FS Storage ── */
    _sec(x, cy, w, "Filesystem  (RAM)"); cy += SM_SEC_H;
    int node_used = 0;
    uint64_t content_bytes = 0;
    for (int n = 0; n < fs_node_count; n++) {
        if (fs_tree[n].type != 0) node_used++;
        if (fs_tree[n].type == NODE_TYPE_FILE)
            content_bytes += (uint64_t)fs_tree[n].content_len;
    }
    int npct = (MAX_NODES > 0) ? node_used*100/MAX_NODES : 0;
    _bar(x+p, cy, w-2*p, SM_BAR_H, npct, RGB(190,130,60)); cy += SM_BAR_H+4;
    {
        char nn[8], nb[20];
        _u32str2((uint32_t)node_used, nn); _smartsize(content_bytes, nb);
        char ln[64]; ln[0]='\0';
        _cat(ln,"Nodes: "); _cat(ln,nn); _cat(ln," / 64");
        _cat(ln,"   Content: "); _cat(ln,nb);
        draw_string(x+p, cy, ln, g_theme.text_secondary, COLOR_TRANSPARENT);
    }
}

/* ── Processes tab ────────────────────────────────────── */
static void _draw_procs(int x, int y, int w, int h) {
    (void)h;
    int p = SM_PAD;
    int cy = y + 2;

    /* system processes */
    _sec(x, cy, w, "SYSTEM PROCESSES  (Protected)"); cy += SM_SEC_H;
    for (int i = 0; i < KPROC_CNT; i++) {
        uint32_t row_bg = (i%2==0) ? RGB(15,18,30) : RGB(18,22,36);
        draw_rect(x, cy, w, SM_ROW, row_bg);
        gfx_fill_circle(x+p+4, cy+SM_ROW/2, 4, RGB(70,200,90));
        draw_string(x+p+14, cy+2, kp_name[i], g_theme.text_primary,   COLOR_TRANSPARENT);
        draw_string(x+p+90, cy+2, kp_desc[i], g_theme.text_secondary, COLOR_TRANSPARENT);
        draw_string(x+w-68,  cy+2, "Protected",  RGB(80,85,115),       COLOR_TRANSPARENT);
        cy += SM_ROW;
    }
    cy += 4;

    /* app processes (open windows) */
    _sec(x, cy, w, "APP PROCESSES"); cy += SM_SEC_H;
    int app_cnt = 0;
    for (int id = 0; id < MAX_WINDOWS; id++) {
        char title[32]; title[0]='\0';
        if (!win_get_title(id, title, 32)) continue;
        app_cnt++;
        uint32_t row_bg = (app_cnt%2==0) ? RGB(15,18,30) : RGB(18,22,36);
        draw_rect(x, cy, w, SM_ROW, row_bg);
        gfx_fill_circle(x+p+4, cy+SM_ROW/2, 4, g_theme.accent);
        draw_string(x+p+14, cy+2, title, g_theme.text_primary, COLOR_TRANSPARENT);
        draw_string(x+p+220, cy+2, "App", g_theme.text_secondary, COLOR_TRANSPARENT);
        /* End button */
        int bx = x+w-SM_PAD-SM_BTN_W;
        int by = cy+(SM_ROW-SM_BTN_H)/2;
        gfx_fill_rounded_rect(bx, by, SM_BTN_W, SM_BTN_H, 2, RGB(160,40,40));
        draw_string(bx+6, by+2, "End", COLOR_WHITE, COLOR_TRANSPARENT);
        cy += SM_ROW;
    }
    if (app_cnt == 0) {
        draw_string(x+p, cy+2, "(no app windows open)",
                    RGB(60,65,90), COLOR_TRANSPARENT);
    }
}

/* ── top-level draw ──────────────────────────────────── */
static void _draw(int x, int y, int w, int h) {
    _sm_w = w;
    draw_rect(x, y, w, h, g_theme.win_bg);
    _draw_tabs(x, y, w);
    if (_tab == 0)
        _draw_overview(x, y+SM_TAB_H, w, h-SM_TAB_H);
    else
        _draw_procs   (x, y+SM_TAB_H, w, h-SM_TAB_H);
}

/* ── click handler ────────────────────────────────────── */
static void _click(int rx, int ry) {
    /* tab bar */
    if (ry < SM_TAB_H) {
        if (rx < SM_TAB_W) _tab = 0;
        else if (rx < 2*SM_TAB_W) _tab = 1;
        gui_render(); return;
    }

    /* processes tab: End button hit-test */
    if (_tab == 1) {
        /* mirror _draw_procs layout */
        int cy = SM_TAB_H + 2 + SM_SEC_H + KPROC_CNT*SM_ROW + 4 + SM_SEC_H;
        for (int id = 0; id < MAX_WINDOWS; id++) {
            char title[32]; title[0]='\0';
            if (!win_get_title(id, title, 32)) continue;
            int bx = _sm_w - SM_PAD - SM_BTN_W;
            int by = cy + (SM_ROW - SM_BTN_H)/2;
            if (rx >= bx && rx < bx+SM_BTN_W && ry >= by && ry < by+SM_BTN_H) {
                win_close(id);
                if (id == sm_id) sm_id = -1;
                gui_render(); return;
            }
            cy += SM_ROW;
        }
    }
}

/* ── open ────────────────────────────────────────────── */
void app_sysmon_open(void) {
    if (win_is_open(sm_id)) { win_set_focus(sm_id); return; }
    _tab = 0;
    sm_id = win_create(200, 60, SM_WIN_W, SM_WIN_H, "System Monitor",
                       WIN_FLAG_DRAGGABLE|WIN_FLAG_CLOSABLE|WIN_FLAG_RESIZABLE);

    win_set_min_size(sm_id, 320, 280);
    win_set_content(sm_id, _draw);
    win_set_click(sm_id, _click);
}
