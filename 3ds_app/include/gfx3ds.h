#ifndef DSOBD_3DS_GFX3DS_H
#define DSOBD_3DS_GFX3DS_H

#include <3ds.h>
#include <citro2d.h>
#include <stdbool.h>

#define TOP_W 400
#define TOP_H 240
#define BOT_W 320
#define BOT_H 240

#define CLR_BG         C2D_Color32(6, 13, 24, 255)
#define CLR_BLACK      C2D_Color32(0, 0, 0, 255)
#define CLR_PANEL      C2D_Color32(13, 24, 42, 242)
#define CLR_PANEL_DARK C2D_Color32(8, 15, 27, 245)
#define CLR_PANEL_HI   C2D_Color32(20, 37, 64, 245)
#define CLR_BORDER     C2D_Color32(31, 58, 96, 255)
#define CLR_BORDER_HL  C2D_Color32(58, 130, 210, 255)
#define CLR_CYAN       C2D_Color32(56, 189, 248, 255)
#define CLR_GT_BLUE    C2D_Color32(64, 156, 255, 255)
#define CLR_WHITE      C2D_Color32(248, 250, 252, 255)
#define CLR_GRAY       C2D_Color32(130, 148, 174, 255)
#define CLR_DARK_GRAY  C2D_Color32(24, 38, 62, 255)
#define CLR_GREEN      C2D_Color32(16, 185, 129, 255)
#define CLR_GREEN_BG   C2D_Color32(8, 44, 34, 240)
#define CLR_YELLOW     C2D_Color32(245, 184, 11, 255)
#define CLR_YELLOW_BG  C2D_Color32(46, 35, 8, 240)
#define CLR_ORANGE     C2D_Color32(249, 115, 22, 255)
#define CLR_RED        C2D_Color32(239, 68, 68, 255)
#define CLR_RED_BG     C2D_Color32(50, 14, 18, 240)
#define CLR_PURPLE     C2D_Color32(192, 132, 252, 255)
#define CLR_LCD_ON     C2D_Color32(255, 174, 28, 255)
#define CLR_LCD_BG     C2D_Color32(18, 10, 2, 255)
#define CLR_LCD_GHOST  C2D_Color32(56, 30, 6, 255)

bool gfx3ds_init(void);
void gfx3ds_fini(void);
void gfx3ds_begin_frame(void);

void ui_draw_deck_top(void);
void ui_draw_deck_bottom(void);
void ui_draw_dial_large(float cx, float cy, float radius);
void ui_draw_dial_small(float cx, float cy, float radius);
void ui_draw_gear(float x, float y, float size, u32 color);

void ui_panel(float x, float y, float w, float h, u32 fill, u32 border);
void ui_button(float x, float y, float w, float h, const char *label, u32 label_col, bool active);
void ui_hbar(float x, float y, float w, float h, float frac, u32 fill, u32 bg);
void ui_vbar(float x, float y, float w, float h, float frac, u32 fill, u32 bg);

void ui_arc_band(float cx, float cy, float r0, float r1, float start_deg, float sweep_deg,
                 float f0, float f1, u32 color, int steps);
void ui_radial_tick(float cx, float cy, float r0, float r1, float deg, float thickness, u32 color);
void ui_needle(float cx, float cy, float r_tail, float r_tip, float deg, float half_w, u32 color);

void ui_text(float x, float y, float scale, u32 color, u32 flags, const char *str);
void ui_textf(float x, float y, float scale, u32 color, u32 flags, const char *fmt, ...)
    __attribute__((format(printf, 6, 7)));
void ui_text_clipped(float x, float y, float max_w, float scale, u32 color, const char *str);
float ui_text_width(float scale, const char *str);

void ui_num(float x, float y, float scale, u32 color, u32 flags, const char *str);
void ui_numf(float x, float y, float scale, u32 color, u32 flags, const char *fmt, ...)
    __attribute__((format(printf, 6, 7)));
void ui_num_dims(float scale, const char *str, float *w, float *h);

#endif // DSOBD_3DS_GFX3DS_H
