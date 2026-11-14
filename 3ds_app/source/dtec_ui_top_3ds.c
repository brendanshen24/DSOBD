#include <math.h>
#include <stdio.h>
#include <string.h>

#include "dtec_common.h"
#include "dtec_obd.h"
#include "gfx3ds.h"
#include "platform_3ds.h"

#define HUD_REDLINE 7000.0f
#define HUD_RPM_MAX 8500.0f

static float s_rpm_ease;
static uint32_t s_hud_frame;

static float clamp01f(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static u32 zone_color(float f) {
    return f < 0.60f ? CLR_CYAN : (f < 0.82f ? CLR_YELLOW : CLR_RED);
}

static void draw_header(const char *mode_name) {
    C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, TOP_W, 22.0f, CLR_PANEL);
    C2D_DrawRectSolid(0.0f, 22.0f, 0.0f, TOP_W, 1.0f, CLR_BORDER);

    ui_text(10.0f, 3.0f, 0.58f, CLR_CYAN, 0, "3DSOBD");
    if (g_dtec.logging) {
        C2D_DrawCircleSolid(86.0f, 11.0f, 0.0f, 3.5f, CLR_RED);
        ui_text(94.0f, 4.0f, 0.52f, CLR_RED, 0, "REC");
    }

    if (g_dtec.profile_confirmed && g_dtec.car_brand[0]) {
        ui_textf(200.0f, 4.0f, 0.52f, CLR_GRAY, C2D_AlignCenter, "%s  •  %s",
                 g_dtec.car_brand, mode_name);
    } else {
        ui_text(200.0f, 4.0f, 0.54f, CLR_GRAY, C2D_AlignCenter, mode_name);
    }

    if (g_dtec.net_status == NET_OBD_CONNECTED) {
        ui_panel(306.0f, 3.0f, 86.0f, 16.0f, CLR_GREEN_BG, CLR_GREEN);
        ui_text(349.0f, 4.0f, 0.48f, CLR_GREEN, C2D_AlignCenter, "LIVE");
    } else if (g_dtec.net_status == NET_TCP_CONNECTING || g_dtec.net_status == NET_WIFI_CONNECTING) {
        ui_panel(306.0f, 3.0f, 86.0f, 16.0f, CLR_YELLOW_BG, CLR_YELLOW);
        ui_text(349.0f, 4.0f, 0.48f, CLR_YELLOW, C2D_AlignCenter, "LINKING");
    } else {
        ui_panel(306.0f, 3.0f, 86.0f, 16.0f, CLR_RED_BG, CLR_RED);
        ui_text(349.0f, 4.0f, 0.48f, CLR_WHITE, C2D_AlignCenter, "NO LINK");
    }
}

static void draw_metric_card(float x, float y, float w, float h,
                             const char *label, const char *val, u32 val_col,
                             bool has_bar, float frac, u32 bar_col) {
    ui_panel(x, y, w, h, CLR_PANEL, CLR_BORDER);
    ui_text(x + 8.0f, y + 4.0f, 0.44f, CLR_GRAY, 0, label);
    ui_text_clipped(x + 8.0f, y + 17.0f, w - 16.0f, 0.66f, val_col, val);
    if (has_bar) {
        ui_hbar(x + 8.0f, y + h - 8.0f, w - 16.0f, 4.0f, frac, bar_col, CLR_DARK_GRAY);
    }
}

/* Reusable 4-card telemetry dock along the right side of 400x240 widescreen layouts. */
static void draw_side_dock(float x, float y0, float w, float card_h, float gap) {
    char buf[24];
    float y = y0;

    if (g_dtec.has_coolant) snprintf(buf, sizeof(buf), "%d °C", g_dtec.coolant_c);
    else snprintf(buf, sizeof(buf), "-- °C");
    u32 cool_col = (g_dtec.has_coolant && g_dtec.coolant_c > 104) ? CLR_RED : CLR_WHITE;
    float cool_f = g_dtec.has_coolant ? clamp01f((g_dtec.coolant_c - 40.0f) / 80.0f) : 0.0f;
    draw_metric_card(x, y, w, card_h, "COOLANT", buf, cool_col,
                     true, cool_f, cool_col == CLR_RED ? CLR_RED : CLR_ORANGE);
    y += card_h + gap;

    if (g_dtec.has_tps) snprintf(buf, sizeof(buf), "%d%%", (int)g_dtec.tps_pct);
    else snprintf(buf, sizeof(buf), "--%%");
    draw_metric_card(x, y, w, card_h, "THROTTLE", buf, CLR_WHITE,
                     true, g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f, CLR_GREEN);
    y += card_h + gap;

    if (g_dtec.has_boost) {
        snprintf(buf, sizeof(buf), "%+.0f kPa", g_dtec.boost_kpa);
        draw_metric_card(x, y, w, card_h, "BOOST", buf, CLR_CYAN,
                         true, g_dtec.boost_kpa / 200.0f, CLR_CYAN);
    } else {
        if (g_dtec.has_load) snprintf(buf, sizeof(buf), "%d%%", (int)g_dtec.engine_load_pct);
        else snprintf(buf, sizeof(buf), "--%%");
        draw_metric_card(x, y, w, card_h, "ENG LOAD", buf, CLR_WHITE,
                         true, g_dtec.has_load ? g_dtec.engine_load_pct / 100.0f : 0.0f, CLR_PURPLE);
    }
    y += card_h + gap;

    snprintf(buf, sizeof(buf), "%s", g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V");
    float vnum = 0.0f;
    if (g_dtec.has_voltage) sscanf(g_dtec.battery_voltage, "%f", &vnum);
    draw_metric_card(x, y, w, card_h, "BATTERY", buf,
                     g_dtec.has_voltage ? CLR_WHITE : CLR_GRAY,
                     true, g_dtec.has_voltage ? clamp01f((vnum - 11.0f) / 4.0f) : 0.0f, CLR_YELLOW);
}

static void draw_shift_lights_wide(float y, float h) {
    const int n = 20;
    float margin = 8.0f;
    float total_w = TOP_W - margin * 2.0f;
    float step = total_w / (float)n;
    float w = step - 3.0f;
    int lit = (int)(clamp01f(s_rpm_ease / HUD_REDLINE) * n + 0.5f);
    bool flash = s_rpm_ease >= HUD_REDLINE;

    for (int i = 0; i < n; i++) {
        u32 col = i < 8 ? CLR_GREEN : (i < 14 ? CLR_YELLOW : CLR_RED);
        if (flash) col = (s_hud_frame & 4) ? CLR_CYAN : CLR_RED;
        else if (i >= lit) col = CLR_DARK_GRAY;
        if (flash && (s_hud_frame & 2)) col = CLR_DARK_GRAY;

        float bx = margin + i * step;
        if (i < lit || flash) {
            u32 glow = (col & 0x00FFFFFFu) | 0x44000000u;
            C2D_DrawRectSolid(bx - 1.0f, y - 1.0f, 0.0f, w + 2.0f, h + 2.0f, glow);
        }
        C2D_DrawRectSolid(bx, y, 0.0f, w, h, col);
        if (i < lit && !flash) {
            C2D_DrawRectSolid(bx, y, 0.0f, w, 2.0f, C2D_Color32(255, 255, 255, 130));
        }
    }
}

static void draw_link_dot(float x, float y) {
    u32 col = CLR_RED;
    if (g_dtec.net_status == NET_OBD_CONNECTED) col = CLR_GREEN;
    else if (g_dtec.net_status == NET_TCP_CONNECTING || g_dtec.net_status == NET_WIFI_CONNECTING) col = CLR_YELLOW;
    C2D_DrawCircleSolid(x, y, 0.0f, 3.5f, col);
}

static void draw_dial_labels(float cx, float cy, float r, float start_deg, float sweep_deg, int every,
                             float max_val, float red_val, float scale) {
    for (int k = 0; (float)k <= max_val + 0.01f; k += every) {
        float frac = (float)k / max_val;
        float a = (start_deg + frac * sweep_deg) * (float)(M_PI / 180.0);
        float lx = cx + cosf(a) * r;
        float ly = cy + sinf(a) * r;
        u32 col = (float)k >= red_val ? CLR_RED : CLR_WHITE;
        ui_textf(lx, ly - 8.0f * scale, scale, col, C2D_AlignCenter, "%d", k);
    }
}

static void render_multi_gauge(void) {
    draw_header("MULTI");

    /* Left hero speed + RPM bar */
    ui_panel(8.0f, 28.0f, 250.0f, 132.0f, CLR_PANEL_DARK, CLR_BORDER);
    ui_text(133.0f, 34.0f, 0.52f, CLR_CYAN, C2D_AlignCenter, "VEHICLE SPEED");

    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)g_dtec.speed_kmh);
    else snprintf(spd, sizeof(spd), "---");
    ui_num(135.0f, 48.0f, 0.96f, C2D_Color32(0, 0, 0, 120), C2D_AlignCenter, spd);
    ui_num(133.0f, 46.0f, 0.96f, s_rpm_ease >= HUD_REDLINE ? CLR_RED : CLR_WHITE, C2D_AlignCenter, spd);

    if (g_dtec.has_speed) {
        unsigned mph = (g_dtec.speed_kmh * 621u) / 1000u;
        ui_textf(133.0f, 137.0f, 0.54f, CLR_GRAY, C2D_AlignCenter, "%u km/h   •   %u mph",
                 (unsigned)g_dtec.speed_kmh, mph);
    } else {
        ui_text(133.0f, 137.0f, 0.54f, CLR_GRAY, C2D_AlignCenter, "--- km/h   •   -- mph");
    }

    /* Wide segmented RPM panel */
    ui_panel(8.0f, 166.0f, 250.0f, 66.0f, CLR_PANEL, CLR_BORDER);
    ui_text(16.0f, 171.0f, 0.50f, CLR_GRAY, 0, "ENGINE RPM");
    if (g_dtec.has_rpm) {
        ui_textf(248.0f, 170.0f, 0.58f, s_rpm_ease >= HUD_REDLINE ? CLR_RED : CLR_WHITE,
                 C2D_AlignRight, "%d RPM", (int)g_dtec.rpm);
    } else {
        ui_text(248.0f, 170.0f, 0.58f, CLR_GRAY, C2D_AlignRight, "---- RPM");
    }

    const int segs = 38;
    int lit = g_dtec.has_rpm ? (int)(clamp01f(s_rpm_ease / HUD_RPM_MAX) * segs) : 0;
    for (int i = 0; i < segs; i++) {
        float f = (float)i / (float)segs;
        u32 col = zone_color(f);
        float sx = 16.0f + i * 6.1f;
        C2D_DrawRectSolid(sx, 192.0f, 0.0f, 4.5f, 18.0f, i < lit ? col : CLR_DARK_GRAY);
        if (i < lit) C2D_DrawRectSolid(sx, 192.0f, 0.0f, 4.5f, 2.0f, CLR_WHITE);
    }
    ui_textf(16.0f, 214.0f, 0.42f, CLR_GRAY, 0, "PEAK %d RPM", (int)g_dtec.peak_rpm);
    ui_text(248.0f, 214.0f, 0.42f, CLR_GRAY, C2D_AlignRight, "SELECT VIEW  •  START POLL");

    /* Right 4-card widescreen dock */
    draw_side_dock(266.0f, 28.0f, 126.0f, 47.0f, 5.0f);
}

static void render_digital_speed(void) {
    draw_shift_lights_wide(5.0f, 11.0f);

    /* Left speed + RPM sweep */
    ui_panel(8.0f, 24.0f, 250.0f, 208.0f, CLR_PANEL_DARK, CLR_BORDER);
    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)g_dtec.speed_kmh);
    else snprintf(spd, sizeof(spd), "---");
    u32 spd_col = s_rpm_ease >= HUD_REDLINE ? CLR_RED : CLR_WHITE;
    ui_num(20.0f, 34.0f, 1.12f, spd_col, 0, spd);
    ui_text(192.0f, 92.0f, 0.68f, CLR_CYAN, 0, "km/h");
    if (g_dtec.has_speed) {
        ui_textf(192.0f, 112.0f, 0.48f, CLR_GRAY, 0, "%u mph",
                 (unsigned)((g_dtec.speed_kmh * 621u) / 1000u));
    }

    const int segs = 40;
    int lit = (int)(clamp01f(s_rpm_ease / HUD_RPM_MAX) * segs);
    for (int i = 0; i < segs; i++) {
        u32 col = zone_color((float)i / (float)segs);
        C2D_DrawRectSolid(16.0f + i * 5.85f, 146.0f, 0.0f, 4.3f, 12.0f, i < lit ? col : CLR_DARK_GRAY);
    }

    ui_text(16.0f, 168.0f, 0.46f, CLR_GRAY, 0, "ENGINE RPM");
    if (g_dtec.has_rpm) ui_numf(16.0f, 180.0f, 0.50f, CLR_CYAN, 0, "%d", (int)g_dtec.rpm);
    else ui_num(16.0f, 180.0f, 0.50f, CLR_DARK_GRAY, 0, "----");

    ui_text(248.0f, 168.0f, 0.46f, CLR_GRAY, C2D_AlignRight, "PEAK RPM");
    ui_textf(248.0f, 190.0f, 0.66f, CLR_YELLOW, C2D_AlignRight, "%d", (int)g_dtec.peak_rpm);

    /* Right 4-card dock */
    draw_side_dock(266.0f, 24.0f, 126.0f, 49.0f, 4.0f);
    draw_link_dot(393.0f, 233.0f);
}

static void render_big_speed(void) {
    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)g_dtec.speed_kmh);
    else snprintf(spd, sizeof(spd), "---");

    ui_num(200.0f, 22.0f, 1.70f, CLR_WHITE, C2D_AlignCenter, spd);
    if (g_dtec.has_speed) {
        ui_textf(200.0f, 178.0f, 0.72f, CLR_GRAY, C2D_AlignCenter, "KM/H   •   %u MPH",
                 (unsigned)((g_dtec.speed_kmh * 621u) / 1000u));
    } else {
        ui_text(200.0f, 178.0f, 0.72f, CLR_GRAY, C2D_AlignCenter, "KM/H");
    }

    float f = clamp01f(s_rpm_ease / HUD_RPM_MAX);
    u32 col = zone_color(f);
    if (s_rpm_ease >= HUD_REDLINE && (s_hud_frame & 4)) col = CLR_WHITE;
    C2D_DrawRectSolid(0.0f, 222.0f, 0.0f, TOP_W, 18.0f, CLR_DARK_GRAY);
    C2D_DrawRectSolid(0.0f, 222.0f, 0.0f, TOP_W * f, 18.0f, col);
    draw_link_dot(392.0f, 12.0f);
}

static void render_arc_tach(void) {
    const float cx = 132.0f, cy = 124.0f, r = 104.0f;
    ui_draw_dial_large(cx, cy, r);

    const int wedges = 32;
    float frac = clamp01f(s_rpm_ease / HUD_RPM_MAX);
    for (int i = 0; i < wedges; i++) {
        float f0 = (float)i / (float)wedges;
        float f1 = ((float)i + 0.78f) / (float)wedges;
        u32 col = (f0 < frac) ? zone_color(f0) : CLR_DARK_GRAY;
        ui_arc_band(cx, cy, 74.0f, 92.0f, 150.0f, 240.0f, f0, f1, col, 2);
    }

    draw_dial_labels(cx, cy, 62.0f, 150.0f, 240.0f, 1, HUD_RPM_MAX / 1000.0f, HUD_REDLINE / 1000.0f, 0.54f);

    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)g_dtec.speed_kmh);
    else snprintf(spd, sizeof(spd), "---");
    ui_num(cx, cy - 36.0f, 0.66f, CLR_WHITE, C2D_AlignCenter, spd);
    ui_text(cx, cy + 26.0f, 0.52f, CLR_CYAN, C2D_AlignCenter, "km/h");
    if (g_dtec.has_rpm) {
        ui_textf(cx, cy + 44.0f, 0.56f, s_rpm_ease >= HUD_REDLINE ? CLR_RED : CLR_YELLOW,
                 C2D_AlignCenter, "%d RPM", (int)g_dtec.rpm);
    } else {
        ui_text(cx, cy + 44.0f, 0.56f, CLR_YELLOW, C2D_AlignCenter, "--- RPM");
    }

    draw_side_dock(266.0f, 16.0f, 126.0f, 50.0f, 4.0f);
    draw_link_dot(393.0f, 233.0f);
}

static void render_sim_dash(void) {
    draw_shift_lights_wide(4.0f, 9.0f);

    bool redline = s_rpm_ease >= HUD_REDLINE;
    u32 box_border = (redline && (s_hud_frame & 4)) ? CLR_RED : CLR_ORANGE;

    ui_vbar(12.0f, 22.0f, 26.0f, 124.0f, g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f, CLR_GREEN, CLR_DARK_GRAY);
    ui_text(25.0f, 150.0f, 0.48f, CLR_GRAY, C2D_AlignCenter, "THR");

    ui_panel(50.0f, 22.0f, 300.0f, 140.0f, CLR_LCD_BG, box_border);
    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)g_dtec.speed_kmh);
    else snprintf(spd, sizeof(spd), "---");
    ui_num(200.0f, 32.0f, 1.06f, CLR_LCD_ON, C2D_AlignCenter, spd);
    if (g_dtec.has_speed) {
        ui_textf(200.0f, 134.0f, 0.56f, CLR_LCD_ON, C2D_AlignCenter, "KM/H   •   %u MPH",
                 (unsigned)((g_dtec.speed_kmh * 621u) / 1000u));
    } else {
        ui_text(200.0f, 134.0f, 0.56f, CLR_LCD_ON, C2D_AlignCenter, "KM/H");
    }

    if (g_dtec.has_boost) {
        ui_vbar(362.0f, 22.0f, 26.0f, 124.0f, g_dtec.boost_kpa / 200.0f, CLR_CYAN, CLR_DARK_GRAY);
        ui_text(375.0f, 150.0f, 0.48f, CLR_GRAY, C2D_AlignCenter, "BST");
    } else {
        ui_vbar(362.0f, 22.0f, 26.0f, 124.0f, g_dtec.has_load ? g_dtec.engine_load_pct / 100.0f : 0.0f,
                CLR_PURPLE, CLR_DARK_GRAY);
        ui_text(375.0f, 150.0f, 0.48f, CLR_GRAY, C2D_AlignCenter, "LOAD");
    }

    /* 4 LCD cards across bottom 400px */
    const float cw = 90.0f;
    for (int i = 0; i < 4; i++) {
        ui_panel(10.0f + i * 96.0f, 170.0f, cw, 62.0f, CLR_LCD_BG, CLR_BORDER);
    }
    ui_text(18.0f, 175.0f, 0.44f, CLR_GRAY, 0, "RPM");
    if (g_dtec.has_rpm) ui_numf(55.0f, 188.0f, 0.42f, redline ? CLR_RED : CLR_LCD_ON, C2D_AlignCenter, "%d", (int)g_dtec.rpm);
    else ui_num(55.0f, 188.0f, 0.42f, CLR_LCD_GHOST, C2D_AlignCenter, "----");

    ui_text(114.0f, 175.0f, 0.44f, CLR_GRAY, 0, "WATER");
    if (g_dtec.has_coolant) {
        ui_textf(151.0f, 196.0f, 0.80f, g_dtec.coolant_c > 104 ? CLR_RED : CLR_LCD_ON,
                 C2D_AlignCenter, "%d°C", g_dtec.coolant_c);
    } else {
        ui_text(151.0f, 196.0f, 0.80f, CLR_LCD_GHOST, C2D_AlignCenter, "--°C");
    }

    ui_text(210.0f, 175.0f, 0.44f, CLR_GRAY, 0, "BATT");
    ui_text(247.0f, 198.0f, 0.70f, g_dtec.has_voltage ? CLR_LCD_ON : CLR_GRAY,
            C2D_AlignCenter, g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V");

    ui_text(306.0f, 175.0f, 0.44f, CLR_GRAY, 0, "PEAK RPM");
    ui_textf(343.0f, 198.0f, 0.72f, CLR_LCD_ON, C2D_AlignCenter, "%d", (int)g_dtec.peak_rpm);
    draw_link_dot(394.0f, 234.0f);
}

static void render_gt_style(void) {
    float frac = clamp01f(s_rpm_ease / HUD_RPM_MAX);
    bool redline = s_rpm_ease >= HUD_REDLINE;

    /* Wide center-out rev strip */
    float half = clamp01f(s_rpm_ease / HUD_REDLINE) * 180.0f;
    u32 strip = s_rpm_ease > HUD_REDLINE * 0.85f ? CLR_RED : CLR_GT_BLUE;
    if (redline && (s_hud_frame & 4)) strip = CLR_WHITE;
    C2D_DrawRectSolid(20.0f, 5.0f, 0.0f, 360.0f, 6.0f, CLR_DARK_GRAY);
    C2D_DrawRectSolid(200.0f - half, 5.0f, 0.0f, half * 2.0f, 6.0f, strip);

    const float cx = 134.0f, cy = 128.0f, r = 102.0f;
    ui_draw_dial_large(cx, cy, r);
    ui_arc_band(cx, cy, 94.0f, 96.0f, 135.0f, 270.0f, 0.0f, 1.0f, CLR_GRAY, 72);
    ui_arc_band(cx, cy, 82.0f, 92.0f, 135.0f, 270.0f, HUD_REDLINE / HUD_RPM_MAX, 1.0f,
                C2D_Color32(90, 14, 18, 255), 24);
    ui_arc_band(cx, cy, 82.0f, 92.0f, 135.0f, 270.0f, 0.0f, frac, redline ? CLR_RED : CLR_WHITE, 72);
    for (int k = 0; (float)k * 1000.0f <= HUD_RPM_MAX; k++) {
        ui_radial_tick(cx, cy, 74.0f, 81.0f, 135.0f + ((float)k * 1000.0f / HUD_RPM_MAX) * 270.0f,
                       1.8f, CLR_WHITE);
    }
    draw_dial_labels(cx, cy, 63.0f, 135.0f, 270.0f, 1, HUD_RPM_MAX / 1000.0f, HUD_REDLINE / 1000.0f, 0.54f);
    ui_text(cx, cy - 42.0f, 0.44f, CLR_GRAY, C2D_AlignCenter, "x1000 r/min");

    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)g_dtec.speed_kmh);
    else snprintf(spd, sizeof(spd), "---");
    ui_num(cx, cy - 30.0f, 0.64f, CLR_WHITE, C2D_AlignCenter, spd);
    ui_text(cx, cy + 32.0f, 0.48f, CLR_GRAY, C2D_AlignCenter, "km/h");
    if (g_dtec.has_rpm) ui_textf(cx, cy + 50.0f, 0.56f, CLR_GT_BLUE, C2D_AlignCenter, "%d rpm", (int)g_dtec.rpm);
    else ui_text(cx, cy + 50.0f, 0.56f, CLR_GT_BLUE, C2D_AlignCenter, "--- rpm");

    draw_side_dock(266.0f, 20.0f, 126.0f, 49.0f, 4.0f);
    draw_link_dot(393.0f, 233.0f);
}

static void render_forza_style(void) {
    const float cx = 134.0f, cy = 124.0f, r = 104.0f;
    float frac = clamp01f(s_rpm_ease / HUD_RPM_MAX);
    float red_f = HUD_REDLINE / HUD_RPM_MAX;

    ui_draw_dial_large(cx, cy, r);
    ui_arc_band(cx, cy, r - 11.0f, r - 5.0f, 135.0f, 270.0f, red_f, 1.0f,
                C2D_Color32(105, 16, 16, 255), 24);
    ui_arc_band(cx, cy, r - 11.0f, r - 5.0f, 135.0f, 270.0f, 0.0f, frac < red_f ? frac : red_f,
                CLR_WHITE, 72);
    if (frac > red_f) {
        ui_arc_band(cx, cy, r - 11.0f, r - 5.0f, 135.0f, 270.0f, red_f, frac, CLR_RED, 24);
    }

    for (int k = 0; (float)k * 500.0f <= HUD_RPM_MAX; k++) {
        float deg = 135.0f + ((float)k * 500.0f / HUD_RPM_MAX) * 270.0f;
        u32 col = k * 500.0f >= HUD_REDLINE ? CLR_RED : CLR_WHITE;
        ui_radial_tick(cx, cy, (k & 1) ? r - 19.0f : r - 24.0f, r - 13.0f, deg,
                       (k & 1) ? 1.2f : 2.0f, col);
    }
    draw_dial_labels(cx, cy, r - 35.0f, 135.0f, 270.0f, 1, HUD_RPM_MAX / 1000.0f, HUD_REDLINE / 1000.0f, 0.56f);

    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)g_dtec.speed_kmh);
    else snprintf(spd, sizeof(spd), "---");
    ui_num(cx, cy + 20.0f, 0.56f, CLR_WHITE, C2D_AlignCenter, spd);
    ui_text(cx, cy + 72.0f, 0.46f, CLR_GRAY, C2D_AlignCenter, "KM/H");

    ui_needle(cx, cy, 18.0f, r - 14.0f, 135.0f + frac * 270.0f, 3.2f, CLR_ORANGE);

    draw_side_dock(266.0f, 16.0f, 126.0f, 50.0f, 4.0f);
    draw_link_dot(393.0f, 233.0f);
}

static void bng_panel(float x, float y, float w, float h, const char *title) {
    C2D_DrawRectSolid(x, y, 0.0f, w, h, C2D_Color32(18, 22, 30, 245));
    C2D_DrawRectSolid(x, y, 0.0f, w, 3.0f, CLR_ORANGE);
    ui_text(x + 8.0f, y + 6.0f, 0.50f, CLR_GRAY, 0, title);
}

static void render_beamng_style(void) {
    bng_panel(8.0f, 8.0f, 206.0f, 224.0f, "TACHO");
    const float cx = 111.0f, cy = 122.0f;
    float frac = clamp01f(s_rpm_ease / HUD_RPM_MAX);

    ui_arc_band(cx, cy, 70.0f, 84.0f, 135.0f, 270.0f, 0.0f, 1.0f, CLR_DARK_GRAY, 72);
    if (frac > 0.0f) {
        float f_cyan = frac < 0.60f ? frac : 0.60f;
        ui_arc_band(cx, cy, 70.0f, 84.0f, 135.0f, 270.0f, 0.0f, f_cyan, CLR_WHITE, 44);
        if (frac > 0.60f) {
            float f_org = frac < 0.82f ? frac : 0.82f;
            ui_arc_band(cx, cy, 70.0f, 84.0f, 135.0f, 270.0f, 0.60f, f_org, CLR_ORANGE, 20);
        }
        if (frac > 0.82f) {
            ui_arc_band(cx, cy, 70.0f, 84.0f, 135.0f, 270.0f, 0.82f, frac, CLR_RED, 16);
        }
    }
    draw_dial_labels(cx, cy, 58.0f, 135.0f, 270.0f, 2, HUD_RPM_MAX / 1000.0f, HUD_REDLINE / 1000.0f, 0.52f);

    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)g_dtec.speed_kmh);
    else snprintf(spd, sizeof(spd), "---");
    ui_num(cx, cy - 32.0f, 0.62f, CLR_WHITE, C2D_AlignCenter, spd);
    ui_text(cx, cy + 26.0f, 0.48f, CLR_GRAY, C2D_AlignCenter, "km/h");
    if (g_dtec.has_rpm) ui_textf(cx, 204.0f, 0.58f, CLR_ORANGE, C2D_AlignCenter, "%d rpm", (int)g_dtec.rpm);
    else ui_text(cx, 204.0f, 0.58f, CLR_ORANGE, C2D_AlignCenter, "--- rpm");

    /* Engine panel */
    bng_panel(222.0f, 8.0f, 170.0f, 96.0f, "ENGINE");
    if (g_dtec.has_coolant) ui_textf(232.0f, 28.0f, 0.58f, CLR_WHITE, 0, "Temp    %d °C", g_dtec.coolant_c);
    else ui_text(232.0f, 28.0f, 0.58f, CLR_GRAY, 0, "Temp    -- °C");

    ui_textf(232.0f, 48.0f, 0.58f, g_dtec.has_voltage ? CLR_WHITE : CLR_GRAY, 0, "Batt    %s",
             g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V");

    if (g_dtec.has_boost) ui_textf(232.0f, 68.0f, 0.58f, CLR_CYAN, 0, "Boost   %+.0f kPa", g_dtec.boost_kpa);
    else if (g_dtec.has_load) ui_textf(232.0f, 68.0f, 0.58f, CLR_WHITE, 0, "Load    %d%%", (int)g_dtec.engine_load_pct);
    else ui_text(232.0f, 68.0f, 0.58f, CLR_GRAY, 0, "Load    --%%");

    /* Inputs panel (3 wide vertical bars) */
    bng_panel(222.0f, 110.0f, 170.0f, 122.0f, "INPUTS");
    ui_vbar(242.0f, 134.0f, 28.0f, 74.0f, g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f, CLR_GREEN, CLR_DARK_GRAY);
    ui_text(256.0f, 212.0f, 0.46f, CLR_GRAY, C2D_AlignCenter, "THR");

    ui_vbar(294.0f, 134.0f, 28.0f, 74.0f, g_dtec.has_load ? g_dtec.engine_load_pct / 100.0f : 0.0f,
            CLR_GT_BLUE, CLR_DARK_GRAY);
    ui_text(308.0f, 212.0f, 0.46f, CLR_GRAY, C2D_AlignCenter, "LOD");

    ui_vbar(346.0f, 134.0f, 28.0f, 74.0f, g_dtec.has_boost ? g_dtec.boost_kpa / 200.0f : 0.0f,
            CLR_CYAN, CLR_DARK_GRAY);
    ui_text(360.0f, 212.0f, 0.46f, CLR_GRAY, C2D_AlignCenter, "BST");
    draw_link_dot(394.0f, 234.0f);
}

static void draw_analog_dial(float cx, float cy, float r, const char *title,
                             float val, bool has_val, float min_v, float max_v, float red_v,
                             int step_label, const char *unit, u32 accent, float peak_v) {
    ui_draw_dial_small(cx, cy, r);

    const float start_deg = 135.0f;
    const float sweep_deg = 270.0f;

    if (red_v > min_v && red_v < max_v) {
        ui_arc_band(cx, cy, r - 12.0f, r - 7.0f, start_deg, sweep_deg,
                    (red_v - min_v) / (max_v - min_v), 1.0f, C2D_Color32(110, 18, 18, 255), 18);
    }

    for (int t = 0; t <= 16; t++) {
        float f = (float)t / 16.0f;
        float deg = start_deg + f * sweep_deg;
        float v = min_v + f * (max_v - min_v);
        u32 col = (red_v > min_v && v >= red_v) ? CLR_RED : ((t & 1) ? CLR_GRAY : CLR_WHITE);
        ui_radial_tick(cx, cy, (t & 1) ? r - 12.0f : r - 16.0f, r - 7.0f, deg, (t & 1) ? 1.1f : 1.8f, col);
    }

    if (step_label > 0) {
        draw_dial_labels(cx, cy, r - 25.0f, start_deg, sweep_deg, step_label, max_v, red_v > 0.0f ? red_v : max_v + 1.0f, 0.45f);
    }

    if (peak_v > min_v) {
        float p_norm = clamp01f((peak_v - min_v) / (max_v - min_v));
        float pa = (start_deg + p_norm * sweep_deg) * (float)(M_PI / 180.0);
        C2D_DrawCircleSolid(cx + cosf(pa) * (r - 10.0f), cy + sinf(pa) * (r - 10.0f), 0.0f, 2.5f, CLR_YELLOW);
    }

    ui_text(cx, cy - 26.0f, 0.48f, accent, C2D_AlignCenter, title);

    char val_str[16];
    if (has_val) {
        if (fabsf(val) < 10.0f && max_v <= 20.0f) snprintf(val_str, sizeof(val_str), "%.1f", val);
        else snprintf(val_str, sizeof(val_str), "%d", (int)val);
    } else {
        snprintf(val_str, sizeof(val_str), "--");
    }
    ui_text(cx, cy + 20.0f, 0.64f, CLR_WHITE, C2D_AlignCenter, val_str);
    ui_text(cx, cy + 36.0f, 0.44f, CLR_GRAY, C2D_AlignCenter, unit);

    float norm = has_val ? clamp01f((val - min_v) / (max_v - min_v)) : 0.0f;
    ui_needle(cx, cy, 15.0f, r - 11.0f, start_deg + norm * sweep_deg, 2.8f, CLR_RED);
}

static void render_analog_dials(void) {
    draw_header("ANALOG");

    draw_analog_dial(104.0f, 112.0f, 80.0f, "TACH", g_dtec.rpm / 1000.0f, g_dtec.has_rpm,
                     0.0f, 9.0f, 7.0f, 1, "x1000 RPM", CLR_YELLOW, g_dtec.peak_rpm / 1000.0f);
    draw_analog_dial(296.0f, 112.0f, 80.0f, "SPEED", (float)g_dtec.speed_kmh, g_dtec.has_speed,
                     0.0f, 240.0f, 999.0f, 40, "km/h", CLR_CYAN, 0.0f);

    /* Bottom 4-pill widescreen strip */
    const float w = 92.0f;
    char s[20];
    ui_panel(8.0f, 198.0f, w, 34.0f, CLR_PANEL, CLR_BORDER);
    ui_text(16.0f, 202.0f, 0.42f, CLR_GRAY, 0, "THROTTLE");
    if (g_dtec.has_tps) snprintf(s, sizeof(s), "%d%%", (int)g_dtec.tps_pct); else snprintf(s, sizeof(s), "--%%");
    ui_text(16.0f, 214.0f, 0.56f, CLR_GREEN, 0, s);

    ui_panel(105.0f, 198.0f, w, 34.0f, CLR_PANEL, CLR_BORDER);
    ui_text(113.0f, 202.0f, 0.42f, CLR_GRAY, 0, "COOLANT");
    if (g_dtec.has_coolant) snprintf(s, sizeof(s), "%d °C", g_dtec.coolant_c); else snprintf(s, sizeof(s), "-- °C");
    ui_text(113.0f, 214.0f, 0.56f, (g_dtec.has_coolant && g_dtec.coolant_c > 104) ? CLR_RED : CLR_WHITE, 0, s);

    ui_panel(202.0f, 198.0f, w, 34.0f, CLR_PANEL, CLR_BORDER);
    ui_text(210.0f, 202.0f, 0.42f, CLR_GRAY, 0, "BATTERY");
    ui_text(210.0f, 214.0f, 0.56f, g_dtec.has_voltage ? CLR_YELLOW : CLR_GRAY, 0,
            g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V");

    ui_panel(299.0f, 198.0f, 93.0f, 34.0f, CLR_PANEL, CLR_BORDER);
    ui_text(307.0f, 202.0f, 0.42f, CLR_GRAY, 0, g_dtec.has_boost ? "BOOST" : "LOAD");
    if (g_dtec.has_boost) snprintf(s, sizeof(s), "%+.0f kPa", g_dtec.boost_kpa);
    else if (g_dtec.has_load) snprintf(s, sizeof(s), "%d%%", (int)g_dtec.engine_load_pct);
    else snprintf(s, sizeof(s), "--%%");
    ui_text(307.0f, 214.0f, 0.56f, CLR_CYAN, 0, s);
}

static void render_realtime_graph(void) {
    draw_header("LOGGER");

    const float gx = 8.0f, gy = 28.0f, gw = 384.0f, gh = 160.0f;
    ui_panel(gx, gy, gw, gh, CLR_BLACK, CLR_BORDER);

    for (int r = 1; r < 5; r++) {
        float ly = gy + gh * (float)r / 5.0f;
        C2D_DrawRectSolid(gx + 2.0f, ly, 0.0f, gw - 4.0f, 1.0f, C2D_Color32(20, 32, 52, 180));
    }
    for (int c = 1; c < 8; c++) {
        float lx = gx + gw * (float)c / 8.0f;
        C2D_DrawRectSolid(lx, gy + 2.0f, 0.0f, 1.0f, gh - 4.0f, C2D_Color32(20, 32, 52, 140));
    }

    int count = g_dtec.hist_count;
    if (count > 1) {
        float step_x = (gw - 8.0f) / (float)(GRAPH_HISTORY_LEN - 1);
        for (int i = 0; i < count - 1; i++) {
            float x1 = gx + 4.0f + (float)i * step_x;
            float x2 = gx + 4.0f + (float)(i + 1) * step_x;

            if (g_dtec.has_boost) {
                float b1 = clamp01f((g_dtec.hist_boost[i] + 100.0f) / 300.0f);
                float b2 = clamp01f((g_dtec.hist_boost[i + 1] + 100.0f) / 300.0f);
                C2D_DrawLine(x1, gy + gh - 6.0f - b1 * (gh - 12.0f), CLR_CYAN,
                             x2, gy + gh - 6.0f - b2 * (gh - 12.0f), CLR_CYAN, 2.0f, 0.0f);
            }

            float r1 = clamp01f(g_dtec.hist_rpm[i] / HUD_RPM_MAX);
            float r2 = clamp01f(g_dtec.hist_rpm[i + 1] / HUD_RPM_MAX);
            C2D_DrawLine(x1, gy + gh - 6.0f - r1 * (gh - 12.0f), CLR_YELLOW,
                         x2, gy + gh - 6.0f - r2 * (gh - 12.0f), CLR_YELLOW, 2.0f, 0.0f);

            float t1 = clamp01f(g_dtec.hist_tps[i] / 100.0f);
            float t2 = clamp01f(g_dtec.hist_tps[i + 1] / 100.0f);
            C2D_DrawLine(x1, gy + gh - 6.0f - t1 * (gh - 12.0f), CLR_GREEN,
                         x2, gy + gh - 6.0f - t2 * (gh - 12.0f), CLR_GREEN, 2.0f, 0.0f);
        }
    }

    ui_panel(8.0f, 194.0f, 384.0f, 38.0f, CLR_PANEL, CLR_BORDER);
    C2D_DrawCircleSolid(22.0f, 213.0f, 0.0f, 4.0f, CLR_YELLOW);
    ui_textf(32.0f, 204.0f, 0.54f, CLR_WHITE, 0, "RPM %d", g_dtec.has_rpm ? (int)g_dtec.rpm : 0);

    C2D_DrawCircleSolid(148.0f, 213.0f, 0.0f, 4.0f, CLR_GREEN);
    ui_textf(158.0f, 204.0f, 0.54f, CLR_WHITE, 0, "TPS %d%%", g_dtec.has_tps ? (int)g_dtec.tps_pct : 0);

    if (g_dtec.has_boost) {
        C2D_DrawCircleSolid(268.0f, 213.0f, 0.0f, 4.0f, CLR_CYAN);
        ui_textf(278.0f, 204.0f, 0.54f, CLR_CYAN, 0, "BST %+.0fkPa", g_dtec.boost_kpa);
    } else {
        ui_textf(382.0f, 204.0f, 0.52f, CLR_GRAY, C2D_AlignRight, "PEAK %d RPM", (int)g_dtec.peak_rpm);
    }
}

static void render_boost_setup(void) {
    draw_header("STATS");

    struct {
        const char *label;
        char val[20];
        u32 col;
        bool bar;
        float frac;
    } cards[6];

    if (g_dtec.has_boost) {
        cards[0].label = "PEAK BOOST";
        snprintf(cards[0].val, 20, "%+.0f kPa", g_dtec.peak_boost);
        cards[0].col = CLR_CYAN;
        cards[0].bar = true;
        cards[0].frac = g_dtec.peak_boost / 200.0f;
    } else {
        cards[0].label = "COOLANT TEMP";
        if (g_dtec.has_coolant) snprintf(cards[0].val, 20, "%d °C", g_dtec.coolant_c);
        else snprintf(cards[0].val, 20, "-- °C");
        cards[0].col = (g_dtec.has_coolant && g_dtec.coolant_c > 104) ? CLR_RED : CLR_ORANGE;
        cards[0].bar = true;
        cards[0].frac = g_dtec.has_coolant ? clamp01f((g_dtec.coolant_c - 40.0f) / 80.0f) : 0.0f;
    }

    cards[1].label = "PEAK RPM";
    if (g_dtec.has_rpm) snprintf(cards[1].val, 20, "%d RPM", (int)g_dtec.peak_rpm);
    else snprintf(cards[1].val, 20, "-- RPM");
    cards[1].col = CLR_YELLOW;
    cards[1].bar = true;
    cards[1].frac = g_dtec.peak_rpm / HUD_RPM_MAX;

    cards[2].label = "MANIFOLD MAP";
    if (g_dtec.has_map) snprintf(cards[2].val, 20, "%d kPa", g_dtec.map_kpa);
    else snprintf(cards[2].val, 20, "-- kPa");
    cards[2].col = CLR_CYAN;
    cards[2].bar = true;
    cards[2].frac = g_dtec.has_map ? (float)g_dtec.map_kpa / 250.0f : 0.0f;

    cards[3].label = "BATTERY";
    snprintf(cards[3].val, 20, "%s", g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V");
    cards[3].col = CLR_WHITE;
    cards[3].bar = false;
    cards[3].frac = 0.0f;

    cards[4].label = "THROTTLE";
    if (g_dtec.has_tps) snprintf(cards[4].val, 20, "%d%%", (int)g_dtec.tps_pct);
    else snprintf(cards[4].val, 20, "--%%");
    cards[4].col = CLR_GREEN;
    cards[4].bar = true;
    cards[4].frac = g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f;

    cards[5].label = "AUTO-POLL";
    snprintf(cards[5].val, 20, "%s", g_dtec.auto_poll ? "ACTIVE" : "PAUSED");
    cards[5].col = g_dtec.auto_poll ? CLR_GREEN : CLR_GRAY;
    cards[5].bar = false;
    cards[5].frac = 0.0f;

    for (int i = 0; i < 6; i++) {
        float cx = 8.0f + (float)(i % 2) * 196.0f;
        float cy = 28.0f + (float)(i / 2) * 68.0f;
        draw_metric_card(cx, cy, 188.0f, 62.0f, cards[i].label, cards[i].val,
                         cards[i].col, cards[i].bar, cards[i].frac, cards[i].col);
    }
}

static const char *enum_text(uint8_t pid, int v) {
    static const char *const standards[] = {NULL, "OBD-II CARB", "OBD EPA", "OBD+OBD-II", "OBD-I", "NON-OBD",
        "EOBD", "EOBD+OBD-II", "EOBD+OBD", "EOBD+OBD+II", "JOBD", "JOBD+OBD-II", "JOBD+EOBD", "JOBD+EOBD+II"};
    static const char *const fuels[] = {NULL, "GASOLINE", "METHANOL", "ETHANOL", "DIESEL", "LPG", "CNG",
        "PROPANE", "ELECTRIC"};
    if (pid == 0x1C) return v > 0 && v < 14 ? standards[v] : NULL;
    if (v > 0 && v < 9) return fuels[v];
    return (v >= 9 && v <= 16) || v == 23 ? "BI-FUEL" : v >= 17 && v <= 22 ? "HYBRID" : NULL;
}

static void pid_text(int i, bool freeze, char *out, size_t n) {
    bool has = i >= 0 && (freeze ? g_dtec.ff_has[i] : g_dtec.pid_has[i]);
    if (!has) { snprintf(out, n, "--"); return; }
    const DTEC_Pid *p = &g_dtec_pids[i];
    float v = freeze ? g_dtec.ff_val[i] : g_dtec.pid_val[i];
    if (p->pid == 0x1C || p->pid == 0x51) {
        const char *name = enum_text(p->pid, (int)v);
        if (name) snprintf(out, n, "%s", name);
        else snprintf(out, n, "TYPE %d", (int)v);
        return;
    }
    snprintf(out, n, "%.*f %s", p->decimals, v, p->unit);
}

static void render_codes(void) {
    draw_header("CODES");

    bool mil = g_dtec.has_monitors && g_dtec.mil_on;
    ui_panel(8.0f, 28.0f, 384.0f, 40.0f, CLR_PANEL, mil ? CLR_ORANGE : CLR_BORDER);
    C2D_DrawCircleSolid(28.0f, 48.0f, 0.0f, 10.0f, mil ? CLR_ORANGE : CLR_DARK_GRAY);
    ui_text(48.0f, 32.0f, 0.58f, mil ? CLR_ORANGE : CLR_WHITE, 0,
            g_dtec.scanning ? "READING ECU..." : !g_dtec.has_monitors ? "NOT SCANNED" :
            mil ? "CHECK ENGINE ON" : "CHECK ENGINE OFF");

    int counts[3] = {0};
    for (int i = 0; i < g_dtec.dtc_count; i++) counts[g_dtec.dtc_kind[i] % 3]++;
    ui_textf(48.0f, 50.0f, 0.48f, CLR_GRAY, 0, "%d STORED   •   %d PENDING   •   %d PERMANENT",
             counts[0], counts[1], counts[2]);

    /* Left: Readiness + Vehicle ID */
    ui_panel(8.0f, 74.0f, 194.0f, 134.0f, CLR_PANEL, CLR_BORDER);
    int total = __builtin_popcount(g_dtec.mon_supported);
    int ready = __builtin_popcount(g_dtec.mon_supported & ~g_dtec.mon_incomplete);
    if (g_dtec.has_monitors) {
        ui_textf(16.0f, 82.0f, 0.50f, ready == total ? CLR_GREEN : CLR_YELLOW, 0,
                 "READINESS  %d/%d READY", ready, total);
        ui_hbar(16.0f, 100.0f, 178.0f, 5.0f, total ? (float)ready / (float)total : 0.0f,
                ready == total ? CLR_GREEN : CLR_YELLOW, CLR_DARK_GRAY);
    } else {
        ui_text(16.0f, 82.0f, 0.50f, CLR_GRAY, 0, "READINESS  --");
        ui_hbar(16.0f, 100.0f, 178.0f, 5.0f, 0.0f, CLR_GRAY, CLR_DARK_GRAY);
    }

    char a[20], b[20], line[64];
    snprintf(line, sizeof(line), "VIN: %s", g_dtec.vin[0] ? g_dtec.vin : "--");
    ui_text_clipped(16.0f, 114.0f, 178.0f, 0.48f, CLR_WHITE, line);
    ui_text_clipped(16.0f, 134.0f, 178.0f, 0.46f, CLR_GRAY, g_dtec.protocol[0] ? g_dtec.protocol : "PROTOCOL --");
    pid_text(dtec_obd_pid_index(0x31), false, a, sizeof(a));
    pid_text(dtec_obd_pid_index(0x21), false, b, sizeof(b));
    snprintf(line, sizeof(line), "CLR: %s", a);
    ui_text_clipped(16.0f, 156.0f, 178.0f, 0.46f, CLR_GRAY, line);
    snprintf(line, sizeof(line), "MIL DIST: %s", b);
    ui_text_clipped(16.0f, 174.0f, 178.0f, 0.46f, CLR_GRAY, line);

    /* Right: Freeze Frame */
    ui_panel(208.0f, 74.0f, 184.0f, 134.0f, CLR_PANEL, CLR_BORDER);
    if (g_dtec.ff_dtc) {
        dtec_obd_format_dtc(g_dtec.ff_dtc, a);
        ui_textf(216.0f, 82.0f, 0.52f, CLR_CYAN, 0, "FREEZE FRAME  %s", a);
    } else {
        ui_text(216.0f, 82.0f, 0.52f, CLR_CYAN, 0, "FREEZE FRAME  NONE");
    }
    for (int i = 0, row = 0; i < DTEC_PID_COUNT && row < 5; i++) {
        if (!g_dtec.ff_has[i]) continue;
        float ry = 104.0f + (float)(row++) * 20.0f;
        ui_text_clipped(216.0f, ry, 96.0f, 0.46f, CLR_GRAY, g_dtec_pids[i].name);
        pid_text(i, true, a, sizeof(a));
        ui_text(384.0f, ry, 0.48f, CLR_WHITE, C2D_AlignRight, a);
    }

    ui_text(200.0f, 215.0f, 0.50f, g_dtec.clear_armed ? CLR_RED : CLR_WHITE, C2D_AlignCenter,
            g_dtec.clear_armed ? "ENGINE OFF  +  PRESS [Y] AGAIN TO ERASE" : "[A] SCAN CODES   •   [Y] CLEAR CODES");
}

static void render_sensors(void) {
    draw_header("SENSORS");

    int list[DTEC_PID_COUNT];
    int n = dtec_obd_sensor_list(list);
    int pages = n ? (n + DTEC_SENSOR_ROWS - 1) / DTEC_SENSOR_ROWS : 1;
    int page = g_dtec.sensor_page % pages;
    char v[48];

    /* 2-column x 4-row widescreen grid for the 8 visible PIDs */
    for (int r = 0; r < DTEC_SENSOR_ROWS && page * DTEC_SENSOR_ROWS + r < n; r++) {
        int i = list[page * DTEC_SENSOR_ROWS + r];
        float x = (r < 4) ? 8.0f : 204.0f;
        float y = 28.0f + (float)(r % 4) * 46.0f;
        ui_panel(x, y, 188.0f, 41.0f, CLR_PANEL, CLR_BORDER);
        ui_text_clipped(x + 8.0f, y + 5.0f, 172.0f, 0.44f, CLR_GRAY, g_dtec_pids[i].name);
        pid_text(i, false, v, sizeof(v));
        ui_text(x + 180.0f, y + 19.0f, 0.56f, g_dtec.pid_has[i] ? CLR_WHITE : CLR_GRAY, C2D_AlignRight, v);
    }

    ui_textf(12.0f, 216.0f, 0.48f, CLR_CYAN, 0, "PAGE %d/%d   •   %d PIDS", page + 1, pages, n);
    ui_text(388.0f, 216.0f, 0.48f, CLR_GRAY, C2D_AlignRight, "[UP/DOWN] PAGE   •   [R] REC CSV");
}

static void render_ecu_info(void) {
    draw_header("ECU");

    char line[64], v[32];
    ui_panel(8.0f, 28.0f, 194.0f, 180.0f, CLR_PANEL, CLR_BORDER);
    ui_text(16.0f, 34.0f, 0.52f, CLR_CYAN, 0, "ECU IDENTITY");

    const char *rows[][2] = {{"VIN", g_dtec.vin}, {"ECU", g_dtec.ecu_name}, {"CAL", g_dtec.cal_id}};
    for (int r = 0; r < 3; r++) {
        snprintf(line, sizeof(line), "%s: %s", rows[r][0], rows[r][1][0] ? rows[r][1] : "--");
        ui_text_clipped(16.0f, 56.0f + (float)r * 22.0f, 178.0f, 0.50f, CLR_WHITE, line);
    }
    pid_text(dtec_obd_pid_index(0x1C), false, v, sizeof(v));
    snprintf(line, sizeof(line), "OBD: %s", v);
    ui_text_clipped(16.0f, 126.0f, 178.0f, 0.48f, CLR_GRAY, line);

    pid_text(dtec_obd_pid_index(0x51), false, v, sizeof(v));
    snprintf(line, sizeof(line), "FUEL: %s", v);
    ui_text_clipped(16.0f, 148.0f, 178.0f, 0.48f, CLR_GRAY, line);

    pid_text(dtec_obd_pid_index(0xA6), false, v, sizeof(v));
    snprintf(line, sizeof(line), "ODO: %s", v);
    ui_text_clipped(16.0f, 170.0f, 178.0f, 0.48f, CLR_GRAY, line);

    ui_panel(208.0f, 28.0f, 184.0f, 180.0f, CLR_PANEL, CLR_BORDER);
    ui_text(216.0f, 34.0f, 0.52f, CLR_CYAN, 0, "MISFIRE MONITORS");
    if (!g_dtec.misfire_cyls) {
        const char *msg = g_dtec.scanning ? "READING ECU..." :
            !g_dtec.codes_read ? "PRESS [A] TO SCAN" : "NOT REPORTED";
        ui_text(300.0f, 108.0f, 0.52f, CLR_GRAY, C2D_AlignCenter, msg);
    } else {
        ui_text(216.0f, 54.0f, 0.42f, CLR_GRAY, 0, "CYL   DRIVE   10-AVG");
        for (int c = 0, k = 0; c < DTEC_MAX_CYLINDERS && k < 12; c++) {
            if (!(g_dtec.misfire_cyls & (1u << c))) continue;
            float cx = 216.0f + (float)(k / 6) * 88.0f;
            float cy = 72.0f + (float)(k % 6) * 20.0f;
            ui_textf(cx, cy, 0.46f, g_dtec.misfire_drive[c] ? CLR_ORANGE : CLR_WHITE, 0,
                     "C%-2d  %3u  %3u", c + 1, g_dtec.misfire_drive[c], g_dtec.misfire_avg[c]);
            k++;
        }
    }

    ui_text(200.0f, 215.0f, 0.48f, CLR_WHITE, C2D_AlignCenter, "[A] SCAN ECU   •   [R] REC CSV");
}

static void render_obd_terminal(void) {
    draw_header("TERMINAL");

    ui_panel(8.0f, 28.0f, 384.0f, 176.0f, CLR_BLACK, CLR_BORDER);
    ui_text(16.0f, 34.0f, 0.48f, CLR_YELLOW, 0, "LIVE OBD-II PACKET LOG");

    for (int i = 0; i < OBD_LOG_LINES && i < g_dtec.log_count; i++) {
        float ly = 54.0f + (float)i * 14.5f;
        u32 col = CLR_WHITE;
        if (g_dtec.log_lines[i][0] == '>') col = CLR_GREEN;
        else if (strstr(g_dtec.log_lines[i], "41")) col = CLR_CYAN;
        ui_text_clipped(16.0f, ly, 368.0f, 0.46f, col, g_dtec.log_lines[i]);
    }

    ui_panel(8.0f, 208.0f, 384.0f, 26.0f, CLR_PANEL, CLR_BORDER);
    ui_text(200.0f, 214.0f, 0.46f, CLR_GRAY, C2D_AlignCenter,
            "[SELECT/D-PAD] VIEW   •   [START] POLL   •   [A] ATRV   •   [B] RPM   •   [X] SPD");
}

static void render_car_select(void) {
    draw_header("VEHICLE");

    int sel = g_dtec.selected_profile;
    if (sel < 0 || sel >= DTEC_PROFILE_COUNT) sel = 0;
    const DTEC_CarProfile *p = &g_dtec_profiles[sel];

    ui_panel(8.0f, 28.0f, 198.0f, 176.0f, CLR_PANEL, CLR_BORDER);
    ui_text(18.0f, 36.0f, 0.56f, CLR_CYAN, 0, p->brand);
    ui_text_clipped(18.0f, 58.0f, 178.0f, 0.68f, CLR_WHITE, p->name);
    ui_text_clipped(18.0f, 86.0f, 178.0f, 0.48f, CLR_GRAY, p->desc);

    ui_textf(18.0f, 126.0f, 0.50f, CLR_WHITE, 0, "PROTOCOL:  %s", p->protocol_cmd ? p->protocol_cmd : "AUTO");
    ui_textf(18.0f, 148.0f, 0.50f, CLR_WHITE, 0, "HEADER:      %s", p->header_cmd ? p->header_cmd : "BROADCAST");

    ui_panel(212.0f, 28.0f, 180.0f, 176.0f, CLR_PANEL, CLR_BORDER);
    ui_text(222.0f, 36.0f, 0.52f, CLR_CYAN, 0, "3DS NETWORK");
    ui_textf(222.0f, 62.0f, 0.50f, CLR_WHITE, 0, "SSID: %s", g_dtec.wifi_ssid[0] ? g_dtec.wifi_ssid : "--");
    ui_textf(222.0f, 86.0f, 0.50f, CLR_GRAY, 0, "LOCAL: %s", g_dtec.local_ip[0] ? g_dtec.local_ip : "--");
    ui_textf(222.0f, 110.0f, 0.50f, CLR_WHITE, 0, "TARGET: %s:%u", g_dtec.target_ip, (unsigned)g_dtec.target_port);
    ui_text_clipped(222.0f, 140.0f, 160.0f, 0.48f, CLR_CYAN,
                    g_dtec.status_msg[0] ? g_dtec.status_msg : "Waiting for link");

    ui_panel(8.0f, 208.0f, 384.0f, 26.0f, CLR_PANEL, CLR_BORDER);
    ui_text(200.0f, 214.0f, 0.48f, CLR_WHITE, C2D_AlignCenter,
            "[UP/DOWN] CHOOSE PROFILE   •   [A / START] CONNECT");
}

static void render_settings(void) {
    draw_header("SETUP");

    ui_panel(8.0f, 28.0f, 190.0f, 180.0f, CLR_PANEL, CLR_BORDER);
    ui_text(18.0f, 36.0f, 0.72f, CLR_WHITE, 0, "SETTINGS");
    ui_text(18.0f, 64.0f, 0.50f, CLR_CYAN, 0, "SOUND EFFECTS");

    ui_panel(18.0f, 88.0f, 170.0f, 56.0f, CLR_PANEL_DARK, CLR_BORDER_HL);
    ui_text(28.0f, 94.0f, 0.80f, g_dtec.sound_enabled ? CLR_GREEN : CLR_GRAY, 0,
            g_dtec.sound_enabled ? "ON" : "OFF");
    ui_text(28.0f, 122.0f, 0.44f, CLR_GRAY, 0, "NDSP MENU + LINK CUES");
    ui_text(18.0f, 162.0f, 0.48f, CLR_WHITE, 0, "[A] TOGGLE   [X] TEST");
    ui_text(18.0f, 182.0f, 0.46f, CLR_GRAY, 0, "[B] RETURN TO VIEW");

    ui_panel(204.0f, 28.0f, 188.0f, 180.0f, CLR_PANEL, CLR_BORDER);
    ui_text(214.0f, 36.0f, 0.54f, CLR_CYAN, 0, "3DS CONTROLS");
    ui_text(214.0f, 64.0f, 0.46f, CLR_WHITE, 0, "SELECT / < >  : Switch View");
    ui_text(214.0f, 84.0f, 0.46f, CLR_WHITE, 0, "D-PAD L / R   : Prev / Next");
    ui_text(214.0f, 104.0f, 0.46f, CLR_WHITE, 0, "START         : Auto-Poll");
    ui_text(214.0f, 124.0f, 0.46f, CLR_WHITE, 0, "R             : CSV SD Log");
    ui_text(214.0f, 144.0f, 0.46f, CLR_WHITE, 0, "START+SELECT  : Exit App");
    ui_text(214.0f, 172.0f, 0.44f, CLR_GRAY, 0, "INI: sdmc:/dsobd.ini");
}

void dtec_3ds_render_top(void) {
    float rpm = g_dtec.has_rpm ? g_dtec.rpm : 0.0f;
    s_rpm_ease += (rpm - s_rpm_ease) * 0.35f;
    s_hud_frame++;

    if (g_dtec.screen_mode != DTEC_MODE_BIG_SPEED) {
        ui_draw_deck_top();
    }

    switch (g_dtec.screen_mode) {
        case DTEC_MODE_SETTINGS:       render_settings(); break;
        case DTEC_MODE_CAR_SELECT:     render_car_select(); break;
        case DTEC_MODE_MULTI_GAUGE:    render_multi_gauge(); break;
        case DTEC_MODE_DIGITAL_SPEED:  render_digital_speed(); break;
        case DTEC_MODE_BIG_SPEED:      render_big_speed(); break;
        case DTEC_MODE_ARC_TACH:       render_arc_tach(); break;
        case DTEC_MODE_SIM_DASH:       render_sim_dash(); break;
        case DTEC_MODE_GT_STYLE:       render_gt_style(); break;
        case DTEC_MODE_FORZA_STYLE:    render_forza_style(); break;
        case DTEC_MODE_BEAMNG_STYLE:   render_beamng_style(); break;
        case DTEC_MODE_ANALOG_DIALS:   render_analog_dials(); break;
        case DTEC_MODE_REALTIME_GRAPH: render_realtime_graph(); break;
        case DTEC_MODE_BOOST_SETUP:    render_boost_setup(); break;
        case DTEC_MODE_CODES:          render_codes(); break;
        case DTEC_MODE_SENSORS:        render_sensors(); break;
        case DTEC_MODE_ECU_INFO:       render_ecu_info(); break;
        case DTEC_MODE_OBD_TERMINAL:   render_obd_terminal(); break;
        default: break;
    }
}
