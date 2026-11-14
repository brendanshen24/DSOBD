#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "gfx3ds.h"
#include "sprites_3ds.h"
#include "sprites_3ds_t3x.h"
#include "barlow_ui_bcfnt.h"
#include "barlow_num_bcfnt.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static C2D_SpriteSheet s_sheet;
static C2D_Font s_font_ui;
static C2D_Font s_font_num;
static C2D_TextBuf s_textbuf;

static float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static u32 scale_rgba(u32 c, float factor) {
    u32 r = (u32)((c & 0xFF) * factor);
    u32 g = (u32)(((c >> 8) & 0xFF) * factor);
    u32 b = (u32)(((c >> 16) & 0xFF) * factor);
    u32 a = (c >> 24) & 0xFF;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return C2D_Color32((u8)r, (u8)g, (u8)b, (u8)a);
}

bool gfx3ds_init(void) {
    s_sheet = C2D_SpriteSheetLoadFromMem(sprites_3ds_t3x, sprites_3ds_t3x_size);
    if (!s_sheet) return false;

    s_font_ui = C2D_FontLoadFromMem(barlow_ui_bcfnt, barlow_ui_bcfnt_size);
    s_font_num = C2D_FontLoadFromMem(barlow_num_bcfnt, barlow_num_bcfnt_size);
    if (!s_font_ui || !s_font_num) return false;

    C2D_FontSetFilter(s_font_ui, GPU_LINEAR, GPU_LINEAR);
    C2D_FontSetFilter(s_font_num, GPU_LINEAR, GPU_LINEAR);

    s_textbuf = C2D_TextBufNew(8192);
    if (!s_textbuf) return false;
    return true;
}

void gfx3ds_fini(void) {
    if (s_textbuf) { C2D_TextBufDelete(s_textbuf); s_textbuf = NULL; }
    if (s_font_ui) { C2D_FontFree(s_font_ui); s_font_ui = NULL; }
    if (s_font_num) { C2D_FontFree(s_font_num); s_font_num = NULL; }
    if (s_sheet) { C2D_SpriteSheetFree(s_sheet); s_sheet = NULL; }
}

void gfx3ds_begin_frame(void) {
    C2D_TextBufClear(s_textbuf);
}

void ui_draw_deck_top(void) {
    C2D_Image img = C2D_SpriteSheetGetImage(s_sheet, sprites_3ds_deck_top_idx);
    C2D_DrawImageAt(img, 0.0f, 0.0f, 0.0f, NULL, 1.0f, 1.0f);
}

void ui_draw_deck_bottom(void) {
    C2D_Image img = C2D_SpriteSheetGetImage(s_sheet, sprites_3ds_deck_top_idx);
    C2D_DrawImageAt(img, 0.0f, 0.0f, 0.0f, NULL, (float)BOT_W / (float)TOP_W, 1.0f);
}

void ui_draw_dial_large(float cx, float cy, float radius) {
    C2D_Image img = C2D_SpriteSheetGetImage(s_sheet, sprites_3ds_dial_large_idx);
    float scale = (radius * 2.0f) / (float)img.subtex->width;
    C2D_DrawImageAt(img, cx - radius, cy - radius, 0.0f, NULL, scale, scale);
}

void ui_draw_dial_small(float cx, float cy, float radius) {
    C2D_Image img = C2D_SpriteSheetGetImage(s_sheet, sprites_3ds_dial_small_idx);
    float scale = (radius * 2.0f) / (float)img.subtex->width;
    C2D_DrawImageAt(img, cx - radius, cy - radius, 0.0f, NULL, scale, scale);
}

void ui_draw_gear(float x, float y, float size, u32 color) {
    C2D_Image img = C2D_SpriteSheetGetImage(s_sheet, sprites_3ds_gear_idx);
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, 1.0f);
    float scale = size / (float)img.subtex->width;
    C2D_DrawImageAt(img, x, y, 0.0f, &tint, scale, scale);
}

void ui_panel(float x, float y, float w, float h, u32 fill, u32 border) {
    u32 bot = scale_rgba(fill, 0.72f);
    C2D_DrawRectangle(x + 1.0f, y + 1.0f, 0.0f, w - 2.0f, h - 2.0f, fill, fill, bot, bot);

    /* Crisp beveled border + subtle top glass sheen */
    C2D_DrawRectSolid(x + 1.0f, y, 0.0f, w - 2.0f, 1.0f, border);
    C2D_DrawRectSolid(x + 1.0f, y + h - 1.0f, 0.0f, w - 2.0f, 1.0f, border);
    C2D_DrawRectSolid(x, y + 1.0f, 0.0f, 1.0f, h - 2.0f, border);
    C2D_DrawRectSolid(x + w - 1.0f, y + 1.0f, 0.0f, 1.0f, h - 2.0f, border);
    C2D_DrawRectSolid(x + 2.0f, y + 1.0f, 0.0f, w - 4.0f, 1.0f, C2D_Color32(255, 255, 255, 22));
}

void ui_button(float x, float y, float w, float h, const char *label, u32 label_col, bool active) {
    ui_panel(x, y, w, h, active ? CLR_PANEL_HI : CLR_PANEL, active ? CLR_BORDER_HL : CLR_BORDER);
    if (!label || !label[0]) return;
    float scale = 0.58f;
    float tw = 0.0f, th = 0.0f;
    C2D_Text txt;
    C2D_TextFontParse(&txt, s_font_ui, s_textbuf, label);
    C2D_TextOptimize(&txt);
    C2D_TextGetDimensions(&txt, scale, scale, &tw, &th);
    if (tw > w - 8.0f && tw > 1.0f) {
        scale *= (w - 8.0f) / tw;
        C2D_TextGetDimensions(&txt, scale, scale, &tw, &th);
    }
    C2D_DrawText(&txt, C2D_WithColor | C2D_AlignCenter,
                 x + w * 0.5f, y + (h - th) * 0.5f, 0.0f, scale, scale, label_col);
}

void ui_hbar(float x, float y, float w, float h, float frac, u32 fill, u32 bg) {
    float f = clamp01(frac);
    C2D_DrawRectSolid(x, y, 0.0f, w, h, bg);
    if (f > 0.005f) {
        float fw = w * f;
        u32 hi = scale_rgba(fill, 1.15f);
        C2D_DrawRectangle(x, y, 0.0f, fw, h, hi, hi, fill, fill);
        if (fw >= 2.0f) {
            C2D_DrawRectSolid(x + fw - 1.5f, y, 0.0f, 1.5f, h, C2D_Color32(255, 255, 255, 160));
        }
    }
}

void ui_vbar(float x, float y, float w, float h, float frac, u32 fill, u32 bg) {
    float f = clamp01(frac);
    C2D_DrawRectSolid(x, y, 0.0f, w, h, bg);
    if (f > 0.005f) {
        float fh = h * f;
        u32 hi = scale_rgba(fill, 1.15f);
        C2D_DrawRectangle(x, y + h - fh, 0.0f, w, fh, hi, hi, fill, fill);
        C2D_DrawRectSolid(x, y + h - fh, 0.0f, w, 1.5f, C2D_Color32(255, 255, 255, 160));
    }
    C2D_DrawRectSolid(x - 1.0f, y - 1.0f, 0.0f, w + 2.0f, 1.0f, CLR_BORDER);
    C2D_DrawRectSolid(x - 1.0f, y + h, 0.0f, w + 2.0f, 1.0f, CLR_BORDER);
    C2D_DrawRectSolid(x - 1.0f, y, 0.0f, 1.0f, h, CLR_BORDER);
    C2D_DrawRectSolid(x + w, y, 0.0f, 1.0f, h, CLR_BORDER);
}

void ui_arc_band(float cx, float cy, float r0, float r1, float start_deg, float sweep_deg,
                 float f0, float f1, u32 color, int steps) {
    f0 = clamp01(f0);
    f1 = clamp01(f1);
    if (f1 <= f0 || steps < 1) return;

    float a0 = (start_deg + f0 * sweep_deg) * (float)(M_PI / 180.0);
    float a1 = (start_deg + f1 * sweep_deg) * (float)(M_PI / 180.0);
    float da = (a1 - a0) / (float)steps;

    float c_prev = cosf(a0);
    float s_prev = sinf(a0);
    for (int i = 1; i <= steps; i++) {
        float a = a0 + da * (float)i;
        float c_cur = cosf(a);
        float s_cur = sinf(a);

        float ix0 = cx + c_prev * r0, iy0 = cy + s_prev * r0;
        float ox0 = cx + c_prev * r1, oy0 = cy + s_prev * r1;
        float ix1 = cx + c_cur * r0,  iy1 = cy + s_cur * r0;
        float ox1 = cx + c_cur * r1,  oy1 = cy + s_cur * r1;

        C2D_DrawTriangle(ix0, iy0, color, ox0, oy0, color, ox1, oy1, color, 0.0f);
        C2D_DrawTriangle(ix0, iy0, color, ox1, oy1, color, ix1, iy1, color, 0.0f);

        c_prev = c_cur;
        s_prev = s_cur;
    }
}

void ui_radial_tick(float cx, float cy, float r0, float r1, float deg, float thickness, u32 color) {
    float a = deg * (float)(M_PI / 180.0);
    float ca = cosf(a);
    float sa = sinf(a);
    C2D_DrawLine(cx + ca * r0, cy + sa * r0, color, cx + ca * r1, cy + sa * r1, color, thickness, 0.0f);
}

static void draw_tapered_poly(float cx, float cy, float r_tail, float r_tip,
                              float ca, float sa, float base_w, float tip_w, u32 col) {
    float px = -sa;
    float py = ca;

    float bx = cx - ca * r_tail;
    float by = cy - sa * r_tail;
    float tx = cx + ca * r_tip;
    float ty = cy + sa * r_tip;

    float x0 = bx + px * base_w, y0 = by + py * base_w;
    float x1 = bx - px * base_w, y1 = by - py * base_w;
    float x2 = tx - px * tip_w,  y2 = ty - py * tip_w;
    float x3 = tx + px * tip_w,  y3 = ty + py * tip_w;

    C2D_DrawTriangle(x0, y0, col, x1, y1, col, x2, y2, col, 0.0f);
    C2D_DrawTriangle(x0, y0, col, x2, y2, col, x3, y3, col, 0.0f);
}

void ui_needle(float cx, float cy, float r_tail, float r_tip, float deg, float half_w, u32 color) {
    float a = deg * (float)(M_PI / 180.0);
    float ca = cosf(a);
    float sa = sinf(a);

    /* Drop shadow */
    draw_tapered_poly(cx + 2.0f, cy + 2.5f, r_tail, r_tip, ca, sa, half_w + 0.6f, 1.0f,
                      C2D_Color32(0, 0, 0, 125));
    /* Main tapered needle */
    draw_tapered_poly(cx, cy, r_tail, r_tip, ca, sa, half_w, 0.8f, color);
    /* Bright inner spine */
    C2D_DrawLine(cx, cy, C2D_Color32(255, 255, 255, 110),
                 cx + ca * (r_tip - 2.0f), cy + sa * (r_tip - 2.0f), C2D_Color32(255, 255, 255, 60),
                 1.1f, 0.0f);

    /* Brushed center hub cap */
    C2D_DrawCircleSolid(cx + 1.0f, cy + 1.5f, 0.0f, 11.0f, C2D_Color32(0, 0, 0, 120));
    C2D_DrawCircleSolid(cx, cy, 0.0f, 10.5f, color);
    C2D_DrawCircleSolid(cx, cy, 0.0f, 8.5f, C2D_Color32(18, 26, 38, 255));
    C2D_DrawCircleSolid(cx, cy, 0.0f, 4.0f, C2D_Color32(72, 92, 118, 255));
}

static void draw_font_str(C2D_Font font, float x, float y, float scale, u32 color, u32 flags, const char *str) {
    if (!str || !str[0]) return;
    C2D_Text txt;
    C2D_TextFontParse(&txt, font, s_textbuf, str);
    C2D_TextOptimize(&txt);
    C2D_DrawText(&txt, C2D_WithColor | flags, x, y, 0.0f, scale, scale, color);
}

void ui_text(float x, float y, float scale, u32 color, u32 flags, const char *str) {
    draw_font_str(s_font_ui, x, y, scale, color, flags, str);
}

void ui_textf(float x, float y, float scale, u32 color, u32 flags, const char *fmt, ...) {
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    draw_font_str(s_font_ui, x, y, scale, color, flags, buf);
}

float ui_text_width(float scale, const char *str) {
    if (!str || !str[0]) return 0.0f;
    C2D_Text txt;
    C2D_TextFontParse(&txt, s_font_ui, s_textbuf, str);
    float w = 0.0f;
    C2D_TextGetDimensions(&txt, scale, scale, &w, NULL);
    return w;
}

void ui_text_clipped(float x, float y, float max_w, float scale, u32 color, const char *str) {
    if (!str || !str[0]) return;
    float ell_w = 0.0f;
    int ell_idx = C2D_FontGlyphIndexFromCodePoint(s_font_ui, '.');
    charWidthInfo_s *ell_info = C2D_FontGetCharWidthInfo(s_font_ui, ell_idx);
    if (ell_info) ell_w = (float)ell_info->charWidth * scale * 3.0f;

    float total_w = 0.0f;
    size_t len = strlen(str);
    size_t fit_len = len;
    for (size_t i = 0; i < len; i++) {
        int gi = C2D_FontGlyphIndexFromCodePoint(s_font_ui, (unsigned char)str[i]);
        charWidthInfo_s *ci = C2D_FontGetCharWidthInfo(s_font_ui, gi);
        float cw = ci ? (float)ci->charWidth * scale : 8.0f * scale;
        if (total_w + cw > max_w) {
            fit_len = i;
            while (fit_len > 0 && total_w + ell_w > max_w) {
                fit_len--;
                int pi = C2D_FontGlyphIndexFromCodePoint(s_font_ui, (unsigned char)str[fit_len]);
                charWidthInfo_s *pci = C2D_FontGetCharWidthInfo(s_font_ui, pi);
                total_w -= pci ? (float)pci->charWidth * scale : 8.0f * scale;
            }
            break;
        }
        total_w += cw;
    }

    char buf[128];
    if (fit_len < len) {
        size_t copy = fit_len < sizeof(buf) - 4 ? fit_len : sizeof(buf) - 4;
        memcpy(buf, str, copy);
        memcpy(buf + copy, "...", 4);
    } else {
        snprintf(buf, sizeof(buf), "%s", str);
    }
    draw_font_str(s_font_ui, x, y, scale, color, 0, buf);
}

void ui_num(float x, float y, float scale, u32 color, u32 flags, const char *str) {
    draw_font_str(s_font_num, x, y, scale, color, flags, str);
}

void ui_numf(float x, float y, float scale, u32 color, u32 flags, const char *fmt, ...) {
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    draw_font_str(s_font_num, x, y, scale, color, flags, buf);
}

void ui_num_dims(float scale, const char *str, float *w, float *h) {
    C2D_Text txt;
    C2D_TextFontParse(&txt, s_font_num, s_textbuf, str ? str : "");
    C2D_TextGetDimensions(&txt, scale, scale, w, h);
}
