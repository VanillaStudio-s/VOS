#ifndef MOUSE_H
#define MOUSE_H

/* ── Cursor types ────────────────────────────────────────────────────────── */
#define CURSOR_ARROW    0   /* default pointer         */
#define CURSOR_RESIZE   1   /* SE resize arrow         */
#define CURSOR_WAIT     2   /* hourglass / spinner     */
#define CURSOR_POINTER  3   /* hand (clickable item)   */
#define CURSOR_TEXT     4   /* I-beam (text input)     */

/* ── Cursor themes ───────────────────────────────────────────────────────── */
#define MOUSE_THEME_VOS     0   /* VOS default   */
#define MOUSE_THEME_WINDOWS 1   /* Windows style */
#define MOUSE_THEME_BREEZE  2   /* KDE Breeze    */

void mouse_init(void);
int  mouse_get_x(void);
int  mouse_get_y(void);
int  mouse_get_buttons(void);

void mouse_set_cursor(int type);          /* CURSOR_* constant     */
int  mouse_get_cursor(void);

void mouse_set_theme(int theme);          /* MOUSE_THEME_* constant */
int  mouse_get_theme(void);

void mouse_draw_cursor(void);             /* blit to back_buffer    */
void mouse_erase_cursor(void);            /* no-op, kept for compat */
void mouse_blit_cursor_to_vga(void);

#endif