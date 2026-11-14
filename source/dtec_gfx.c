#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "dtec_gfx.h"

static uint16_t blend(uint16_t bg, uint16_t fg, unsigned alpha) {
    unsigned inv = 15 - alpha;
    return DTEC_RGB((((bg & 31) * inv) + ((fg & 31) * alpha)) / 15,
        ((((bg >> 5) & 31) * inv) + (((fg >> 5) & 31) * alpha)) / 15,
        ((((bg >> 10) & 31) * inv) + (((fg >> 10) & 31) * alpha)) / 15);
}

void gfx_background(uint16_t *buf) {
    if (buf) memcpy(buf, dtec_art_deck, SCREEN_W * SCREEN_H * sizeof(uint16_t));
}

void gfx_dial(uint16_t *buf, int cx, int cy, int radius) {
    if (!buf || radius <= 0) return;
    for (int y = 0; y < radius * 2; y++) {
        for (int x = 0; x < radius * 2; x++) {
            uint16_t c = dtec_art_dial[(y * 176 / (radius * 2)) * 176 + x * 176 / (radius * 2)];
            if (c & BIT(15)) gfx_pixel(buf, cx - radius + x, cy - radius + y, c);
        }
    }
}

void gfx_panel(uint16_t *buf, int x, int y, int w, int h, uint16_t color, uint16_t border) {
    if (!buf || w < 10 || h < 10) return;
    static const int corners[] = {4, 2, 1, 1};
    for (int row = 0; row < h; row++) {
        int edge = row < h / 2 ? row : h - 1 - row;
        int inset = edge < 4 ? corners[edge] : 0;
        uint16_t shade = blend(color, DTEC_COLOR_BLACK, (unsigned)(row * 4 / h));
        gfx_rect_fill(buf, x + inset, y + row, w - inset * 2, 1, shade);
        if (row == 0 || row == h - 1)
            gfx_rect_fill(buf, x + inset, y + row, w - inset * 2, 1, border);
        else {
            gfx_pixel(buf, x + inset, y + row, border);
            gfx_pixel(buf, x + w - 1 - inset, y + row, border);
        }
    }
    gfx_line(buf, x + 5, y + 1, x + w - 6, y + 1, blend(color, DTEC_COLOR_WHITE, 2));
}

void gfx_gear(uint16_t *buf, int x, int y, uint16_t color) {
    gfx_circle(buf, x + 7, y + 7, 5, color);
    gfx_circle(buf, x + 7, y + 7, 2, color);
    gfx_rect_fill(buf, x + 6, y, 3, 3, color);
    gfx_rect_fill(buf, x + 6, y + 12, 3, 3, color);
    gfx_rect_fill(buf, x, y + 6, 3, 3, color);
    gfx_rect_fill(buf, x + 12, y + 6, 3, 3, color);
}

void gfx_clear(uint16_t *buf, uint16_t color) {
    if (!buf) return;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        buf[i] = color;
    }
}

void gfx_pixel(uint16_t *buf, int x, int y, uint16_t color) {
    if ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H) {
        buf[y * SCREEN_W + x] = color;
    }
}

void gfx_rect(uint16_t *buf, int x, int y, int w, int h, uint16_t color) {
    if (!buf || w <= 0 || h <= 0) return;
    int x2 = x + w - 1;
    int y2 = y + h - 1;

    for (int i = x; i <= x2; i++) {
        gfx_pixel(buf, i, y, color);
        gfx_pixel(buf, i, y2, color);
    }
    for (int j = y; j <= y2; j++) {
        gfx_pixel(buf, x, j, color);
        gfx_pixel(buf, x2, j, color);
    }
}

void gfx_rect_fill(uint16_t *buf, int x, int y, int w, int h, uint16_t color) {
    if (!buf || w <= 0 || h <= 0) return;
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = (x + w) > SCREEN_W ? SCREEN_W : (x + w);
    int y2 = (y + h) > SCREEN_H ? SCREEN_H : (y + h);

    for (int j = y1; j < y2; j++) {
        uint16_t *row = &buf[j * SCREEN_W + x1];
        int count = x2 - x1;
        for (int i = 0; i < count; i++) {
            row[i] = color;
        }
    }
}

void gfx_line(uint16_t *buf, int x0, int y0, int x1, int y1, uint16_t color) {
    if (!buf) return;
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        gfx_pixel(buf, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_circle(uint16_t *buf, int cx, int cy, int radius, uint16_t color) {
    if (!buf || radius <= 0) return;
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;

    while (y >= x) {
        gfx_pixel(buf, cx + x, cy + y, color);
        gfx_pixel(buf, cx - x, cy + y, color);
        gfx_pixel(buf, cx + x, cy - y, color);
        gfx_pixel(buf, cx - x, cy - y, color);
        gfx_pixel(buf, cx + y, cy + x, color);
        gfx_pixel(buf, cx - y, cy + x, color);
        gfx_pixel(buf, cx + y, cy - x, color);
        gfx_pixel(buf, cx - y, cy - x, color);
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void gfx_circle_fill(uint16_t *buf, int cx, int cy, int radius, uint16_t color) {
    if (!buf || radius <= 0) return;
    for (int dy = -radius; dy <= radius; dy++) {
        int dx_limit = (int)sqrtf((float)(radius * radius - dy * dy));
        int py = cy + dy;
        if ((unsigned)py < SCREEN_H) {
            int x1 = cx - dx_limit;
            int x2 = cx + dx_limit;
            if (x1 < 0) x1 = 0;
            if (x2 >= SCREEN_W) x2 = SCREEN_W - 1;
            for (int px = x1; px <= x2; px++) {
                buf[py * SCREEN_W + px] = color;
            }
        }
    }
}

void gfx_draw_char(uint16_t *buf, int x, int y, char c, uint16_t color, int scale) {
    if (!buf || c < 32 || c > 126 || scale < 1 || scale > 3) return;
    int size = 8 * scale;
    const uint8_t *font = scale == 1 ? dtec_font_1 : (scale == 2 ? dtec_font_2 : dtec_font_3);
    const uint8_t *glyph = font + (c - 32) * size * size;
    for (int row = 0; row < size; row++) {
        if ((unsigned)(y + row) >= SCREEN_H) continue;
        for (int col = 0; col < size; col++) {
            if ((unsigned)(x + col) >= SCREEN_W) continue;
            unsigned alpha = glyph[row * size + col];
            if (!alpha) continue;
            uint16_t *pixel = &buf[(y + row) * SCREEN_W + x + col];
            *pixel = alpha == 15 ? color : blend(*pixel, color, alpha);
        }
    }
}

void gfx_draw_str(uint16_t *buf, int x, int y, const char *str, uint16_t color, int scale) {
    if (!buf || !str) return;
    int cur_x = x;
    int char_w = 8 * scale;
    while (*str) {
        if (*str == '\n') {
            y += 9 * scale;
            cur_x = x;
        } else {
            if (cur_x + char_w <= SCREEN_W) {
                gfx_draw_char(buf, cur_x, y, *str, color, scale);
            }
            cur_x += char_w;
        }
        str++;
    }
}

void gfx_draw_str_clipped(uint16_t *buf, int x, int y, int max_w, const char *str, uint16_t color, int scale) {
    if (!buf || !str) return;
    int cur_x = x;
    int char_w = 8 * scale;
    int right_limit = (max_w > 0) ? (x + max_w) : SCREEN_W;
    if (right_limit > SCREEN_W) right_limit = SCREEN_W;

    while (*str && cur_x + char_w <= right_limit) {
        if (*str == '\n') break;
        gfx_draw_char(buf, cur_x, y, *str, color, scale);
        cur_x += char_w;
        str++;
    }
}

void gfx_draw_str_wrap(uint16_t *buf, int x, int y, int max_w, const char *str, uint16_t color, int scale) {
    if (!buf || !str || max_w <= 0) return;
    int cur_x = x;
    int cur_y = y;
    int char_w = 8 * scale;
    int line_h = 9 * scale;
    int right_limit = x + max_w;
    if (right_limit > SCREEN_W) right_limit = SCREEN_W;

    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            cur_y += line_h;
            str++;
            continue;
        }

        if (*str == ' ') {
            cur_x += char_w;
            str++;
            continue;
        }

        const char *word_end = str;
        while (*word_end && *word_end != ' ' && *word_end != '\n') {
            word_end++;
        }
        int word_chars = (int)(word_end - str);
        int word_pixels = word_chars * char_w;

        if (cur_x > x && cur_x + word_pixels > right_limit) {
            cur_x = x;
            cur_y += line_h;
        }

        if (cur_y + 8 * scale > SCREEN_H) break;

        for (int i = 0; i < word_chars; i++) {
            if (cur_x + char_w <= right_limit) {
                gfx_draw_char(buf, cur_x, cur_y, str[i], color, scale);
            }
            cur_x += char_w;
        }
        str = word_end;
    }
}

void gfx_draw_int(uint16_t *buf, int x, int y, int val, uint16_t color, int scale) {
    char str[16];
    snprintf(str, sizeof(str), "%d", val);
    gfx_draw_str(buf, x, y, str, color, scale);
}

void gfx_draw_float(uint16_t *buf, int x, int y, float val, int dec, uint16_t color, int scale) {
    char str[16];
    if (dec == 1) {
        snprintf(str, sizeof(str), "%.1f", val);
    } else if (dec == 2) {
        snprintf(str, sizeof(str), "%.2f", val);
    } else {
        snprintf(str, sizeof(str), "%.0f", val);
    }
    gfx_draw_str(buf, x, y, str, color, scale);
}

// Antialiased numeral sprites replace enlarged rectangular seven-segment blocks.
// The legacy layout arguments keep all existing gauge modes aligned.
void gfx_seg_number(uint16_t *buf, int x, int y, int w, int h, int t, int digits,
                    int val, uint16_t on, uint16_t off, int skew) {
    (void)off;
    if (!buf || digits <= 0 || w <= 0 || h <= 0) return;
    int gap = t + w / 4;
    for (int i = digits - 1; i >= 0; i--) {
        int digit = val < 0 ? 10 : val % 10;
        bool visible = val != 0 || i == digits - 1;
        if (val >= 0) val /= 10;
        if (!visible) continue;
        const uint8_t *mask = dtec_digits + digit * 64 * 112;
        for (int row = 0; row < h; row++) {
            int py = y + row;
            if ((unsigned)py >= SCREEN_H) continue;
            for (int col = 0; col < w; col++) {
                int px = x + i * (w + gap) + col + (h - row) * skew / 16;
                if ((unsigned)px >= SCREEN_W) continue;
                unsigned alpha = mask[(row * 112 / h) * 64 + col * 64 / w];
                if (!alpha) continue;
                uint16_t *pixel = &buf[py * SCREEN_W + px];
                *pixel = alpha == 15 ? on : blend(*pixel, on, alpha);
            }
        }
    }
}
