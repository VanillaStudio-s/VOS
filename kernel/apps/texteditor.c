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

static int te_id = -1;

#define TE_MAX   2048
#define TE_FW    8
#define TE_FH    16
#define TE_MX    8   /* left margin */
#define TE_MY    22  /* top margin (below header bar) */

static char te_buf[TE_MAX];
static int  te_len, te_cursor, te_scroll;
static char te_fname[32];
static int  te_modified;

/* ── helpers ── */
static void _te_strcpy(char* d, const char* s, int max) {
    int i=0; while(s[i]&&i<max-1){d[i]=s[i];i++;} d[i]='\0';
}

/* find line/col of cursor position */
static int _te_line_of(int pos) {
    int ln=0; for(int i=0;i<pos;i++) if(te_buf[i]=='\n') ln++;
    return ln;
}
static int _te_line_start(int ln) {
    int cur=0,l=0;
    while(cur<te_len&&l<ln) { if(te_buf[cur++]=='\n') l++; }
    return cur;
}
static int _te_line_end(int pos) {
    int p=pos; while(p<te_len&&te_buf[p]!='\n') p++; return p;
}
static int _te_col_of(int pos) {
    int c=0; while(pos>0&&te_buf[pos-1]!='\n'){pos--;c++;} return c;
}

/* insert/delete */
static void _te_insert(char c) {
    if (te_len>=TE_MAX-1) return;
    for(int i=te_len;i>te_cursor;i--) te_buf[i]=te_buf[i-1];
    te_buf[te_cursor++]=c; te_len++; te_buf[te_len]='\0';
    te_modified=1;
}
static void _te_delete(void) {
    if (te_cursor<=0) return;
    te_cursor--;
    for(int i=te_cursor;i<te_len-1;i++) te_buf[i]=te_buf[i+1];
    te_len--; te_buf[te_len]='\0'; te_modified=1;
}

/* save to FS */
static void _te_save(void) {
    if (!te_fname[0]) { _te_strcpy(te_fname,"untitled.txt",32); }
    /* find or create file */
    int fi=-1;
    for (int i=1;i<fs_node_count;i++)
        if (fs_tree[i].type==NODE_TYPE_FILE && fs_tree[i].parent_idx==current_dir_idx
            && my_strcmp(fs_tree[i].name,te_fname)==0) { fi=i; break; }
    if (fi<0) fi=fs_node_count<MAX_NODES ? (int)(fs_node_count) : -1;
    if (fi<0) { notify_push("Editor","Filesystem full",NOTIFY_ERROR); return; }
    if (fi==fs_node_count) {
        /* create node */
        fs_tree[fi].type=NODE_TYPE_FILE;
        fs_tree[fi].parent_idx=current_dir_idx;
        int k=0; while(te_fname[k]&&k<31){fs_tree[fi].name[k]=te_fname[k];k++;} fs_tree[fi].name[k]='\0';
        fs_tree[fi].content=(char*)0;
        fs_tree[fi].content_len=0;
        fs_node_count++;
    }
    /* (re)allocate heap content */
    if (fs_tree[fi].content) kfree(fs_tree[fi].content);
    int len=te_len>FS_CONTENT_MAX-1?FS_CONTENT_MAX-1:te_len;
    fs_tree[fi].content=(char*)kmalloc(len+1);
    if (fs_tree[fi].content) {
        int j=0; while(te_buf[j]&&j<len){fs_tree[fi].content[j]=te_buf[j];j++;}
        fs_tree[fi].content[j]='\0';
        fs_tree[fi].content_len=j;
    } else {
        fs_tree[fi].content_len=0;
    }
    te_modified=0;
    notify_push("Editor","File saved",NOTIFY_SUCCESS);
}

/* ── draw ── */
static void _draw(int x, int y, int w, int h) {
    /* header bar */
    gfx_fill_rounded_rect(x+2, y+2, w-4, 18, 2, RGB(18,20,32));
    const char* title = te_fname[0] ? te_fname : "untitled.txt";
    draw_string(x+6, y+4, title, g_theme.text_secondary, COLOR_TRANSPARENT);
    if (te_modified) draw_string(x+w-20, y+4, "*", RGB(255,180,50), COLOR_TRANSPARENT);

    /* text area background */
    draw_rect(x, y+TE_MY, w, h-TE_MY, RGB(14,16,26));

    /* visible lines */
    int vis_lines=(h-TE_MY)/TE_FH;
    int cur_line=_te_line_of(te_cursor);

    /* auto-scroll */
    if (cur_line < te_scroll) te_scroll=cur_line;
    if (cur_line >= te_scroll+vis_lines) te_scroll=cur_line-vis_lines+1;

    int line=0, col=0, ci=0;
    int draw_y=y+TE_MY;

    while (ci<=te_len && line < te_scroll+vis_lines) {
        if (line >= te_scroll) {
            int draw_x=x+TE_MX+col*TE_FW;
            /* cursor */
            if (ci==te_cursor) {
                draw_rect(draw_x, draw_y, TE_FW, TE_FH, g_theme.accent);
            }
            if (ci<te_len) {
                char c=te_buf[ci];
                if (c!='\n') {
                    uint32_t fg=(ci==te_cursor)?RGB(0,0,0):g_theme.text_primary;
                    draw_char(draw_x, draw_y, c, fg, COLOR_TRANSPARENT);
                }
            }
        }
        if (ci>=te_len) break;
        if (te_buf[ci]=='\n') {
            line++; col=0; if(line>=te_scroll) draw_y+=TE_FH;
        } else {
            col++;
            /* wrap long lines */
            if (x+TE_MX+col*TE_FW+TE_FW > x+w-TE_MX) { line++; col=0; if(line>=te_scroll) draw_y+=TE_FH; }
        }
        ci++;
    }

    /* status bar */
    int sb_y=y+h-16;
    draw_rect(x, sb_y, w, 16, RGB(18,20,32));
    char sbuf[32]; int ln=cur_line+1, cl=_te_col_of(te_cursor)+1;
    int i=0;
    sbuf[i++]='L'; sbuf[i++]='n'; sbuf[i++]=' ';
    /* write ln */
    char ns[8]; int nsi=0;
    if(!ln){ns[0]='1';ns[1]='\0';}else{int v=ln,ni=0;char t[8];while(v){t[ni++]='0'+v%10;v/=10;}for(int k=ni-1;k>=0;k--)ns[nsi++]=t[k];ns[nsi]='\0';}
    for(int k=0;ns[k];k++) sbuf[i++]=ns[k];
    sbuf[i++]=' '; sbuf[i++]='C'; sbuf[i++]='l'; sbuf[i++]=' ';
    nsi=0; if(!cl){ns[0]='1';ns[1]='\0';}else{int v=cl,ni=0;char t[8];while(v){t[ni++]='0'+v%10;v/=10;}for(int k=ni-1;k>=0;k--)ns[nsi++]=t[k];ns[nsi]='\0';}
    for(int k=0;ns[k];k++) sbuf[i++]=ns[k];
    sbuf[i]=' '; sbuf[i+1]='C'; sbuf[i+2]='t'; sbuf[i+3]='r'; sbuf[i+4]='l'; sbuf[i+5]='+'; sbuf[i+6]='S'; sbuf[i+7]='='; sbuf[i+8]='S'; sbuf[i+9]='a'; sbuf[i+10]='v'; sbuf[i+11]='e'; sbuf[i+12]='\0';
    draw_string(x+4, sb_y, sbuf, g_theme.text_secondary, COLOR_TRANSPARENT);
}

/* ── key handler ── */
static void _key(char c, unsigned char sc, int ctrl) {
    if (te_id < 0) return;
    /* Ctrl+S = save */
    if (ctrl && (sc==0x1F)) { _te_save(); gui_render(); return; }
    /* ESC = close */
    if (c==0x1B||sc==0x01) { win_close(te_id); te_id=-1; gui_render(); return; }
    /* arrow keys (extended: c=0, sc=0x48/50/4B/4D) */
    if (c==0) {
        if (sc==0x48) { /* up */
            int ln=_te_line_of(te_cursor), col=_te_col_of(te_cursor);
            if (ln>0) { int ns=_te_line_start(ln-1), ne=_te_line_end(ns); int nc=ns+col; if(nc>ne) nc=ne; te_cursor=nc; }
        } else if (sc==0x50) { /* down */
            int ln=_te_line_of(te_cursor), col=_te_col_of(te_cursor);
            int ns=_te_line_start(ln+1);
            if (ns<=te_len) { int ne=_te_line_end(ns); int nc=ns+col; if(nc>ne) nc=ne; te_cursor=nc; }
        } else if (sc==0x4B&&te_cursor>0) te_cursor--;  /* left */
        else if (sc==0x4D&&te_cursor<te_len) te_cursor++;  /* right */
        gui_render(); return;
    }
    if (c=='\b') { _te_delete(); gui_render(); return; }
    if (c=='\n') { _te_insert('\n'); gui_render(); return; }
    if (c>=' '&&c<127) { _te_insert(c); gui_render(); return; }
}

void app_texteditor_open(const char* filename) {
    if (win_is_open(te_id)) { win_set_focus(te_id); return; }
    /* init buffer */
    te_buf[0]='\0'; te_len=0; te_cursor=0; te_scroll=0; te_modified=0;
    te_fname[0]='\0';

    /* load file if given */
    if (filename && filename[0]) {
        _te_strcpy(te_fname, filename, 32);
        for (int i=1;i<fs_node_count;i++) {
            if (fs_tree[i].type==NODE_TYPE_FILE && fs_tree[i].parent_idx==current_dir_idx
                && my_strcmp(fs_tree[i].name,filename)==0) {
                int j=0;
                if (fs_tree[i].content)
                    while(fs_tree[i].content[j]&&j<TE_MAX-1) { te_buf[j]=fs_tree[i].content[j]; j++; }
                te_buf[j]='\0'; te_len=j; break;
            }
        }
    }

    te_id=win_create(60,30,460,320,"Text Editor",
                     WIN_FLAG_DRAGGABLE|WIN_FLAG_CLOSABLE|WIN_FLAG_RESIZABLE);
    win_set_min_size(te_id, 260, 160);
    win_set_content(te_id,_draw);
    win_set_key(te_id,_key);
}
