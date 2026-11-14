#ifndef DSOBD_PSP_GFX_H
#define DSOBD_PSP_GFX_H

#include <stdint.h>
#include <stdbool.h>

#define PSP_SCR_W 480
#define PSP_SCR_H 272
#define PSP_BUF_W 512

/* PSP GU_COLOR_8888 native byte order is ABGR8888 (0xAABBGGRR) */
#define PSP_RGBA(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))

#define CLR_BG          PSP_RGBA(6, 13, 24, 255)
#define CLR_BLACK       PSP_RGBA(0, 0, 0, 255)
#define CLR_PANEL       PSP_RGBA(13, 24, 42, 242)
#define CLR_PANEL_DARK  PSP_RGBA(8, 15, 27, 245)
#define CLR_PANEL_HI    PSP_RGBA(22, 42, 74, 248)
#define CLR_BORDER      PSP_RGBA(31, 58, 96, 255)
#define CLR_BORDER_HL   PSP_RGBA(58, 130, 210, 255)
#define CLR_CYAN        PSP_RGBA(56, 189, 248, 255)
#define CLR_GT_BLUE     PSP_RGBA(64, 156, 255, 255)
#define CLR_WHITE       PSP_RGBA(248, 250, 252, 255)
#define CLR_GRAY        PSP_RGBA(134, 152, 178, 255)
#define CLR_DARK_GRAY   PSP_RGBA(24, 38, 62, 255)
#define CLR_GREEN       PSP_RGBA(16, 185, 129, 255)
#define CLR_GREEN_BG    PSP_RGBA(8, 44, 34, 240)
#define CLR_YELLOW      PSP_RGBA(245, 184, 11, 255)
#define CLR_YELLOW_BG   PSP_RGBA(46, 35, 8, 240)
#define CLR_ORANGE      PSP_RGBA(249, 115, 22, 255)
#define CLR_RED         PSP_RGBA(239, 68, 68, 255)
#define CLR_RED_BG      PSP_RGBA(52, 14, 18, 240)
#define CLR_PURPLE      PSP_RGBA(192, 132, 252, 255)
#define CLR_LCD_ON      PSP_RGBA(255, 174, 28, 255)
#define CLR_LCD_BG      PSP_RGBA(18, 10, 2, 255)
#define CLR_LCD_GHOST   PSP_RGBA(56, 30, 6, 255)

enum {
    UI_ALIGN_LEFT = 0,
    UI_ALIGN_CENTER = 1,
    UI_ALIGN_RIGHT = 2
};

bool gfx_psp_init(void);
void gfx_psp_fini(void);
void gfx_psp_begin_frame(uint32_t clear_color);
void gfx_psp_end_frame(void);

void ui_draw_deck(void);
void ui_draw_dial(float cx, float cy, float radius);
void ui_draw_gear(float x, float y, float size, uint32_t color);
void ui_draw_wifi(float x, float y, float size, uint32_t color);
void ui_draw_mil(float x, float y, float size, uint32_t color);

void ui_rect_solid(float x, float y, float w, float h, uint32_t color);
void ui_rect_grad(float x, float y, float w, float h, uint32_t top_col, uint32_t bot_col);
void ui_panel(float x, float y, float w, float h, uint32_t fill, uint32_t border);
void ui_circle(float cx, float cy, float r, uint32_t color, int segs);
void ui_line_thick(float x0, float y0, float x1, float y1, float thickness, uint32_t color);

void ui_hbar(float x, float y, float w, float h, float frac, uint32_t fill, uint32_t bg);
void ui_vbar(float x, float y, float w, float h, float frac, uint32_t fill, uint32_t bg);

void ui_arc_band(float cx, float cy, float r0, float r1, float start_deg, float sweep_deg,
                 float f0, float f1, uint32_t color, int steps);
void ui_radial_tick(float cx, float cy, float r0, float r1, float deg, float thickness, uint32_t color);
void ui_needle(float cx, float cy, float r_tail, float r_tip, float deg, float half_w, uint32_t color);

/* PlayStation face/shoulder button glyph pills rendered crisply with GU primitives */
void ui_psp_btn_badge(float x, float y, const char *glyph, const char *label, uint32_t btn_col);

void ui_text(float x, float y, float scale, uint32_t color, int align, const char *str);
void ui_textf(float x, float y, float scale, uint32_t color, int align, const char *fmt, ...)
    __attribute__((format(printf, 6, 7)));
void ui_text_clipped(float x, float y, float max_w, float scale, uint32_t color, const char *str);
float ui_text_width(float scale, const char *str);

void ui_num(float x, float y, float scale, uint32_t color, int align, const char *str);
void ui_numf(float x, float y, float scale, uint32_t color, int align, const char *fmt, ...)
    __attribute__((format(printf, 6, 7)));

#endif // DSOBD_PSP_GFX_H
