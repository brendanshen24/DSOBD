#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui_internal_psp.h"
#include "dtec_obd.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define HUD_RPM_MAX 8500.0f

const char *const g_psp_mode_names[PSP_MODE_COUNT] = {
    "VEHICLE", "MULTI", "DIGITAL", "BIG SPEED", "ARC TACH", "SIM LCD",
    "GT STYLE", "FORZA STYLE", "BEAMNG STYLE", "ANALOG", "SCOPE", "STATS",
    "CODES", "SENSORS", "ECU INFO", "TERMINAL", "SETTINGS", "WI-FI LINK"
};

float g_psp_rpm_ease = 0.0f;
uint32_t g_psp_hud_frame = 0;

float psp_clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static float shift_rpm_f(void) {
    return (float)(g_psp.shift_rpm >= 4000 ? g_psp.shift_rpm : 7000);
}

uint32_t psp_zone_color(float f) {
    float red_frac = shift_rpm_f() / HUD_RPM_MAX;
    if (f < red_frac * 0.72f) return CLR_CYAN;
    if (f < red_frac) return CLR_YELLOW;
    return CLR_RED;
}

float psp_display_speed(uint32_t kmh) {
    return g_psp.use_imperial ? ((float)kmh * 0.621371f) : (float)kmh;
}

const char *psp_speed_unit(bool upper) {
    if (g_psp.use_imperial) return upper ? "MPH" : "mph";
    return upper ? "KM/H" : "km/h";
}

void psp_format_temp(int c, char *out, size_t n) {
    if (g_psp.use_imperial) {
        snprintf(out, n, "%d °F", (c * 9) / 5 + 32);
    } else {
        snprintf(out, n, "%d °C", c);
    }
}

float psp_boost_display_val(float kpa) {
    return g_psp.use_imperial ? (kpa * 0.145038f) : kpa;
}

float psp_boost_max_display(void) {
    return g_psp.use_imperial ? 29.0f : 200.0f;
}

const char *psp_boost_unit(void) {
    return g_psp.use_imperial ? "PSI" : "kPa";
}

void psp_format_boost(float kpa, char *out, size_t n) {
    if (g_psp.use_imperial) {
        snprintf(out, n, "%+.1f PSI", kpa * 0.145038f);
    } else {
        snprintf(out, n, "%+.0f kPa", kpa);
    }
}

void psp_draw_link_dot(float x, float y) {
    uint32_t col = CLR_RED;
    if (g_dtec.net_status == NET_OBD_CONNECTED) col = CLR_GREEN;
    else if (g_dtec.net_status == NET_TCP_CONNECTING || g_dtec.net_status == NET_WIFI_CONNECTING) col = CLR_YELLOW;
    ui_circle(x, y, 4.0f, col, 14);
}

void psp_draw_header(const char *mode_name) {
    ui_rect_solid(0.0f, 0.0f, PSP_SCR_W, 24.0f, CLR_PANEL);
    ui_rect_solid(0.0f, 24.0f, PSP_SCR_W, 1.0f, CLR_BORDER);

    ui_text(10.0f, 5.0f, 0.80f, CLR_CYAN, UI_ALIGN_LEFT, "PSPOBD");

    if (g_dtec.logging) {
        if ((g_psp_hud_frame & 16u) == 0) {
            ui_circle(84.0f, 12.0f, 3.5f, CLR_RED, 12);
        }
        ui_text(92.0f, 6.0f, 0.66f, CLR_RED, UI_ALIGN_LEFT, "REC");
    } else {
        ui_panel(78.0f, 5.0f, 32.0f, 15.0f, CLR_PANEL_DARK, CLR_BORDER);
        ui_text(94.0f, 7.0f, 0.56f, CLR_GRAY, UI_ALIGN_CENTER, "PSP");
    }

    if (g_dtec.profile_confirmed && g_dtec.car_brand[0]) {
        ui_textf(236.0f, 6.0f, 0.68f, CLR_WHITE, UI_ALIGN_CENTER, "%s  •  %s",
                 g_dtec.car_brand, mode_name);
    } else {
        ui_text(236.0f, 6.0f, 0.70f, CLR_WHITE, UI_ALIGN_CENTER, mode_name);
    }

    /* PSP Hardware Battery Pill */
    if (g_psp.battery_pct >= 0) {
        uint32_t bcol = g_psp.battery_charging ? CLR_CYAN :
                        (g_psp.battery_pct <= 20 ? CLR_RED : CLR_GREEN);
        ui_panel(336.0f, 4.0f, 62.0f, 16.0f, CLR_PANEL_DARK, CLR_BORDER);
        ui_textf(367.0f, 6.0f, 0.54f, bcol, UI_ALIGN_CENTER,
                 g_psp.battery_charging ? "CHG %d%%" : "BAT %d%%", g_psp.battery_pct);
    } else if (g_psp.ac_online) {
        ui_panel(336.0f, 4.0f, 62.0f, 16.0f, CLR_PANEL_DARK, CLR_BORDER);
        ui_text(367.0f, 6.0f, 0.54f, CLR_CYAN, UI_ALIGN_CENTER, "AC PWR");
    }

    /* Adapter Link Badge */
    if (g_dtec.net_status == NET_OBD_CONNECTED) {
        ui_panel(404.0f, 4.0f, 68.0f, 16.0f, CLR_GREEN_BG, CLR_GREEN);
        ui_text(438.0f, 6.0f, 0.56f, CLR_GREEN, UI_ALIGN_CENTER, "LIVE");
    } else if (g_dtec.net_status == NET_TCP_CONNECTING || g_dtec.net_status == NET_WIFI_CONNECTING) {
        ui_panel(404.0f, 4.0f, 68.0f, 16.0f, CLR_YELLOW_BG, CLR_YELLOW);
        ui_text(438.0f, 6.0f, 0.56f, CLR_YELLOW, UI_ALIGN_CENTER, "LINKING");
    } else {
        ui_panel(404.0f, 4.0f, 68.0f, 16.0f, CLR_RED_BG, CLR_RED);
        ui_text(438.0f, 6.0f, 0.56f, CLR_WHITE, UI_ALIGN_CENTER, "NO LINK");
    }
}

void psp_draw_footer(const char *hint_mode) {
    (void)hint_mode;
    if (g_psp.clean_hud &&
        g_dtec.screen_mode >= DTEC_MODE_MULTI_GAUGE &&
        g_dtec.screen_mode <= DTEC_MODE_BOOST_SETUP) {
        return;
    }

    ui_rect_solid(0.0f, 248.0f, PSP_SCR_W, 24.0f, CLR_PANEL_DARK);
    ui_rect_solid(0.0f, 248.0f, PSP_SCR_W, 1.0f, CLR_BORDER);

    psp_draw_link_dot(12.0f, 260.0f);
    if (g_dtec.logging) {
        ui_textf(22.0f, 254.0f, 0.58f, CLR_RED, UI_ALIGN_LEFT, "REC %s (%lu)",
                 g_dtec.log_file, (unsigned long)g_dtec.log_rows);
    } else if (g_dtec.net_status == NET_OBD_CONNECTED) {
        ui_textf(22.0f, 254.0f, 0.56f, CLR_GRAY, UI_ALIGN_LEFT, "%s  •  RX %lu",
                 g_dtec.wifi_ssid[0] ? g_dtec.wifi_ssid : "OBD",
                 (unsigned long)g_dtec.lines_rx);
    } else {
        ui_text_clipped(22.0f, 254.0f, 188.0f, 0.56f, CLR_GRAY,
                        g_dtec.status_msg[0] ? g_dtec.status_msg : "Waiting for adapter");
    }

    /* Contextual PlayStation button hints on right half */
    if (g_dtec.screen_mode == DTEC_MODE_CODES) {
        ui_psp_btn_badge(216.0f, 253.0f, "X", "SCAN", CLR_CYAN);
        ui_psp_btn_badge(276.0f, 253.0f, "^", g_dtec.clear_armed ? "CONFIRM" : "CLEAR",
                         g_dtec.clear_armed ? CLR_RED : CLR_GREEN);
        ui_psp_btn_badge(352.0f, 253.0f, "[]", "REC", CLR_PURPLE);
        ui_psp_btn_badge(410.0f, 253.0f, "SEL", "MENU", CLR_GRAY);
    } else if (g_dtec.screen_mode == DTEC_MODE_SENSORS) {
        ui_psp_btn_badge(218.0f, 253.0f, "U/D", "PAGE", CLR_CYAN);
        ui_psp_btn_badge(284.0f, 253.0f, "[]", "REC", CLR_PURPLE);
        ui_psp_btn_badge(342.0f, 253.0f, "L/R", "VIEW", CLR_WHITE);
        ui_psp_btn_badge(410.0f, 253.0f, "SEL", "MENU", CLR_GRAY);
    } else if (g_dtec.screen_mode == DTEC_MODE_ECU_INFO) {
        ui_psp_btn_badge(228.0f, 253.0f, "X", "SCAN", CLR_CYAN);
        ui_psp_btn_badge(292.0f, 253.0f, "[]", "REC", CLR_PURPLE);
        ui_psp_btn_badge(352.0f, 253.0f, "L/R", "VIEW", CLR_WHITE);
        ui_psp_btn_badge(414.0f, 253.0f, "SEL", "MENU", CLR_GRAY);
    } else if (g_dtec.screen_mode == DTEC_MODE_OBD_TERMINAL) {
        ui_psp_btn_badge(212.0f, 253.0f, "U/D", "CMD", CLR_WHITE);
        ui_psp_btn_badge(274.0f, 253.0f, "X", "SEND", CLR_CYAN);
        ui_psp_btn_badge(334.0f, 253.0f, "[]", "REC", CLR_PURPLE);
        ui_psp_btn_badge(410.0f, 253.0f, "SEL", "MENU", CLR_GRAY);
    } else if (g_dtec.screen_mode == DTEC_MODE_REALTIME_GRAPH) {
        ui_psp_btn_badge(202.0f, 253.0f, "STICK", "SCRUB", CLR_CYAN);
        ui_psp_btn_badge(282.0f, 253.0f, "^", "LIVE", CLR_GREEN);
        ui_psp_btn_badge(342.0f, 253.0f, "[]", "REC", CLR_PURPLE);
        ui_psp_btn_badge(410.0f, 253.0f, "SEL", "MENU", CLR_GRAY);
    } else if (g_dtec.screen_mode == DTEC_MODE_CAR_SELECT) {
        ui_psp_btn_badge(216.0f, 253.0f, "U/D", "PICK", CLR_WHITE);
        ui_psp_btn_badge(282.0f, 253.0f, "X", "APPLY", CLR_CYAN);
        ui_psp_btn_badge(348.0f, 253.0f, "^", "WIFI", CLR_GREEN);
        ui_psp_btn_badge(410.0f, 253.0f, "SEL", "MENU", CLR_GRAY);
    } else if (g_dtec.screen_mode == PSP_MODE_WIFI) {
        ui_psp_btn_badge(210.0f, 253.0f, "U/D", "LIST", CLR_WHITE);
        ui_psp_btn_badge(274.0f, 253.0f, "X", "LINK", CLR_CYAN);
        ui_psp_btn_badge(336.0f, 253.0f, "^", "IP/PORT", CLR_GREEN);
        ui_psp_btn_badge(414.0f, 253.0f, "[]", "SCAN", CLR_PURPLE);
    } else if (g_dtec.screen_mode == DTEC_MODE_SETTINGS) {
        ui_psp_btn_badge(214.0f, 253.0f, "U/D", "ITEM", CLR_WHITE);
        ui_psp_btn_badge(278.0f, 253.0f, "X", "SET", CLR_CYAN);
        ui_psp_btn_badge(336.0f, 253.0f, "^", "TEST", CLR_GREEN);
        ui_psp_btn_badge(410.0f, 253.0f, "SEL", "MENU", CLR_GRAY);
    } else {
        ui_psp_btn_badge(210.0f, 253.0f, "L/R", "VIEW", CLR_WHITE);
        ui_psp_btn_badge(274.0f, 253.0f, "[]", "REC", CLR_PURPLE);
        ui_psp_btn_badge(332.0f, 253.0f, "O", "HUD", CLR_RED);
        ui_psp_btn_badge(390.0f, 253.0f, "^", "PEAK", CLR_GREEN);
    }
}

void psp_draw_metric_card(float x, float y, float w, float h,
                          const char *label, const char *val, uint32_t val_col,
                          bool has_bar, float frac, uint32_t bar_col) {
    ui_panel(x, y, w, h, CLR_PANEL, CLR_BORDER);
    ui_text(x + 8.0f, y + 5.0f, 0.56f, CLR_GRAY, UI_ALIGN_LEFT, label);
    ui_text_clipped(x + 8.0f, y + 19.0f, w - 16.0f, 0.86f, val_col, val);
    if (has_bar) {
        ui_hbar(x + 8.0f, y + h - 8.0f, w - 16.0f, 4.0f, frac, bar_col, CLR_DARK_GRAY);
    }
}

void psp_draw_side_dock(float x, float y0, float w, float card_h, float gap) {
    char buf[24];
    float y = y0;

    if (g_dtec.has_coolant) psp_format_temp(g_dtec.coolant_c, buf, sizeof(buf));
    else snprintf(buf, sizeof(buf), g_psp.use_imperial ? "-- °F" : "-- °C");
    uint32_t cool_col = (g_dtec.has_coolant && g_dtec.coolant_c > 104) ? CLR_RED : CLR_WHITE;
    float cool_f = g_dtec.has_coolant ? psp_clamp01(((float)g_dtec.coolant_c - 40.0f) / 80.0f) : 0.0f;
    psp_draw_metric_card(x, y, w, card_h, "COOLANT", buf, cool_col,
                         true, cool_f, cool_col == CLR_RED ? CLR_RED : CLR_ORANGE);
    y += card_h + gap;

    if (g_dtec.has_tps) snprintf(buf, sizeof(buf), "%d%%", (int)g_dtec.tps_pct);
    else snprintf(buf, sizeof(buf), "--%%");
    psp_draw_metric_card(x, y, w, card_h, "THROTTLE", buf, CLR_WHITE,
                         true, g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f, CLR_GREEN);
    y += card_h + gap;

    if (g_dtec.has_boost) {
        psp_format_boost(g_dtec.boost_kpa, buf, sizeof(buf));
        float bfrac = psp_boost_display_val(g_dtec.boost_kpa) / psp_boost_max_display();
        psp_draw_metric_card(x, y, w, card_h, "BOOST", buf, CLR_CYAN, true, bfrac, CLR_CYAN);
    } else {
        if (g_dtec.has_load) snprintf(buf, sizeof(buf), "%d%%", (int)g_dtec.engine_load_pct);
        else snprintf(buf, sizeof(buf), "--%%");
        psp_draw_metric_card(x, y, w, card_h, "ENG LOAD", buf, CLR_WHITE,
                             true, g_dtec.has_load ? g_dtec.engine_load_pct / 100.0f : 0.0f, CLR_PURPLE);
    }
    y += card_h + gap;

    snprintf(buf, sizeof(buf), "%s", g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V");
    float vnum = 0.0f;
    if (g_dtec.has_voltage) sscanf(g_dtec.battery_voltage, "%f", &vnum);
    psp_draw_metric_card(x, y, w, card_h, "BATTERY", buf,
                         g_dtec.has_voltage ? CLR_WHITE : CLR_GRAY,
                         true, g_dtec.has_voltage ? psp_clamp01((vnum - 11.0f) / 4.0f) : 0.0f, CLR_YELLOW);
}

void psp_draw_shift_lights(float y, float h) {
    const int n = 24;
    float margin = 8.0f;
    float total_w = PSP_SCR_W - margin * 2.0f;
    float step = total_w / (float)n;
    float w = step - 3.0f;
    float sr = shift_rpm_f();
    int lit = (int)(psp_clamp01(g_psp_rpm_ease / sr) * (float)n + 0.5f);
    bool flash = g_psp_rpm_ease >= sr;

    for (int i = 0; i < n; i++) {
        uint32_t col = i < 10 ? CLR_GREEN : (i < 17 ? CLR_YELLOW : CLR_RED);
        if (flash) col = (g_psp_hud_frame & 4u) ? CLR_CYAN : CLR_RED;
        else if (i >= lit) col = CLR_DARK_GRAY;
        if (flash && (g_psp_hud_frame & 2u)) col = CLR_DARK_GRAY;

        float bx = margin + (float)i * step;
        if (i < lit || flash) {
            uint32_t glow = (col & 0x00FFFFFFu) | 0x48000000u;
            ui_rect_solid(bx - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, glow);
        }
        ui_rect_solid(bx, y, w, h, col);
        if (i < lit && !flash) {
            ui_rect_solid(bx, y, w, 2.0f, PSP_RGBA(255, 255, 255, 130));
        }
    }
}

void psp_draw_dial_labels(float cx, float cy, float r, float start_deg, float sweep_deg,
                          int every, float max_val, float red_val, float scale) {
    for (int k = 0; (float)k <= max_val + 0.01f; k += every) {
        float frac = (float)k / max_val;
        float a = (start_deg + frac * sweep_deg) * (float)(M_PI / 180.0);
        float lx = cx + cosf(a) * r;
        float ly = cy + sinf(a) * r - 8.0f * scale;
        uint32_t col = (float)k >= red_val ? CLR_RED : CLR_WHITE;
        ui_textf(lx, ly, scale, col, UI_ALIGN_CENTER, "%d", k);
    }
}

void psp_render_multi_gauge(void) {
    psp_draw_header("MULTI");

    float red_rpm = shift_rpm_f();
    ui_panel(8.0f, 29.0f, 282.0f, 118.0f, CLR_PANEL_DARK, CLR_BORDER);
    ui_text(149.0f, 35.0f, 0.64f, CLR_CYAN, UI_ALIGN_CENTER, "VEHICLE SPEED");

    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)(psp_display_speed(g_dtec.speed_kmh) + 0.5f));
    else snprintf(spd, sizeof(spd), "---");
    ui_num(151.0f, 52.0f, 1.12f, PSP_RGBA(0, 0, 0, 120), UI_ALIGN_CENTER, spd);
    ui_num(149.0f, 50.0f, 1.12f, g_psp_rpm_ease >= red_rpm ? CLR_RED : CLR_WHITE, UI_ALIGN_CENTER, spd);

    if (g_dtec.has_speed) {
        unsigned mph = (unsigned)(g_dtec.speed_kmh * 0.621371f + 0.5f);
        if (g_psp.use_imperial) {
            ui_textf(149.0f, 127.0f, 0.64f, CLR_GRAY, UI_ALIGN_CENTER, "%u mph  •  %u km/h",
                     mph, (unsigned)g_dtec.speed_kmh);
        } else {
            ui_textf(149.0f, 127.0f, 0.64f, CLR_GRAY, UI_ALIGN_CENTER, "%u km/h  •  %u mph",
                     (unsigned)g_dtec.speed_kmh, mph);
        }
    } else {
        ui_text(149.0f, 127.0f, 0.64f, CLR_GRAY, UI_ALIGN_CENTER, "--- km/h  •  -- mph");
    }

    ui_panel(8.0f, 152.0f, 282.0f, 90.0f, CLR_PANEL, CLR_BORDER);
    ui_text(18.0f, 158.0f, 0.60f, CLR_GRAY, UI_ALIGN_LEFT, "ENGINE RPM");
    if (g_dtec.has_rpm) {
        ui_textf(280.0f, 157.0f, 0.70f, g_psp_rpm_ease >= red_rpm ? CLR_RED : CLR_WHITE,
                 UI_ALIGN_RIGHT, "%d RPM", (int)g_dtec.rpm);
    } else {
        ui_text(280.0f, 157.0f, 0.70f, CLR_GRAY, UI_ALIGN_RIGHT, "---- RPM");
    }

    const int segs = 42;
    int lit = g_dtec.has_rpm ? (int)(psp_clamp01(g_psp_rpm_ease / HUD_RPM_MAX) * (float)segs) : 0;
    for (int i = 0; i < segs; i++) {
        float f = (float)i / (float)segs;
        uint32_t col = psp_zone_color(f);
        float sx = 18.0f + (float)i * 6.28f;
        ui_rect_solid(sx, 180.0f, 4.6f, 20.0f, i < lit ? col : CLR_DARK_GRAY);
        if (i < lit) ui_rect_solid(sx, 180.0f, 4.6f, 2.0f, CLR_WHITE);
    }

    ui_textf(18.0f, 208.0f, 0.56f, CLR_YELLOW, UI_ALIGN_LEFT, "PEAK %d RPM", (int)g_dtec.peak_rpm);
    ui_textf(18.0f, 223.0f, 0.54f, CLR_GRAY, UI_ALIGN_LEFT, "SHIFT %u", (unsigned)g_psp.shift_rpm);
    ui_textf(280.0f, 214.0f, 0.58f, g_dtec.auto_poll ? CLR_GREEN : CLR_GRAY, UI_ALIGN_RIGHT,
             g_dtec.auto_poll ? "START: POLLING" : "START: PAUSED");

    psp_draw_side_dock(298.0f, 29.0f, 174.0f, 50.0f, 4.0f);
    psp_draw_footer("MULTI");
}

void psp_render_digital_speed(void) {
    psp_draw_shift_lights(6.0f, 12.0f);

    float red_rpm = shift_rpm_f();
    ui_panel(8.0f, 24.0f, 282.0f, 218.0f, CLR_PANEL_DARK, CLR_BORDER);

    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)(psp_display_speed(g_dtec.speed_kmh) + 0.5f));
    else snprintf(spd, sizeof(spd), "---");
    uint32_t spd_col = g_psp_rpm_ease >= red_rpm ? CLR_RED : CLR_WHITE;
    ui_num(20.0f, 36.0f, 1.32f, spd_col, UI_ALIGN_LEFT, spd);
    ui_text(204.0f, 82.0f, 0.86f, CLR_CYAN, UI_ALIGN_LEFT, psp_speed_unit(false));
    if (g_dtec.has_speed) {
        unsigned alt = g_psp.use_imperial ? (unsigned)g_dtec.speed_kmh
                                          : (unsigned)(g_dtec.speed_kmh * 0.621371f + 0.5f);
        ui_textf(204.0f, 102.0f, 0.60f, CLR_GRAY, UI_ALIGN_LEFT, "%u %s",
                 alt, g_psp.use_imperial ? "km/h" : "mph");
    }

    const int segs = 42;
    int lit = (int)(psp_clamp01(g_psp_rpm_ease / HUD_RPM_MAX) * (float)segs);
    for (int i = 0; i < segs; i++) {
        uint32_t col = psp_zone_color((float)i / (float)segs);
        ui_rect_solid(18.0f + (float)i * 6.28f, 146.0f, 4.6f, 14.0f, i < lit ? col : CLR_DARK_GRAY);
    }

    ui_text(18.0f, 172.0f, 0.58f, CLR_GRAY, UI_ALIGN_LEFT, "ENGINE RPM");
    if (g_dtec.has_rpm) ui_numf(18.0f, 192.0f, 0.66f, CLR_CYAN, UI_ALIGN_LEFT, "%d", (int)g_dtec.rpm);
    else ui_num(18.0f, 192.0f, 0.66f, CLR_DARK_GRAY, UI_ALIGN_LEFT, "----");

    ui_text(280.0f, 172.0f, 0.58f, CLR_GRAY, UI_ALIGN_RIGHT, "PEAK RPM");
    ui_textf(280.0f, 200.0f, 0.86f, CLR_YELLOW, UI_ALIGN_RIGHT, "%d", (int)g_dtec.peak_rpm);

    psp_draw_side_dock(298.0f, 24.0f, 174.0f, 51.0f, 4.0f);
    psp_draw_footer("DIGITAL");
}

void psp_render_big_speed(void) {
    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)(psp_display_speed(g_dtec.speed_kmh) + 0.5f));
    else snprintf(spd, sizeof(spd), "---");

    ui_num(240.0f, 28.0f, 1.82f, g_psp_rpm_ease >= shift_rpm_f() ? CLR_RED : CLR_WHITE, UI_ALIGN_CENTER, spd);

    if (g_dtec.has_speed) {
        unsigned mph = (unsigned)(g_dtec.speed_kmh * 0.621371f + 0.5f);
        if (g_psp.use_imperial) {
            ui_textf(240.0f, 168.0f, 0.90f, CLR_GRAY, UI_ALIGN_CENTER, "MPH   •   %u KM/H",
                     (unsigned)g_dtec.speed_kmh);
        } else {
            ui_textf(240.0f, 168.0f, 0.90f, CLR_GRAY, UI_ALIGN_CENTER, "KM/H   •   %u MPH", mph);
        }
    } else {
        ui_text(240.0f, 168.0f, 0.90f, CLR_GRAY, UI_ALIGN_CENTER, psp_speed_unit(true));
    }

    float f = psp_clamp01(g_psp_rpm_ease / HUD_RPM_MAX);
    uint32_t col = psp_zone_color(f);
    if (g_psp_rpm_ease >= shift_rpm_f() && (g_psp_hud_frame & 4u)) col = CLR_WHITE;
    ui_rect_solid(12.0f, 214.0f, 456.0f, 20.0f, CLR_DARK_GRAY);
    ui_rect_solid(12.0f, 214.0f, 456.0f * f, 20.0f, col);
    if (g_dtec.has_rpm) {
        ui_textf(240.0f, 194.0f, 0.66f, CLR_CYAN, UI_ALIGN_CENTER, "%d RPM", (int)g_dtec.rpm);
    }

    psp_draw_link_dot(466.0f, 14.0f);
    psp_draw_footer("BIG SPEED");
}

void psp_render_arc_tach(void) {
    const float cx = 148.0f, cy = 128.0f, r = 110.0f;
    ui_draw_dial(cx, cy, r);

    const int wedges = 36;
    float frac = psp_clamp01(g_psp_rpm_ease / HUD_RPM_MAX);
    for (int i = 0; i < wedges; i++) {
        float f0 = (float)i / (float)wedges;
        float f1 = ((float)i + 0.78f) / (float)wedges;
        uint32_t col = (f0 < frac) ? psp_zone_color(f0) : CLR_DARK_GRAY;
        ui_arc_band(cx, cy, 78.0f, 96.0f, 150.0f, 240.0f, f0, f1, col, 2);
    }

    psp_draw_dial_labels(cx, cy, 64.0f, 150.0f, 240.0f, 1,
                         HUD_RPM_MAX / 1000.0f, shift_rpm_f() / 1000.0f, 0.68f);

    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)(psp_display_speed(g_dtec.speed_kmh) + 0.5f));
    else snprintf(spd, sizeof(spd), "---");
    ui_num(cx, cy - 32.0f, 0.80f, CLR_WHITE, UI_ALIGN_CENTER, spd);
    ui_text(cx, cy + 28.0f, 0.64f, CLR_CYAN, UI_ALIGN_CENTER, psp_speed_unit(false));
    if (g_dtec.has_rpm) {
        ui_textf(cx, cy + 50.0f, 0.72f, g_psp_rpm_ease >= shift_rpm_f() ? CLR_RED : CLR_YELLOW,
                 UI_ALIGN_CENTER, "%d RPM", (int)g_dtec.rpm);
    } else {
        ui_text(cx, cy + 50.0f, 0.72f, CLR_YELLOW, UI_ALIGN_CENTER, "--- RPM");
    }

    psp_draw_side_dock(298.0f, 14.0f, 174.0f, 54.0f, 4.0f);
    psp_draw_link_dot(468.0f, 10.0f);
    psp_draw_footer("ARC TACH");
}

void psp_render_sim_dash(void) {
    psp_draw_shift_lights(6.0f, 11.0f);

    float red_rpm = shift_rpm_f();
    bool redline = g_psp_rpm_ease >= red_rpm;
    uint32_t box_border = (redline && (g_psp_hud_frame & 4u)) ? CLR_RED : CLR_ORANGE;

    ui_vbar(12.0f, 24.0f, 30.0f, 130.0f, g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f, CLR_GREEN, CLR_DARK_GRAY);
    ui_text(27.0f, 160.0f, 0.58f, CLR_GRAY, UI_ALIGN_CENTER, "THR");

    ui_panel(54.0f, 24.0f, 372.0f, 146.0f, CLR_LCD_BG, box_border);
    ui_num(240.0f, 36.0f, 1.24f, CLR_LCD_GHOST, UI_ALIGN_CENTER, "888");
    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)(psp_display_speed(g_dtec.speed_kmh) + 0.5f));
    else snprintf(spd, sizeof(spd), "---");
    ui_num(240.0f, 36.0f, 1.24f, CLR_LCD_ON, UI_ALIGN_CENTER, spd);

    if (g_dtec.has_speed) {
        unsigned alt = g_psp.use_imperial ? (unsigned)g_dtec.speed_kmh
                                          : (unsigned)(g_dtec.speed_kmh * 0.621371f + 0.5f);
        ui_textf(240.0f, 136.0f, 0.70f, CLR_LCD_ON, UI_ALIGN_CENTER, "%s   •   %u %s",
                 psp_speed_unit(true), alt, g_psp.use_imperial ? "KM/H" : "MPH");
    } else {
        ui_text(240.0f, 136.0f, 0.70f, CLR_LCD_ON, UI_ALIGN_CENTER, psp_speed_unit(true));
    }

    if (g_dtec.has_boost) {
        float bf = psp_boost_display_val(g_dtec.boost_kpa) / psp_boost_max_display();
        ui_vbar(438.0f, 24.0f, 30.0f, 130.0f, bf, CLR_CYAN, CLR_DARK_GRAY);
        ui_text(453.0f, 160.0f, 0.58f, CLR_GRAY, UI_ALIGN_CENTER, "BST");
    } else {
        ui_vbar(438.0f, 24.0f, 30.0f, 130.0f,
                g_dtec.has_load ? g_dtec.engine_load_pct / 100.0f : 0.0f, CLR_PURPLE, CLR_DARK_GRAY);
        ui_text(453.0f, 160.0f, 0.58f, CLR_GRAY, UI_ALIGN_CENTER, "LOAD");
    }

    const float cw = 110.0f;
    for (int i = 0; i < 4; i++) {
        ui_panel(10.0f + (float)i * 116.5f, 178.0f, cw, 64.0f, CLR_LCD_BG, CLR_BORDER);
    }
    ui_text(18.0f, 183.0f, 0.56f, CLR_GRAY, UI_ALIGN_LEFT, "RPM");
    if (g_dtec.has_rpm) {
        ui_numf(65.0f, 200.0f, 0.52f, redline ? CLR_RED : CLR_LCD_ON, UI_ALIGN_CENTER, "%d", (int)g_dtec.rpm);
    } else {
        ui_num(65.0f, 200.0f, 0.52f, CLR_LCD_GHOST, UI_ALIGN_CENTER, "----");
    }

    ui_text(134.5f, 183.0f, 0.56f, CLR_GRAY, UI_ALIGN_LEFT, "WATER");
    if (g_dtec.has_coolant) {
        char tb[16];
        psp_format_temp(g_dtec.coolant_c, tb, sizeof(tb));
        ui_text(181.5f, 206.0f, 0.96f, g_dtec.coolant_c > 104 ? CLR_RED : CLR_LCD_ON, UI_ALIGN_CENTER, tb);
    } else {
        ui_text(181.5f, 206.0f, 0.96f, CLR_LCD_GHOST, UI_ALIGN_CENTER, g_psp.use_imperial ? "-- °F" : "-- °C");
    }

    ui_text(251.0f, 183.0f, 0.56f, CLR_GRAY, UI_ALIGN_LEFT, "BATT");
    ui_text(298.0f, 206.0f, 0.88f, g_dtec.has_voltage ? CLR_LCD_ON : CLR_LCD_GHOST,
            UI_ALIGN_CENTER, g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V");

    ui_text(367.5f, 183.0f, 0.56f, CLR_GRAY, UI_ALIGN_LEFT, "PEAK RPM");
    ui_textf(414.5f, 206.0f, 0.92f, CLR_LCD_ON, UI_ALIGN_CENTER, "%d", (int)g_dtec.peak_rpm);

    psp_draw_link_dot(470.0f, 12.0f);
    psp_draw_footer("SIM LCD");
}

void psp_render_gt_style(void) {
    float red_rpm = shift_rpm_f();
    float frac = psp_clamp01(g_psp_rpm_ease / HUD_RPM_MAX);
    bool redline = g_psp_rpm_ease >= red_rpm;

    /* Gran Turismo PSP signature center-out rev strip */
    float half = psp_clamp01(g_psp_rpm_ease / red_rpm) * 220.0f;
    uint32_t strip = g_psp_rpm_ease > red_rpm * 0.85f ? CLR_RED : CLR_GT_BLUE;
    if (redline && (g_psp_hud_frame & 4u)) strip = CLR_WHITE;
    ui_rect_solid(20.0f, 5.0f, 440.0f, 7.0f, CLR_DARK_GRAY);
    ui_rect_solid(240.0f - half, 5.0f, half * 2.0f, 7.0f, strip);

    const float cx = 148.0f, cy = 132.0f, r = 108.0f;
    ui_draw_dial(cx, cy, r);
    ui_arc_band(cx, cy, 98.0f, 100.0f, 135.0f, 270.0f, 0.0f, 1.0f, CLR_GRAY, 72);
    ui_arc_band(cx, cy, 85.0f, 96.0f, 135.0f, 270.0f, red_rpm / HUD_RPM_MAX, 1.0f,
                PSP_RGBA(95, 16, 20, 255), 24);
    ui_arc_band(cx, cy, 85.0f, 96.0f, 135.0f, 270.0f, 0.0f, frac, redline ? CLR_RED : CLR_WHITE, 72);

    for (int k = 0; (float)k * 1000.0f <= HUD_RPM_MAX; k++) {
        ui_radial_tick(cx, cy, 76.0f, 84.0f, 135.0f + ((float)k * 1000.0f / HUD_RPM_MAX) * 270.0f,
                       2.0f, CLR_WHITE);
    }
    psp_draw_dial_labels(cx, cy, 64.0f, 135.0f, 270.0f, 1,
                         HUD_RPM_MAX / 1000.0f, red_rpm / 1000.0f, 0.66f);
    ui_text(cx, cy - 42.0f, 0.54f, CLR_GRAY, UI_ALIGN_CENTER, "x1000 r/min");

    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)(psp_display_speed(g_dtec.speed_kmh) + 0.5f));
    else snprintf(spd, sizeof(spd), "---");
    ui_num(cx, cy - 28.0f, 0.78f, CLR_WHITE, UI_ALIGN_CENTER, spd);
    ui_text(cx, cy + 32.0f, 0.60f, CLR_GRAY, UI_ALIGN_CENTER, psp_speed_unit(false));
    if (g_dtec.has_rpm) {
        ui_textf(cx, cy + 56.0f, 0.70f, CLR_GT_BLUE, UI_ALIGN_CENTER, "%d rpm", (int)g_dtec.rpm);
    } else {
        ui_text(cx, cy + 56.0f, 0.70f, CLR_GT_BLUE, UI_ALIGN_CENTER, "--- rpm");
    }

    psp_draw_side_dock(298.0f, 20.0f, 174.0f, 52.0f, 4.0f);
    psp_draw_link_dot(468.0f, 10.0f);
    psp_draw_footer("GT STYLE");
}

void psp_render_forza_style(void) {
    const float cx = 148.0f, cy = 128.0f, r = 110.0f;
    float red_rpm = shift_rpm_f();
    float frac = psp_clamp01(g_psp_rpm_ease / HUD_RPM_MAX);
    float red_f = red_rpm / HUD_RPM_MAX;

    ui_draw_dial(cx, cy, r);
    ui_arc_band(cx, cy, r - 12.0f, r - 6.0f, 135.0f, 270.0f, red_f, 1.0f,
                PSP_RGBA(110, 18, 18, 255), 24);
    ui_arc_band(cx, cy, r - 12.0f, r - 6.0f, 135.0f, 270.0f, 0.0f, frac < red_f ? frac : red_f,
                CLR_WHITE, 72);
    if (frac > red_f) {
        ui_arc_band(cx, cy, r - 12.0f, r - 6.0f, 135.0f, 270.0f, red_f, frac, CLR_RED, 24);
    }

    for (int k = 0; (float)k * 500.0f <= HUD_RPM_MAX; k++) {
        float deg = 135.0f + ((float)k * 500.0f / HUD_RPM_MAX) * 270.0f;
        uint32_t col = (float)k * 500.0f >= red_rpm ? CLR_RED : CLR_WHITE;
        ui_radial_tick(cx, cy, (k & 1) ? r - 20.0f : r - 26.0f, r - 14.0f, deg,
                       (k & 1) ? 1.3f : 2.2f, col);
    }
    psp_draw_dial_labels(cx, cy, r - 37.0f, 135.0f, 270.0f, 1,
                         HUD_RPM_MAX / 1000.0f, red_rpm / 1000.0f, 0.68f);

    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)(psp_display_speed(g_dtec.speed_kmh) + 0.5f));
    else snprintf(spd, sizeof(spd), "---");
    ui_num(cx, cy + 20.0f, 0.66f, CLR_WHITE, UI_ALIGN_CENTER, spd);
    ui_text(cx, cy + 72.0f, 0.58f, CLR_GRAY, UI_ALIGN_CENTER, psp_speed_unit(true));

    ui_needle(cx, cy, 18.0f, r - 15.0f, 135.0f + frac * 270.0f, 3.4f, CLR_ORANGE);

    psp_draw_side_dock(298.0f, 14.0f, 174.0f, 54.0f, 4.0f);
    psp_draw_link_dot(468.0f, 10.0f);
    psp_draw_footer("FORZA STYLE");
}

static void bng_panel(float x, float y, float w, float h, const char *title) {
    ui_rect_solid(x, y, w, h, PSP_RGBA(18, 22, 30, 245));
    ui_rect_solid(x, y, w, 3.0f, CLR_ORANGE);
    ui_text(x + 8.0f, y + 7.0f, 0.58f, CLR_GRAY, UI_ALIGN_LEFT, title);
}

void psp_render_beamng_style(void) {
    bng_panel(8.0f, 8.0f, 232.0f, 234.0f, "TACHO");
    const float cx = 124.0f, cy = 128.0f;
    float frac = psp_clamp01(g_psp_rpm_ease / HUD_RPM_MAX);

    ui_arc_band(cx, cy, 74.0f, 90.0f, 135.0f, 270.0f, 0.0f, 1.0f, CLR_DARK_GRAY, 72);
    if (frac > 0.0f) {
        float f_w = frac < 0.60f ? frac : 0.60f;
        ui_arc_band(cx, cy, 74.0f, 90.0f, 135.0f, 270.0f, 0.0f, f_w, CLR_WHITE, 44);
        if (frac > 0.60f) {
            float f_o = frac < 0.82f ? frac : 0.82f;
            ui_arc_band(cx, cy, 74.0f, 90.0f, 135.0f, 270.0f, 0.60f, f_o, CLR_ORANGE, 20);
        }
        if (frac > 0.82f) {
            ui_arc_band(cx, cy, 74.0f, 90.0f, 135.0f, 270.0f, 0.82f, frac, CLR_RED, 16);
        }
    }
    psp_draw_dial_labels(cx, cy, 60.0f, 135.0f, 270.0f, 2,
                         HUD_RPM_MAX / 1000.0f, shift_rpm_f() / 1000.0f, 0.64f);

    char spd[12];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)(psp_display_speed(g_dtec.speed_kmh) + 0.5f));
    else snprintf(spd, sizeof(spd), "---");
    ui_num(cx, cy - 30.0f, 0.74f, CLR_WHITE, UI_ALIGN_CENTER, spd);
    ui_text(cx, cy + 28.0f, 0.58f, CLR_GRAY, UI_ALIGN_CENTER, psp_speed_unit(false));
    if (g_dtec.has_rpm) ui_textf(cx, 212.0f, 0.72f, CLR_ORANGE, UI_ALIGN_CENTER, "%d rpm", (int)g_dtec.rpm);
    else ui_text(cx, 212.0f, 0.72f, CLR_ORANGE, UI_ALIGN_CENTER, "--- rpm");

    bng_panel(248.0f, 8.0f, 224.0f, 102.0f, "ENGINE");
    char tb[24];
    if (g_dtec.has_coolant) {
        psp_format_temp(g_dtec.coolant_c, tb, sizeof(tb));
        ui_textf(258.0f, 30.0f, 0.72f, CLR_WHITE, UI_ALIGN_LEFT, "Temp   %s", tb);
    } else {
        ui_text(258.0f, 30.0f, 0.72f, CLR_GRAY, UI_ALIGN_LEFT, "Temp   --");
    }
    ui_textf(258.0f, 50.0f, 0.72f, g_dtec.has_voltage ? CLR_WHITE : CLR_GRAY, UI_ALIGN_LEFT,
             "Batt   %s", g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V");
    if (g_dtec.has_boost) {
        psp_format_boost(g_dtec.boost_kpa, tb, sizeof(tb));
        ui_textf(258.0f, 70.0f, 0.72f, CLR_CYAN, UI_ALIGN_LEFT, "Boost  %s", tb);
    } else if (g_dtec.has_load) {
        ui_textf(258.0f, 70.0f, 0.72f, CLR_WHITE, UI_ALIGN_LEFT, "Load   %d%%", (int)g_dtec.engine_load_pct);
    } else {
        ui_text(258.0f, 70.0f, 0.72f, CLR_GRAY, UI_ALIGN_LEFT, "Load   --%%");
    }
    if (g_dtec.has_map) ui_textf(258.0f, 90.0f, 0.66f, CLR_GRAY, UI_ALIGN_LEFT, "MAP    %d kPa", g_dtec.map_kpa);
    else ui_text(258.0f, 90.0f, 0.66f, CLR_GRAY, UI_ALIGN_LEFT, "MAP    -- kPa");

    bng_panel(248.0f, 116.0f, 224.0f, 126.0f, "INPUTS");
    ui_vbar(266.0f, 140.0f, 30.0f, 76.0f, g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f, CLR_GREEN, CLR_DARK_GRAY);
    ui_text(281.0f, 222.0f, 0.56f, CLR_GRAY, UI_ALIGN_CENTER, "THR");

    ui_vbar(318.0f, 140.0f, 30.0f, 76.0f, g_dtec.has_load ? g_dtec.engine_load_pct / 100.0f : 0.0f,
            CLR_GT_BLUE, CLR_DARK_GRAY);
    ui_text(333.0f, 222.0f, 0.56f, CLR_GRAY, UI_ALIGN_CENTER, "LOD");

    float bf = g_dtec.has_boost ? psp_boost_display_val(g_dtec.boost_kpa) / psp_boost_max_display() : 0.0f;
    ui_vbar(370.0f, 140.0f, 30.0f, 76.0f, bf, CLR_CYAN, CLR_DARK_GRAY);
    ui_text(385.0f, 222.0f, 0.56f, CLR_GRAY, UI_ALIGN_CENTER, "BST");

    float cf = g_dtec.has_coolant ? psp_clamp01(((float)g_dtec.coolant_c - 40.0f) / 80.0f) : 0.0f;
    ui_vbar(422.0f, 140.0f, 30.0f, 76.0f, cf, CLR_ORANGE, CLR_DARK_GRAY);
    ui_text(437.0f, 222.0f, 0.56f, CLR_GRAY, UI_ALIGN_CENTER, "TMP");

    psp_draw_link_dot(466.0f, 14.0f);
    psp_draw_footer("BEAMNG STYLE");
}

static void draw_analog_dial(float cx, float cy, float r, const char *title,
                             float val, bool has_val, float min_v, float max_v, float red_v,
                             int step_label, const char *unit, uint32_t accent, float peak_v) {
    ui_draw_dial(cx, cy, r);

    const float start_deg = 135.0f;
    const float sweep_deg = 270.0f;

    if (red_v > min_v && red_v < max_v) {
        ui_arc_band(cx, cy, r - 14.0f, r - 8.0f, start_deg, sweep_deg,
                    (red_v - min_v) / (max_v - min_v), 1.0f, PSP_RGBA(115, 20, 20, 255), 18);
    }

    for (int t = 0; t <= 16; t++) {
        float f = (float)t / 16.0f;
        float deg = start_deg + f * sweep_deg;
        float v = min_v + f * (max_v - min_v);
        uint32_t col = (red_v > min_v && v >= red_v) ? CLR_RED : ((t & 1) ? CLR_GRAY : CLR_WHITE);
        ui_radial_tick(cx, cy, (t & 1) ? r - 14.0f : r - 19.0f, r - 8.0f, deg,
                       (t & 1) ? 1.2f : 2.0f, col);
    }

    if (step_label > 0) {
        psp_draw_dial_labels(cx, cy, r - 29.0f, start_deg, sweep_deg, step_label, max_v,
                             red_v > 0.0f ? red_v : max_v + 1.0f, 0.54f);
    }

    if (peak_v > min_v) {
        float p_norm = psp_clamp01((peak_v - min_v) / (max_v - min_v));
        float pa = (start_deg + p_norm * sweep_deg) * (float)(M_PI / 180.0);
        ui_circle(cx + cosf(pa) * (r - 11.0f), cy + sinf(pa) * (r - 11.0f), 3.0f, CLR_YELLOW, 12);
    }

    ui_text(cx, cy - 30.0f, 0.60f, accent, UI_ALIGN_CENTER, title);

    char val_str[16];
    if (has_val) {
        if (fabsf(val) < 10.0f && max_v <= 20.0f) snprintf(val_str, sizeof(val_str), "%.1f", val);
        else snprintf(val_str, sizeof(val_str), "%d", (int)(val + 0.5f));
    } else {
        snprintf(val_str, sizeof(val_str), "--");
    }
    ui_text(cx, cy + 20.0f, 0.84f, CLR_WHITE, UI_ALIGN_CENTER, val_str);
    ui_text(cx, cy + 40.0f, 0.54f, CLR_GRAY, UI_ALIGN_CENTER, unit);

    float norm = has_val ? psp_clamp01((val - min_v) / (max_v - min_v)) : 0.0f;
    ui_needle(cx, cy, 16.0f, r - 12.0f, start_deg + norm * sweep_deg, 3.0f, CLR_RED);
}

void psp_render_analog_dials(void) {
    psp_draw_header("ANALOG CLUSTER");

    /* PSP Widescreen Signature: Twin 184px dials flanking a central MFD telemetry strip */
    draw_analog_dial(102.0f, 136.0f, 92.0f, "TACH", g_dtec.rpm / 1000.0f, g_dtec.has_rpm,
                     0.0f, 9.0f, shift_rpm_f() / 1000.0f, 1, "x1000 RPM",
                     CLR_YELLOW, g_dtec.peak_rpm / 1000.0f);

    /* Center MFD Stack (x=202..278, 76px wide) */
    char s[20];
    float y = 30.0f;
    if (g_dtec.has_coolant) psp_format_temp(g_dtec.coolant_c, s, sizeof(s));
    else snprintf(s, sizeof(s), "--");
    psp_draw_metric_card(202.0f, y, 76.0f, 50.0f, "TEMP", s,
                         (g_dtec.has_coolant && g_dtec.coolant_c > 104) ? CLR_RED : CLR_WHITE,
                         true, g_dtec.has_coolant ? psp_clamp01(((float)g_dtec.coolant_c - 40.0f) / 80.0f) : 0.0f,
                         CLR_ORANGE);
    y += 54.0f;

    if (g_dtec.has_tps) snprintf(s, sizeof(s), "%d%%", (int)g_dtec.tps_pct);
    else snprintf(s, sizeof(s), "--%%");
    psp_draw_metric_card(202.0f, y, 76.0f, 50.0f, "TPS", s, CLR_GREEN,
                         true, g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f, CLR_GREEN);
    y += 54.0f;

    if (g_dtec.has_boost) {
        psp_format_boost(g_dtec.boost_kpa, s, sizeof(s));
        psp_draw_metric_card(202.0f, y, 76.0f, 50.0f, "BOOST", s, CLR_CYAN,
                             true, psp_boost_display_val(g_dtec.boost_kpa) / psp_boost_max_display(), CLR_CYAN);
    } else {
        if (g_dtec.has_load) snprintf(s, sizeof(s), "%d%%", (int)g_dtec.engine_load_pct);
        else snprintf(s, sizeof(s), "--%%");
        psp_draw_metric_card(202.0f, y, 76.0f, 50.0f, "LOAD", s, CLR_PURPLE,
                             true, g_dtec.has_load ? g_dtec.engine_load_pct / 100.0f : 0.0f, CLR_PURPLE);
    }
    y += 54.0f;

    psp_draw_metric_card(202.0f, y, 76.0f, 50.0f, "VOLTS",
                         g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V",
                         g_dtec.has_voltage ? CLR_YELLOW : CLR_GRAY, false, 0.0f, CLR_YELLOW);

    float max_spd = g_psp.use_imperial ? 160.0f : 240.0f;
    int spd_step = g_psp.use_imperial ? 20 : 40;
    draw_analog_dial(378.0f, 136.0f, 92.0f, "SPEED",
                     psp_display_speed(g_dtec.speed_kmh), g_dtec.has_speed,
                     0.0f, max_spd, 999.0f, spd_step, psp_speed_unit(false),
                     CLR_CYAN, psp_display_speed(g_psp.peak_speed_kmh));

    psp_draw_footer("ANALOG");
}

void psp_render_realtime_graph(int scope_cursor, bool scope_scrubbing) {
    psp_draw_header("TELEMETRY SCOPE");

    const float gx = 8.0f, gy = 29.0f, gw = 464.0f, gh = 166.0f;
    ui_panel(gx, gy, gw, gh, CLR_BLACK, CLR_BORDER);

    for (int r = 1; r < 5; r++) {
        float ly = gy + gh * (float)r / 5.0f;
        ui_rect_solid(gx + 2.0f, ly, gw - 4.0f, 1.0f, PSP_RGBA(22, 34, 54, 190));
    }
    for (int c = 1; c < 10; c++) {
        float lx = gx + gw * (float)c / 10.0f;
        ui_rect_solid(lx, gy + 2.0f, 1.0f, gh - 4.0f, PSP_RGBA(22, 34, 54, 150));
    }

    int count = g_dtec.hist_count;
    float step_x = (gw - 12.0f) / (float)(GRAPH_HISTORY_LEN - 1);
    if (count > 1) {
        for (int i = 0; i < count - 1; i++) {
            float x1 = gx + 6.0f + (float)i * step_x;
            float x2 = gx + 6.0f + (float)(i + 1) * step_x;

            if (g_dtec.has_boost) {
                float b1 = psp_clamp01((g_dtec.hist_boost[i] + 100.0f) / 300.0f);
                float b2 = psp_clamp01((g_dtec.hist_boost[i + 1] + 100.0f) / 300.0f);
                ui_line_thick(x1, gy + gh - 8.0f - b1 * (gh - 16.0f),
                              x2, gy + gh - 8.0f - b2 * (gh - 16.0f), 2.0f, CLR_CYAN);
            }

            float r1 = psp_clamp01(g_dtec.hist_rpm[i] / HUD_RPM_MAX);
            float r2 = psp_clamp01(g_dtec.hist_rpm[i + 1] / HUD_RPM_MAX);
            ui_line_thick(x1, gy + gh - 8.0f - r1 * (gh - 16.0f),
                          x2, gy + gh - 8.0f - r2 * (gh - 16.0f), 2.2f, CLR_YELLOW);

            float t1 = psp_clamp01(g_dtec.hist_tps[i] / 100.0f);
            float t2 = psp_clamp01(g_dtec.hist_tps[i + 1] / 100.0f);
            ui_line_thick(x1, gy + gh - 8.0f - t1 * (gh - 16.0f),
                          x2, gy + gh - 8.0f - t2 * (gh - 16.0f), 2.0f, CLR_GREEN);
        }
    }

    int inspect_idx = (scope_scrubbing && count > 0) ? scope_cursor : (count - 1);
    if (inspect_idx < 0) inspect_idx = 0;
    if (inspect_idx >= count && count > 0) inspect_idx = count - 1;

    if (scope_scrubbing && count > 0) {
        float cx = gx + 6.0f + (float)inspect_idx * step_x;
        ui_rect_solid(cx, gy + 2.0f, 1.5f, gh - 4.0f, PSP_RGBA(255, 255, 255, 190));

        float ry = gy + gh - 8.0f - psp_clamp01(g_dtec.hist_rpm[inspect_idx] / HUD_RPM_MAX) * (gh - 16.0f);
        float ty = gy + gh - 8.0f - psp_clamp01(g_dtec.hist_tps[inspect_idx] / 100.0f) * (gh - 16.0f);
        ui_circle(cx, ry, 4.0f, CLR_YELLOW, 12);
        ui_circle(cx, ty, 4.0f, CLR_GREEN, 12);
        if (g_dtec.has_boost) {
            float by = gy + gh - 8.0f - psp_clamp01((g_dtec.hist_boost[inspect_idx] + 100.0f) / 300.0f) * (gh - 16.0f);
            ui_circle(cx, by, 4.0f, CLR_CYAN, 12);
        }

        ui_panel(gx + 8.0f, gy + 6.0f, 212.0f, 20.0f, CLR_PANEL_HI, CLR_CYAN);
        ui_textf(gx + 16.0f, gy + 10.0f, 0.56f, CLR_WHITE, UI_ALIGN_LEFT,
                 "CURSOR #%d/%d  ([^] LIVE)", inspect_idx + 1, count);
    } else {
        ui_text(gx + 10.0f, gy + 6.0f, 0.54f, CLR_GRAY, UI_ALIGN_LEFT,
                "MOVE ANALOG STICK TO INSPECT TRACE");
    }

    float show_rpm = count > 0 ? g_dtec.hist_rpm[inspect_idx] : (g_dtec.has_rpm ? g_dtec.rpm : 0.0f);
    float show_tps = count > 0 ? g_dtec.hist_tps[inspect_idx] : (g_dtec.has_tps ? g_dtec.tps_pct : 0.0f);
    float show_bst = count > 0 ? g_dtec.hist_boost[inspect_idx] : g_dtec.boost_kpa;

    ui_panel(8.0f, 201.0f, 464.0f, 41.0f, CLR_PANEL, CLR_BORDER);
    ui_circle(24.0f, 221.0f, 4.5f, CLR_YELLOW, 12);
    ui_textf(36.0f, 214.0f, 0.72f, CLR_WHITE, UI_ALIGN_LEFT, "RPM %d", (int)show_rpm);

    ui_circle(162.0f, 221.0f, 4.5f, CLR_GREEN, 12);
    ui_textf(174.0f, 214.0f, 0.72f, CLR_WHITE, UI_ALIGN_LEFT, "TPS %d%%", (int)show_tps);

    if (g_dtec.has_boost) {
        char bb[20];
        psp_format_boost(show_bst, bb, sizeof(bb));
        ui_circle(288.0f, 221.0f, 4.5f, CLR_CYAN, 12);
        ui_textf(300.0f, 214.0f, 0.72f, CLR_CYAN, UI_ALIGN_LEFT, "BST %s", bb);
    } else {
        ui_textf(462.0f, 214.0f, 0.68f, CLR_GRAY, UI_ALIGN_RIGHT, "PEAK %d RPM", (int)g_dtec.peak_rpm);
    }

    psp_draw_footer("SCOPE");
}

void psp_render_stats(void) {
    psp_draw_header("SESSION STATS");

    struct {
        const char *label;
        char val[24];
        uint32_t col;
        bool bar;
        float frac;
    } cards[8];

    cards[0].label = "PEAK RPM";
    if (g_dtec.peak_rpm > 0.5f) snprintf(cards[0].val, 24, "%d RPM", (int)g_dtec.peak_rpm);
    else snprintf(cards[0].val, 24, "-- RPM");
    cards[0].col = CLR_YELLOW;
    cards[0].bar = true;
    cards[0].frac = g_dtec.peak_rpm / HUD_RPM_MAX;

    cards[1].label = "PEAK SPEED";
    if (g_psp.peak_speed_kmh > 0) {
        snprintf(cards[1].val, 24, "%u %s",
                 (unsigned)(psp_display_speed(g_psp.peak_speed_kmh) + 0.5f),
                 psp_speed_unit(false));
    } else {
        snprintf(cards[1].val, 24, "-- %s", psp_speed_unit(false));
    }
    cards[1].col = CLR_CYAN;
    cards[1].bar = true;
    cards[1].frac = (float)g_psp.peak_speed_kmh / 240.0f;

    if (g_dtec.has_boost) {
        cards[2].label = "PEAK BOOST";
        psp_format_boost(g_dtec.peak_boost, cards[2].val, 24);
        cards[2].col = CLR_CYAN;
        cards[2].bar = true;
        cards[2].frac = psp_boost_display_val(g_dtec.peak_boost) / psp_boost_max_display();
    } else {
        cards[2].label = "PEAK COOLANT";
        if (g_psp.peak_coolant_c > -900) psp_format_temp(g_psp.peak_coolant_c, cards[2].val, 24);
        else snprintf(cards[2].val, 24, "--");
        cards[2].col = (g_psp.peak_coolant_c > 104) ? CLR_RED : CLR_ORANGE;
        cards[2].bar = true;
        cards[2].frac = g_psp.peak_coolant_c > -900 ? psp_clamp01(((float)g_psp.peak_coolant_c - 40.0f) / 80.0f) : 0.0f;
    }

    cards[3].label = "MANIFOLD MAP";
    if (g_dtec.has_map) snprintf(cards[3].val, 24, "%d kPa", g_dtec.map_kpa);
    else snprintf(cards[3].val, 24, "-- kPa");
    cards[3].col = CLR_CYAN;
    cards[3].bar = true;
    cards[3].frac = g_dtec.has_map ? (float)g_dtec.map_kpa / 250.0f : 0.0f;

    cards[4].label = "THROTTLE";
    if (g_dtec.has_tps) snprintf(cards[4].val, 24, "%d%%", (int)g_dtec.tps_pct);
    else snprintf(cards[4].val, 24, "--%%");
    cards[4].col = CLR_GREEN;
    cards[4].bar = true;
    cards[4].frac = g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f;

    cards[5].label = "BATTERY";
    snprintf(cards[5].val, 24, "%s", g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V");
    cards[5].col = CLR_WHITE;
    cards[5].bar = false;
    cards[5].frac = 0.0f;

    cards[6].label = "RX / TX LINES";
    snprintf(cards[6].val, 24, "%lu / %lu", (unsigned long)g_dtec.lines_rx, (unsigned long)g_dtec.cmds_sent);
    cards[6].col = CLR_WHITE;
    cards[6].bar = false;
    cards[6].frac = 0.0f;

    cards[7].label = "AUTO-POLL";
    snprintf(cards[7].val, 24, "%s", g_dtec.auto_poll ? "ACTIVE" : "PAUSED");
    cards[7].col = g_dtec.auto_poll ? CLR_GREEN : CLR_GRAY;
    cards[7].bar = false;
    cards[7].frac = 0.0f;

    for (int i = 0; i < 8; i++) {
        float cx = 8.0f + (float)(i % 4) * 117.0f;
        float cy = 30.0f + (float)(i / 4) * 107.0f;
        psp_draw_metric_card(cx, cy, 113.0f, 99.0f, cards[i].label, cards[i].val,
                             cards[i].col, cards[i].bar, cards[i].frac, cards[i].col);
    }

    psp_draw_footer("STATS");
}
