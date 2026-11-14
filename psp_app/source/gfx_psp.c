#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_psp.h"
#include "psp_assets.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    uint32_t color;
    float x, y, z;
} VertCol;

typedef struct {
    float u, v;
    uint32_t color;
    float x, y, z;
} VertTex;

#define VTYPE_COL (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)
#define VTYPE_TEX (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

static unsigned int __attribute__((aligned(16))) s_list[65536];

static void *s_vram_deck = NULL;
static void *s_vram_dial = NULL;
static void *s_vram_gear = NULL;
static void *s_vram_wifi = NULL;
static void *s_vram_mil = NULL;
static void *s_vram_font_ui = NULL;
static void *s_vram_font_num = NULL;

enum {
    TEX_NONE = 0,
    TEX_DECK,
    TEX_DIAL,
    TEX_GEAR,
    TEX_WIFI,
    TEX_MIL,
    TEX_FONT_UI,
    TEX_FONT_NUM
};

static int s_bound_tex = TEX_NONE;
static float s_num_digit_adv = 0.0f;

static float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static uint32_t scale_rgba(uint32_t c, float factor) {
    uint32_t r = (uint32_t)((c & 0xFFu) * factor);
    uint32_t g = (uint32_t)(((c >> 8) & 0xFFu) * factor);
    uint32_t b = (uint32_t)(((c >> 16) & 0xFFu) * factor);
    uint32_t a = (c >> 24) & 0xFFu;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return PSP_RGBA(r, g, b, a);
}

static void *upload_vram_tex(int w, int h, int psm, const void *src, size_t bytes) {
    void *dst = guGetStaticVramTexture(w, h, psm);
    if (!dst) return (void *)src;
    memcpy(dst, src, bytes);
    return dst;
}

bool gfx_psp_init(void) {
    void *fbp0 = guGetStaticVramBuffer(PSP_BUF_W, PSP_SCR_H, GU_PSM_8888);
    void *fbp1 = guGetStaticVramBuffer(PSP_BUF_W, PSP_SCR_H, GU_PSM_8888);

    s_vram_deck     = upload_vram_tex(256, 256, GU_PSM_8888, psp_tex_deck, sizeof(psp_tex_deck));
    s_vram_dial     = upload_vram_tex(256, 256, GU_PSM_8888, psp_tex_dial, sizeof(psp_tex_dial));
    s_vram_gear     = upload_vram_tex(32,  32,  GU_PSM_8888, psp_tex_gear, sizeof(psp_tex_gear));
    s_vram_wifi     = upload_vram_tex(32,  32,  GU_PSM_8888, psp_tex_wifi, sizeof(psp_tex_wifi));
    s_vram_mil      = upload_vram_tex(32,  32,  GU_PSM_8888, psp_tex_mil,  sizeof(psp_tex_mil));
    s_vram_font_ui  = upload_vram_tex(512, 256, GU_PSM_T8,   psp_tex_font_ui,  sizeof(psp_tex_font_ui));
    s_vram_font_num = upload_vram_tex(512, 256, GU_PSM_T8,   psp_tex_font_num, sizeof(psp_tex_font_num));

    for (int i = 0; i < PSP_NUM_GLYPH_COUNT; i++) {
        if (psp_num_glyphs[i].code >= '0' && psp_num_glyphs[i].code <= '9') {
            if (psp_num_glyphs[i].xadvance > s_num_digit_adv) {
                s_num_digit_adv = psp_num_glyphs[i].xadvance;
            }
        }
    }

    sceKernelDcacheWritebackAll();

    sceGuInit();
    sceGuStart(GU_DIRECT, s_list);
    sceGuDrawBuffer(GU_PSM_8888, fbp0, PSP_BUF_W);
    sceGuDispBuffer(PSP_SCR_W, PSP_SCR_H, fbp1, PSP_BUF_W);
    sceGuOffset(2048 - (PSP_SCR_W / 2), 2048 - (PSP_SCR_H / 2));
    sceGuViewport(2048, 2048, PSP_SCR_W, PSP_SCR_H);
    sceGuScissor(0, 0, PSP_SCR_W, PSP_SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuAmbientColor(0xFFFFFFFFu);
    sceGuClearColor(CLR_BG);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    return true;
}

void gfx_psp_fini(void) {
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

void gfx_psp_begin_frame(uint32_t clear_color) {
    s_bound_tex = TEX_NONE;
    sceGuStart(GU_DIRECT, s_list);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuClearColor(clear_color);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

void gfx_psp_end_frame(void) {
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

static void use_untextured(void) {
    if (s_bound_tex != TEX_NONE) {
        sceGuDisable(GU_TEXTURE_2D);
        s_bound_tex = TEX_NONE;
    }
}

static void bind_rgba_tex(int tex_id, int w, int h, const void *ptr, int tfunc) {
    if (s_bound_tex == tex_id) return;
    if (s_bound_tex == TEX_NONE) sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, GU_TRUE);
    sceGuTexImage(0, w, h, w, ptr);
    sceGuTexFunc(tfunc, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    s_bound_tex = tex_id;
}

static void bind_font_tex(int tex_id, const void *ptr) {
    if (s_bound_tex == tex_id) return;
    if (s_bound_tex == TEX_NONE) sceGuEnable(GU_TEXTURE_2D);
    sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
    sceGuClutLoad(256 / 8, psp_clut_alpha);
    sceGuTexMode(GU_PSM_T8, 0, 0, GU_TRUE);
    sceGuTexImage(0, 512, 256, 512, ptr);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    s_bound_tex = tex_id;
}

static void draw_sprite(float u0, float v0, float u1, float v1,
                        float x0, float y0, float x1, float y1, uint32_t col) {
    VertTex *v = (VertTex *)sceGuGetMemory(2 * sizeof(VertTex));
    v[0].u = u0; v[0].v = v0; v[0].color = col; v[0].x = x0; v[0].y = y0; v[0].z = 0.0f;
    v[1].u = u1; v[1].v = v1; v[1].color = col; v[1].x = x1; v[1].y = y1; v[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, VTYPE_TEX, 2, NULL, v);
}

void ui_draw_deck(void) {
    bind_rgba_tex(TEX_DECK, 256, 256, s_vram_deck, GU_TFX_REPLACE);
    /* Slice in 64px vertical strips for optimal PSP texture cache hit rate */
    for (int sx = 0; sx < PSP_SCR_W; sx += 64) {
        int sw = (sx + 64 <= PSP_SCR_W) ? 64 : (PSP_SCR_W - sx);
        float u0 = (float)sx * (256.0f / (float)PSP_SCR_W);
        float u1 = (float)(sx + sw) * (256.0f / (float)PSP_SCR_W);
        draw_sprite(u0, 0.0f, u1, 256.0f, (float)sx, 0.0f, (float)(sx + sw), (float)PSP_SCR_H, 0xFFFFFFFFu);
    }
}

void ui_draw_dial(float cx, float cy, float radius) {
    bind_rgba_tex(TEX_DIAL, 256, 256, s_vram_dial, GU_TFX_REPLACE);
    draw_sprite(0.0f, 0.0f, 256.0f, 256.0f, cx - radius, cy - radius, cx + radius, cy + radius, 0xFFFFFFFFu);
}

void ui_draw_gear(float x, float y, float size, uint32_t color) {
    bind_rgba_tex(TEX_GEAR, 32, 32, s_vram_gear, GU_TFX_MODULATE);
    draw_sprite(0.0f, 0.0f, 32.0f, 32.0f, x, y, x + size, y + size, color);
}

void ui_draw_wifi(float x, float y, float size, uint32_t color) {
    bind_rgba_tex(TEX_WIFI, 32, 32, s_vram_wifi, GU_TFX_MODULATE);
    draw_sprite(0.0f, 0.0f, 32.0f, 32.0f, x, y, x + size, y + size, color);
}

void ui_draw_mil(float x, float y, float size, uint32_t color) {
    bind_rgba_tex(TEX_MIL, 32, 32, s_vram_mil, GU_TFX_MODULATE);
    draw_sprite(0.0f, 0.0f, 32.0f, 32.0f, x, y, x + size, y + size, color);
}

void ui_rect_solid(float x, float y, float w, float h, uint32_t color) {
    use_untextured();
    VertCol *v = (VertCol *)sceGuGetMemory(2 * sizeof(VertCol));
    v[0].color = color; v[0].x = x;     v[0].y = y;     v[0].z = 0.0f;
    v[1].color = color; v[1].x = x + w; v[1].y = y + h; v[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, VTYPE_COL, 2, NULL, v);
}

void ui_rect_grad(float x, float y, float w, float h, uint32_t top_col, uint32_t bot_col) {
    use_untextured();
    VertCol *v = (VertCol *)sceGuGetMemory(6 * sizeof(VertCol));
    v[0] = (VertCol){top_col, x,     y,     0.0f};
    v[1] = (VertCol){top_col, x + w, y,     0.0f};
    v[2] = (VertCol){bot_col, x + w, y + h, 0.0f};
    v[3] = (VertCol){top_col, x,     y,     0.0f};
    v[4] = (VertCol){bot_col, x + w, y + h, 0.0f};
    v[5] = (VertCol){bot_col, x,     y + h, 0.0f};
    sceGuDrawArray(GU_TRIANGLES, VTYPE_COL, 6, NULL, v);
}

void ui_panel(float x, float y, float w, float h, uint32_t fill, uint32_t border) {
    uint32_t bot = scale_rgba(fill, 0.72f);
    ui_rect_grad(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, fill, bot);
    ui_rect_solid(x + 1.0f, y,         w - 2.0f, 1.0f, border);
    ui_rect_solid(x + 1.0f, y + h - 1.0f, w - 2.0f, 1.0f, border);
    ui_rect_solid(x,        y + 1.0f,  1.0f, h - 2.0f, border);
    ui_rect_solid(x + w - 1.0f, y + 1.0f, 1.0f, h - 2.0f, border);
    ui_rect_solid(x + 2.0f, y + 1.0f,  w - 4.0f, 1.0f, PSP_RGBA(255, 255, 255, 26));
}

void ui_circle(float cx, float cy, float r, uint32_t color, int segs) {
    if (segs < 6) segs = 12;
    use_untextured();
    VertCol *v = (VertCol *)sceGuGetMemory((segs + 2) * sizeof(VertCol));
    v[0] = (VertCol){color, cx, cy, 0.0f};
    float step = (float)(2.0 * M_PI) / (float)segs;
    for (int i = 0; i <= segs; i++) {
        float a = step * (float)i;
        v[i + 1] = (VertCol){color, cx + cosf(a) * r, cy + sinf(a) * r, 0.0f};
    }
    sceGuDrawArray(GU_TRIANGLE_FAN, VTYPE_COL, segs + 2, NULL, v);
}

void ui_line_thick(float x0, float y0, float x1, float y1, float thickness, uint32_t color) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float nx = (-dy / len) * (thickness * 0.5f);
    float ny = (dx / len) * (thickness * 0.5f);

    use_untextured();
    VertCol *v = (VertCol *)sceGuGetMemory(6 * sizeof(VertCol));
    v[0] = (VertCol){color, x0 + nx, y0 + ny, 0.0f};
    v[1] = (VertCol){color, x0 - nx, y0 - ny, 0.0f};
    v[2] = (VertCol){color, x1 - nx, y1 - ny, 0.0f};
    v[3] = (VertCol){color, x0 + nx, y0 + ny, 0.0f};
    v[4] = (VertCol){color, x1 - nx, y1 - ny, 0.0f};
    v[5] = (VertCol){color, x1 + nx, y1 + ny, 0.0f};
    sceGuDrawArray(GU_TRIANGLES, VTYPE_COL, 6, NULL, v);
}

void ui_hbar(float x, float y, float w, float h, float frac, uint32_t fill, uint32_t bg) {
    float f = clamp01(frac);
    ui_rect_solid(x, y, w, h, bg);
    if (f > 0.005f) {
        float fw = w * f;
        uint32_t hi = scale_rgba(fill, 1.18f);
        ui_rect_grad(x, y, fw, h, hi, fill);
        if (fw >= 2.0f) {
            ui_rect_solid(x + fw - 1.5f, y, 1.5f, h, PSP_RGBA(255, 255, 255, 170));
        }
    }
}

void ui_vbar(float x, float y, float w, float h, float frac, uint32_t fill, uint32_t bg) {
    float f = clamp01(frac);
    ui_rect_solid(x, y, w, h, bg);
    if (f > 0.005f) {
        float fh = h * f;
        uint32_t hi = scale_rgba(fill, 1.18f);
        ui_rect_grad(x, y + h - fh, w, fh, hi, fill);
        ui_rect_solid(x, y + h - fh, w, 1.5f, PSP_RGBA(255, 255, 255, 170));
    }
    ui_rect_solid(x - 1.0f, y - 1.0f, w + 2.0f, 1.0f, CLR_BORDER);
    ui_rect_solid(x - 1.0f, y + h,    w + 2.0f, 1.0f, CLR_BORDER);
    ui_rect_solid(x - 1.0f, y,        1.0f, h, CLR_BORDER);
    ui_rect_solid(x + w,    y,        1.0f, h, CLR_BORDER);
}

void ui_arc_band(float cx, float cy, float r0, float r1, float start_deg, float sweep_deg,
                 float f0, float f1, uint32_t color, int steps) {
    f0 = clamp01(f0);
    f1 = clamp01(f1);
    if (f1 <= f0 || steps < 1) return;

    use_untextured();
    VertCol *v = (VertCol *)sceGuGetMemory((steps + 1) * 2 * sizeof(VertCol));

    float a0 = (start_deg + f0 * sweep_deg) * (float)(M_PI / 180.0);
    float a1 = (start_deg + f1 * sweep_deg) * (float)(M_PI / 180.0);
    float da = (a1 - a0) / (float)steps;

    for (int i = 0; i <= steps; i++) {
        float a = a0 + da * (float)i;
        float ca = cosf(a);
        float sa = sinf(a);
        v[i * 2]     = (VertCol){color, cx + ca * r0, cy + sa * r0, 0.0f};
        v[i * 2 + 1] = (VertCol){color, cx + ca * r1, cy + sa * r1, 0.0f};
    }
    sceGuDrawArray(GU_TRIANGLE_STRIP, VTYPE_COL, (steps + 1) * 2, NULL, v);
}

void ui_radial_tick(float cx, float cy, float r0, float r1, float deg, float thickness, uint32_t color) {
    float a = deg * (float)(M_PI / 180.0);
    float ca = cosf(a);
    float sa = sinf(a);
    ui_line_thick(cx + ca * r0, cy + sa * r0, cx + ca * r1, cy + sa * r1, thickness, color);
}

static void draw_tapered_poly(float cx, float cy, float r_tail, float r_tip,
                              float ca, float sa, float base_w, float tip_w, uint32_t col) {
    float px = -sa;
    float py = ca;
    float bx = cx - ca * r_tail;
    float by = cy - sa * r_tail;
    float tx = cx + ca * r_tip;
    float ty = cy + sa * r_tip;

    use_untextured();
    VertCol *v = (VertCol *)sceGuGetMemory(6 * sizeof(VertCol));
    v[0] = (VertCol){col, bx + px * base_w, by + py * base_w, 0.0f};
    v[1] = (VertCol){col, bx - px * base_w, by - py * base_w, 0.0f};
    v[2] = (VertCol){col, tx - px * tip_w,  ty - py * tip_w,  0.0f};
    v[3] = (VertCol){col, bx + px * base_w, by + py * base_w, 0.0f};
    v[4] = (VertCol){col, tx - px * tip_w,  ty - py * tip_w,  0.0f};
    v[5] = (VertCol){col, tx + px * tip_w,  ty + py * tip_w,  0.0f};
    sceGuDrawArray(GU_TRIANGLES, VTYPE_COL, 6, NULL, v);
}

void ui_needle(float cx, float cy, float r_tail, float r_tip, float deg, float half_w, uint32_t color) {
    float a = deg * (float)(M_PI / 180.0);
    float ca = cosf(a);
    float sa = sinf(a);

    draw_tapered_poly(cx + 2.0f, cy + 2.5f, r_tail, r_tip, ca, sa, half_w + 0.6f, 1.0f, PSP_RGBA(0, 0, 0, 125));
    draw_tapered_poly(cx, cy, r_tail, r_tip, ca, sa, half_w, 0.8f, color);
    ui_line_thick(cx, cy, cx + ca * (r_tip - 2.0f), cy + sa * (r_tip - 2.0f), 1.1f, PSP_RGBA(255, 255, 255, 115));

    ui_circle(cx + 1.0f, cy + 1.5f, 11.0f, PSP_RGBA(0, 0, 0, 120), 20);
    ui_circle(cx, cy, 10.5f, color, 20);
    ui_circle(cx, cy, 8.5f, PSP_RGBA(18, 26, 38, 255), 20);
    ui_circle(cx, cy, 4.0f, PSP_RGBA(76, 98, 126, 255), 16);
}

void ui_psp_btn_badge(float x, float y, const char *glyph, const char *label, uint32_t btn_col) {
    float bw = 13.0f;
    float bh = 13.0f;
    if (strlen(glyph) > 1) bw = (float)strlen(glyph) * 7.0f + 6.0f;
    ui_panel(x, y, bw, bh, CLR_PANEL_DARK, btn_col);
    ui_text(x + bw * 0.5f, y + 1.0f, 0.48f, btn_col, UI_ALIGN_CENTER, glyph);
    if (label && label[0]) {
        ui_text(x + bw + 3.0f, y + 1.0f, 0.48f, CLR_GRAY, UI_ALIGN_LEFT, label);
    }
}

static uint16_t next_utf8(const char **p) {
    const unsigned char *s = (const unsigned char *)(*p);
    if (*s == 0) return 0;
    if (*s < 0x80) {
        (*p)++;
        return *s;
    }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        uint16_t cp = (uint16_t)(((s[0] & 0x1F) << 6) | (s[1] & 0x3F));
        (*p) += 2;
        return cp;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        uint16_t cp = (uint16_t)(((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F));
        (*p) += 3;
        return cp;
    }
    (*p)++;
    return '?';
}

static const PspGlyph *find_glyph(const PspGlyph *table, int count, uint16_t code) {
    for (int i = 0; i < count; i++) {
        if (table[i].code == code) return &table[i];
    }
    return &table[0];
}

static float measure_str(const PspGlyph *table, int count, float scale, const char *str, bool tabular_num) {
    if (!str) return 0.0f;
    const char *p = str;
    float w = 0.0f;
    while (*p) {
        uint16_t cp = next_utf8(&p);
        const PspGlyph *g = find_glyph(table, count, cp);
        float adv = (tabular_num && cp >= '0' && cp <= '9') ? s_num_digit_adv : g->xadvance;
        w += adv * scale;
    }
    return w;
}

static void draw_glyph_str(int tex_id, const void *tex_ptr, const PspGlyph *table, int count,
                           float x, float y, float scale, uint32_t color, int align,
                           const char *str, bool tabular_num) {
    if (!str || !str[0]) return;
    if (align == UI_ALIGN_CENTER) {
        x -= measure_str(table, count, scale, str, tabular_num) * 0.5f;
    } else if (align == UI_ALIGN_RIGHT) {
        x -= measure_str(table, count, scale, str, tabular_num);
    }

    int max_glyphs = 0;
    for (const char *q = str; *q; ) {
        next_utf8(&q);
        max_glyphs++;
    }
    if (max_glyphs == 0) return;

    bind_font_tex(tex_id, tex_ptr);
    VertTex *verts = (VertTex *)sceGuGetMemory(max_glyphs * 2 * sizeof(VertTex));
    int n = 0;
    float pen_x = x;
    const char *p = str;

    while (*p) {
        uint16_t cp = next_utf8(&p);
        const PspGlyph *g = find_glyph(table, count, cp);
        float adv = (tabular_num && cp >= '0' && cp <= '9') ? s_num_digit_adv : g->xadvance;

        if (cp != ' ') {
            float gx = pen_x + (tabular_num && cp >= '0' && cp <= '9'
                                ? (adv - g->xadvance) * 0.5f * scale : 0.0f) + (float)g->xoff * scale;
            float gy = y + (float)g->yoff * scale;
            float gw = (float)g->w * scale;
            float gh = (float)g->h * scale;

            verts[n * 2].u = (float)g->u0;
            verts[n * 2].v = (float)g->v0;
            verts[n * 2].color = color;
            verts[n * 2].x = gx;
            verts[n * 2].y = gy;
            verts[n * 2].z = 0.0f;

            verts[n * 2 + 1].u = (float)g->u1;
            verts[n * 2 + 1].v = (float)g->v1;
            verts[n * 2 + 1].color = color;
            verts[n * 2 + 1].x = gx + gw;
            verts[n * 2 + 1].y = gy + gh;
            verts[n * 2 + 1].z = 0.0f;
            n++;
        }
        pen_x += adv * scale;
    }
    if (n > 0) {
        sceGuDrawArray(GU_SPRITES, VTYPE_TEX, n * 2, NULL, verts);
    }
}

void ui_text(float x, float y, float scale, uint32_t color, int align, const char *str) {
    draw_glyph_str(TEX_FONT_UI, s_vram_font_ui, psp_ui_glyphs, PSP_UI_GLYPH_COUNT,
                   x, y, scale, color, align, str, false);
}

void ui_textf(float x, float y, float scale, uint32_t color, int align, const char *fmt, ...) {
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ui_text(x, y, scale, color, align, buf);
}

float ui_text_width(float scale, const char *str) {
    return measure_str(psp_ui_glyphs, PSP_UI_GLYPH_COUNT, scale, str, false);
}

void ui_text_clipped(float x, float y, float max_w, float scale, uint32_t color, const char *str) {
    if (!str || !str[0]) return;
    if (ui_text_width(scale, str) <= max_w) {
        ui_text(x, y, scale, color, UI_ALIGN_LEFT, str);
        return;
    }
    float ell_w = ui_text_width(scale, "...");
    char buf[160];
    size_t len = strlen(str);
    size_t out_len = 0;
    float w = 0.0f;
    const char *p = str;
    while (*p && out_len + 4 < sizeof(buf)) {
        const char *prev = p;
        uint16_t cp = next_utf8(&p);
        const PspGlyph *g = find_glyph(psp_ui_glyphs, PSP_UI_GLYPH_COUNT, cp);
        float cw = g->xadvance * scale;
        if (w + cw + ell_w > max_w) break;
        size_t clen = (size_t)(p - prev);
        if (out_len + clen + 4 >= sizeof(buf)) break;
        memcpy(buf + out_len, prev, clen);
        out_len += clen;
        w += cw;
    }
    if (out_len <= len) {
        memcpy(buf + out_len, "...", 4);
    }
    ui_text(x, y, scale, color, UI_ALIGN_LEFT, buf);
}

void ui_num(float x, float y, float scale, uint32_t color, int align, const char *str) {
    draw_glyph_str(TEX_FONT_NUM, s_vram_font_num, psp_num_glyphs, PSP_NUM_GLYPH_COUNT,
                   x, y, scale, color, align, str, true);
}

void ui_numf(float x, float y, float scale, uint32_t color, int align, const char *fmt, ...) {
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ui_num(x, y, scale, color, align, buf);
}
