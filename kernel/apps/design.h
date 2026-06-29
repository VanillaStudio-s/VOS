#ifndef DESIGN_H
#define DESIGN_H

#include "kernel.h"

/* ── design.h / design.c ────────────────────────────────────────────────────
 *
 *  Single source of truth for ALL visual/design settings.
 *  Both settings.c and the Customizer app read/write through this API.
 *
 *  On startup call design_init() from gui_init().
 *  Each setter immediately applies its change to the relevant subsystem.
 *
 * ─────────────────────────────────────────────────────────────────────────── */

/* ── Accent color ─────────────────────────────────────────────────────────── */
void     design_set_accent(uint32_t rgb);
uint32_t design_get_accent(void);

/* ── Desktop background ───────────────────────────────────────────────────── */
void     design_set_desktop_color(uint32_t rgb);
uint32_t design_get_desktop_color(void);

/* ── Window appearance ────────────────────────────────────────────────────── */
void design_set_win_rounded (int v);  /* 0/1  */
int  design_get_win_rounded (void);
void design_set_win_shadow  (int v);  /* 0/1  */
int  design_get_win_shadow  (void);
void design_set_win_border  (int v);  /* 0/1  */
int  design_get_win_border  (void);
void design_set_transparency(int v);  /* 0–4  */
int  design_get_transparency(void);

/* ── Mouse ────────────────────────────────────────────────────────────────── */
void design_set_mouse_theme(int theme); /* MOUSE_THEME_*  */
int  design_get_mouse_theme(void);
void design_set_cursor_type(int type);  /* CURSOR_*       */
int  design_get_cursor_type(void);

/* ── Start menu ───────────────────────────────────────────────────────────── */
void design_set_tile_mode(int v);   /* 0=list  1=tile */
int  design_get_tile_mode(void);

/* ── Init ─────────────────────────────────────────────────────────────────── */
void design_init(void);

#endif