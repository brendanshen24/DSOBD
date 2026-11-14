#ifndef DTEC_GFX_H
#define DTEC_GFX_H

#include "dtec_common.h"
#include "dtec_assets.h"

// Shared top-screen layout. Every screen with a bottom row (hints or pedal bars)
// puts it at UI_FOOT_Y and ends gauges/panels by UI_CONTENT_END, so nothing ever
// runs into it. tests/render_test.c fails if a screen draws in that gap.
#define UI_PAD          8                         // left/right margin
#define UI_FOOT_Y       180                       // bottom row, 8 px tall
#define UI_CONTENT_END  (UI_FOOT_Y - 6)           // 174: content stops above here

void gfx_panel(uint16_t *buf, int x, int y, int w, int h, uint16_t color, uint16_t border);
void gfx_background(uint16_t *buf);
void gfx_dial(uint16_t *buf, int cx, int cy, int radius);
void gfx_gear(uint16_t *buf, int x, int y, uint16_t color);

void gfx_clear(uint16_t *buf, uint16_t color);
void gfx_pixel(uint16_t *buf, int x, int y, uint16_t color);
void gfx_rect(uint16_t *buf, int x, int y, int w, int h, uint16_t color);
void gfx_rect_fill(uint16_t *buf, int x, int y, int w, int h, uint16_t color);
void gfx_line(uint16_t *buf, int x0, int y0, int x1, int y1, uint16_t color);
void gfx_circle(uint16_t *buf, int cx, int cy, int radius, uint16_t color);
void gfx_circle_fill(uint16_t *buf, int cx, int cy, int radius, uint16_t color);

void gfx_draw_char(uint16_t *buf, int x, int y, char c, uint16_t color, int scale);
void gfx_draw_str(uint16_t *buf, int x, int y, const char *str, uint16_t color, int scale);
// Proportional Barlow text. align: 0 left, 1 center, 2 right of x.
int gfx_text_width(const char *str, int scale);
void gfx_draw_str_align(uint16_t *buf, int x, int y, int align, const char *str, uint16_t color, int scale);
void gfx_draw_str_clipped(uint16_t *buf, int x, int y, int max_w, const char *str, uint16_t color, int scale);
void gfx_draw_str_wrap(uint16_t *buf, int x, int y, int max_w, const char *str, uint16_t color, int scale);

void gfx_draw_int(uint16_t *buf, int x, int y, int val, uint16_t color, int scale);
void gfx_draw_float(uint16_t *buf, int x, int y, float val, int dec, uint16_t color, int scale);

// Smooth numeral sprites, right-aligned in `digits` cells of w x h.
// t controls spacing; off is retained for compatibility with the gauge layouts.
// val < 0 shows dashes. skew > 0 slants the digits right (pixels per 16 rows).
void gfx_seg_number(uint16_t *buf, int x, int y, int w, int h, int t, int digits,
                    int val, uint16_t on, uint16_t off, int skew);

#endif // DTEC_GFX_H
