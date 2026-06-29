#include "kernel.h"
#include "drivers/screen.h"
#include "gfx/gfx.h"
#include "gui/gui.h"
#include "gui/theme.h"
#include "gui/window.h"
#include "apps/calculator.h"

static int calc_id = -1;

/* ── state ── */
static char  disp[20];
static int   disp_len;
static long long val_a;
static char  op;
static int   fresh;

/* ── helpers ── */
static void _itos(long long n, char* b) {
    if (!n) { b[0]='0'; b[1]='\0'; return; }
    char t[22]; int i=0, neg=(n<0);
    if (neg) n=-n;
    while (n&&i<20) { t[i++]='0'+(int)(n%10); n/=10; }
    if (neg) t[i++]='-';
    int j=0; for(int k=i-1;k>=0;k--) b[j++]=t[k]; b[j]='\0';
}
static long long _stoi(const char* s) {
    long long r=0; int neg=0,i=0;
    if (s[0]=='-') { neg=1; i=1; }
    for (;s[i];i++) if(s[i]>='0'&&s[i]<='9') r=r*10+(s[i]-'0');
    return neg?-r:r;
}

/* ── layout ── */
#define BW 48
#define BH 34
#define BP  6
#define ROWS 5
#define COLS 4

static const char* _btns[ROWS][COLS] = {
    {"C", "+/-", "%",  "/"},
    {"7", "8",   "9",  "*"},
    {"4", "5",   "6",  "-"},
    {"1", "2",   "3",  "+"},
    {"0", ".",   "=",  "<"},
};

/* ── button press ── */
static void _press(const char* b) {
    char ch = b[0];
    if (ch=='C') {
        val_a=0; op=0; fresh=1; disp[0]='0'; disp[1]='\0'; disp_len=1;
    } else if (ch=='+'&&b[1]=='/') {
        if (disp[0]=='-') {
            int i=0; while(disp[i+1]){disp[i]=disp[i+1];i++;} disp[i]='\0'; disp_len--;
        } else {
            int i=disp_len; for(;i>=0;i--) disp[i+1]=disp[i]; disp[0]='-'; disp_len++;
        }
    } else if (ch=='%') {
        long long v=_stoi(disp); _itos(v/100,disp);
        disp_len=0; while(disp[disp_len]) disp_len++; fresh=1;
    } else if (ch=='<') {
        if (disp_len>1) { disp[--disp_len]='\0'; }
        else { disp[0]='0'; disp[1]='\0'; disp_len=1; }
    } else if (ch=='+'||ch=='-'||ch=='*'||ch=='/') {
        val_a=_stoi(disp); op=ch; fresh=1;
    } else if (ch=='=') {
        long long vb=_stoi(disp),res=val_a;
        if (op=='+') res=val_a+vb;
        else if (op=='-') res=val_a-vb;
        else if (op=='*') res=val_a*vb;
        else if (op=='/'&&vb) res=val_a/vb;
        _itos(res,disp); disp_len=0; while(disp[disp_len]) disp_len++;
        op=0; fresh=1; val_a=0;
    } else if (ch>='0'&&ch<='9') {
        if (fresh||(disp_len==1&&disp[0]=='0')) {
            disp[0]=ch; disp[1]='\0'; disp_len=1; fresh=0;
        } else if (disp_len<18) { disp[disp_len++]=ch; disp[disp_len]='\0'; }
    } else if (ch=='.') {
        int hd=0; for(int i=0;disp[i];i++) if(disp[i]=='.') hd=1;
        if (!hd&&disp_len<18) { disp[disp_len++]='.'; disp[disp_len]='\0'; }
    }
}

/* ── content draw ── */
static void _draw(int x, int y, int w, int h) {
    (void)h;
    /* display */
    draw_rect(x+4, y+4, w-8, 38, RGB(10,12,22));
    draw_rect_outline(x+4, y+4, w-8, 38, RGB(55,65,105));
    int tx = x+w-12-disp_len*8;
    draw_string(tx, y+15, disp, RGB(200,220,255), COLOR_TRANSPARENT);

    /* op indicator */
    if (op) {
        char obs[2]={op,'\0'};
        draw_string(x+8, y+18, obs, g_theme.accent, COLOR_TRANSPARENT);
    }

    /* buttons */
    for (int r=0;r<ROWS;r++) for (int c=0;c<COLS;c++) {
        const char* lbl=_btns[r][c];
        int bx=x+BP+c*(BW+BP), by=y+50+r*(BH+BP);
        uint32_t bg=(lbl[0]>='0'&&lbl[0]<='9')?RGB(38,42,58)
                   :(lbl[0]=='+'||lbl[0]=='-'||lbl[0]=='*'||lbl[0]=='/')?RGB(55,35,95)
                   :(lbl[0]=='=')?g_theme.accent
                   :(lbl[0]=='C')?RGB(120,35,35):RGB(45,50,75);
        gfx_fill_rounded_rect(bx,by,BW,BH,4,bg);
        int ll=0; while(lbl[ll]) ll++;
        draw_string(bx+(BW-ll*8)/2, by+(BH-16)/2, lbl, COLOR_WHITE, COLOR_TRANSPARENT);
    }
}

/* ── click handler ── */
static void _click(int rx, int ry) {
    ry -= 50;
    if (ry<0) return;
    int row=ry/(BH+BP);
    if (row>=ROWS) return;
    for (int c=0;c<COLS;c++) {
        int bx=BP+c*(BW+BP);
        if (rx>=bx&&rx<bx+BW) { _press(_btns[row][c]); return; }
    }
}

void app_calculator_open(void) {
    if (win_is_open(calc_id)) { win_set_focus(calc_id); return; }
    int cw=COLS*(BW+BP)+BP;
    int ch=50+ROWS*(BH+BP)+BP;
    calc_id=win_create(180,60,cw,ch+24,"Calculator",
                       WIN_FLAG_DRAGGABLE|WIN_FLAG_CLOSABLE|WIN_FLAG_RESIZABLE);
    win_set_min_size(calc_id, cw, ch + 24);
    win_set_content(calc_id,_draw);
    win_set_click(calc_id,_click);
    disp[0]='0'; disp[1]='\0'; disp_len=1; val_a=0; op=0; fresh=1;
}
