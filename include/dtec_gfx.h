#ifndef DTEC_GFX_H
#define DTEC_GFX_H

#include "dtec_common.h"
#include "dtec_assets.h"

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
