#include <stdio.h>
#include <string.h>
#include <math.h>

#include "dtec_common.h"
#include "dtec_gfx.h"
#include "dtec_obd.h"

static void draw_header(uint16_t *buf, const char *mode_name) {
    // Header Bar (256 x 16)
    gfx_rect_fill(buf, 0, 0, SCREEN_W, 16, DTEC_COLOR_PANEL);
    gfx_line(buf, 0, 16, SCREEN_W - 1, 16, DTEC_COLOR_BORDER);

    gfx_draw_str_clipped(buf, 4, 4, 96, "DSOBD", DTEC_COLOR_CYAN, 1);
    gfx_draw_str_clipped(buf, 106, 4, 60, mode_name, DTEC_COLOR_GRAY, 1);
    if (g_dtec.logging) {
        gfx_circle_fill(buf, 55, 7, 3, DTEC_COLOR_RED);
        gfx_draw_str(buf, 62, 4, "REC", DTEC_COLOR_RED, 1);
    }

    // Status Badge (strictly fits within x=175 to 252)
    if (g_dtec.net_status == NET_OBD_CONNECTED) {
        gfx_panel(buf, 175, 2, 77, 12, DTEC_RGB(1, 10, 7), DTEC_COLOR_GREEN);
        gfx_draw_str(buf, 196, 4, "LIVE", DTEC_COLOR_GREEN, 1);
    } else if (g_dtec.net_status == NET_TCP_CONNECTING || g_dtec.net_status == NET_WIFI_CONNECTING) {
        gfx_panel(buf, 175, 2, 77, 12, DTEC_RGB(9, 7, 1), DTEC_COLOR_YELLOW);
        gfx_draw_str(buf, 184, 4, "LINKING", DTEC_COLOR_YELLOW, 1);
    } else {
        gfx_panel(buf, 175, 2, 77, 12, DTEC_RGB(8, 2, 3), DTEC_COLOR_RED);
        gfx_draw_str(buf, 184, 4, "NO LINK", DTEC_COLOR_WHITE, 1);
    }
}

static void render_multi_gauge(uint16_t *buf) {
    draw_header(buf, "MULTI");
    gfx_draw_str(buf, 76, 24, "VEHICLE SPEED", DTEC_COLOR_CYAN, 1);
    gfx_seg_number(buf, 61, 36, 36, 51, 4, 3,
        g_dtec.has_speed ? (int)g_dtec.speed_kmh : -1, DTEC_COLOR_WHITE, DTEC_COLOR_BG, 0);
    char speed[20];
    if (g_dtec.has_speed) snprintf(speed, sizeof(speed), "%u km/h", (unsigned)g_dtec.speed_kmh);
    else snprintf(speed, sizeof(speed), "--- km/h");
    gfx_draw_str(buf, 128 - (int)strlen(speed) * 4, 92, speed, DTEC_COLOR_GRAY, 1);

    gfx_panel(buf, 4, 108, 248, 32, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);
    gfx_draw_str(buf, 10, 113, "RPM", DTEC_COLOR_GRAY, 1);
    char rpm[16];
    if (g_dtec.has_rpm) snprintf(rpm, sizeof(rpm), "%d", (int)g_dtec.rpm);
    else snprintf(rpm, sizeof(rpm), "----");
    gfx_draw_str(buf, 210, 113, rpm, DTEC_COLOR_WHITE, 1);
    int lit = g_dtec.has_rpm ? (int)(g_dtec.rpm * 38 / 8500.0f) : 0;
    for (int i = 0; i < 38; i++) {
        uint16_t col = i < 27 ? DTEC_COLOR_CYAN : (i < 32 ? DTEC_COLOR_YELLOW : DTEC_COLOR_RED);
        int x = 10 + i * 6;
        gfx_rect_fill(buf, x, 126, 4, 8, i < lit ? col : DTEC_COLOR_DARK_GRAY);
        if (i < lit) gfx_line(buf, x, 126, x + 3, 126, DTEC_COLOR_WHITE);
    }
    char values[4][16];
    const char *labels[] = {"TEMP C", "TPS", "VOLTS", g_dtec.has_boost ? "BOOST" : "LOAD"};
    if (g_dtec.has_coolant) snprintf(values[0], 16, "%d C", g_dtec.coolant_c);
    else snprintf(values[0], 16, "-- C");
    if (g_dtec.has_tps) snprintf(values[1], 16, "%d%%", (int)g_dtec.tps_pct);
    else snprintf(values[1], 16, "--%%");
    snprintf(values[2], 16, "%.6s", g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V");
    if (g_dtec.has_boost) snprintf(values[3], 16, "%+.0fkPa", g_dtec.boost_kpa);
    else if (g_dtec.has_load) snprintf(values[3], 16, "%d%%", (int)g_dtec.engine_load_pct);
    else snprintf(values[3], 16, "--%%");
    for (int i = 0; i < 4; i++) {
        int x = 4 + i * 63;
        gfx_panel(buf, x, 145, 59, 34, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);
        gfx_draw_str_clipped(buf, x + 3, 150, 53, labels[i], DTEC_COLOR_GRAY, 1);
        gfx_draw_str_clipped(buf, x + 1, 163, 57, values[i],
            i == 0 && g_dtec.has_coolant && g_dtec.coolant_c > 104 ? DTEC_COLOR_RED : DTEC_COLOR_WHITE, 1);
    }
    gfx_draw_str(buf, 8, 184, "SELECT / VIEW", DTEC_COLOR_GRAY, 1);
    gfx_draw_str(buf, 144, 184, "START / POLL", DTEC_COLOR_GRAY, 1);
}

static void draw_analog_dial(uint16_t *buf, int cx, int cy, int radius, const char *title,
                             float val, bool has_val, float min_v, float max_v, const char *unit,
                             uint16_t color, float peak_v) {
    gfx_dial(buf, cx, cy, radius);

    const float start_a = 135.0f * (M_PI / 180.0f);
    const float end_a   = 405.0f * (M_PI / 180.0f);
    const float sweep   = end_a - start_a;

    for (int t = 0; t <= 8; t++) {
        float a = start_a + (t / 8.0f) * sweep;
        int x1 = cx + (int)(cosf(a) * (radius - 2));
        int y1 = cy + (int)(sinf(a) * (radius - 2));
        int x2 = cx + (int)(cosf(a) * (radius - 7));
        int y2 = cy + (int)(sinf(a) * (radius - 7));
        gfx_line(buf, x1, y1, x2, y2, DTEC_COLOR_GRAY);
    }

    float norm = 0.0f;
    if (has_val) {
        norm = (val - min_v) / (max_v - min_v);
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
    }
    float needle_a = start_a + norm * sweep;

    if (peak_v > min_v) {
        float p_norm = (peak_v - min_v) / (max_v - min_v);
        if (p_norm < 0.0f) p_norm = 0.0f;
        if (p_norm > 1.0f) p_norm = 1.0f;
        float peak_a = start_a + p_norm * sweep;
        int px = cx + (int)(cosf(peak_a) * (radius - 5));
        int py = cy + (int)(sinf(peak_a) * (radius - 5));
        gfx_rect_fill(buf, px - 1, py - 1, 3, 3, DTEC_COLOR_WHITE);
    }

    int nx = cx + (int)(cosf(needle_a) * (radius - 9));
    int ny = cy + (int)(sinf(needle_a) * (radius - 9));
    gfx_line(buf, cx, cy, nx, ny, DTEC_COLOR_RED);

    gfx_circle_fill(buf, cx, cy, 4, DTEC_COLOR_WHITE);

    gfx_draw_str_clipped(buf, cx - (int)(strlen(title) * 4), cy - 18, radius * 2, title, color, 1);

    char val_str[16];
    if (has_val) {
        if (fabsf(val) < 10.0f) snprintf(val_str, 16, "%.1f", val);
        else snprintf(val_str, 16, "%d", (int)val);
    } else {
        snprintf(val_str, 16, "--");
    }
    gfx_draw_str_clipped(buf, cx - (int)(strlen(val_str) * 4), cy + 14, radius * 2, val_str, DTEC_COLOR_WHITE, 1);
    gfx_draw_str_clipped(buf, cx - (int)(strlen(unit) * 4), cy + 24, radius * 2, unit, DTEC_COLOR_GRAY, 1);
}

static void render_analog_dials(uint16_t *buf) {
    draw_header(buf, "ANALOG");

    draw_analog_dial(buf, 64, 88, 52, "TACH", g_dtec.rpm / 1000.0f, g_dtec.has_rpm, 0.0f, 9.0f, "x1000 RPM", DTEC_COLOR_YELLOW, g_dtec.peak_rpm / 1000.0f);
    draw_analog_dial(buf, 192, 88, 52, "SPEED", g_dtec.speed_kmh, g_dtec.has_speed, 0.0f, 240.0f, "km/h", DTEC_COLOR_CYAN, 0.0f);

    gfx_panel(buf, 4, 148, 248, 16, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);

    char throttle[12], coolant[12], info_str[36];
    if (g_dtec.has_tps) snprintf(throttle, sizeof(throttle), "%d%%", (int)g_dtec.tps_pct);
    else snprintf(throttle, sizeof(throttle), "--%%");
    if (g_dtec.has_coolant) snprintf(coolant, sizeof(coolant), "%dC", g_dtec.coolant_c);
    else snprintf(coolant, sizeof(coolant), "--C");
    snprintf(info_str, sizeof(info_str), "TPS:%s COOL:%s", throttle, coolant);
    gfx_draw_str_clipped(buf, 8, 152, 240, info_str, DTEC_COLOR_WHITE, 1);

    gfx_draw_str_clipped(buf, 4, 172, 248, "[SELECT] Next   [START] Poll", DTEC_COLOR_GRAY, 1);
}

static void render_realtime_graph(uint16_t *buf) {
    draw_header(buf, "LOGGER");

    const int gx = 8;
    const int gy = 24;
    const int gw = 240;
    const int gh = 104;

    gfx_rect_fill(buf, gx, gy, gw, gh, DTEC_COLOR_BLACK);
    gfx_rect(buf, gx, gy, gw, gh, DTEC_COLOR_BORDER);

    for (int y = gy + 17; y < gy + gh; y += 17) {
        for (int x = gx; x < gx + gw; x += 4) {
            gfx_pixel(buf, x, y, DTEC_COLOR_DARK_GRAY);
        }
    }

    int count = g_dtec.hist_count;
    if (count > 1) {
        float step_x = (float)gw / (float)(GRAPH_HISTORY_LEN - 1);

        for (int i = 0; i < count - 1; i++) {
            int x1 = gx + (int)(i * step_x);
            int x2 = gx + (int)((i + 1) * step_x);

            if (g_dtec.has_boost) {
                float bnorm1 = (g_dtec.hist_boost[i] + 100.0f) / 300.0f;
                float bnorm2 = (g_dtec.hist_boost[i + 1] + 100.0f) / 300.0f;
                int by1 = (gy + gh) - (int)(bnorm1 * (gh - 4)) - 2;
                int by2 = (gy + gh) - (int)(bnorm2 * (gh - 4)) - 2;
                gfx_line(buf, x1, by1, x2, by2, DTEC_COLOR_CYAN);
            }

            // RPM Trace (Orange)
            float rnorm1 = (g_dtec.hist_rpm[i]) / 8500.0f;
            float rnorm2 = (g_dtec.hist_rpm[i + 1]) / 8500.0f;
            int ry1 = (gy + gh) - (int)(rnorm1 * (gh - 4)) - 2;
            int ry2 = (gy + gh) - (int)(rnorm2 * (gh - 4)) - 2;
            gfx_line(buf, x1, ry1, x2, ry2, DTEC_COLOR_YELLOW);

            // TPS Trace (White)
            float tnorm1 = (g_dtec.hist_tps[i]) / 100.0f;
            float tnorm2 = (g_dtec.hist_tps[i + 1]) / 100.0f;
            int ty1 = (gy + gh) - (int)(tnorm1 * (gh - 4)) - 2;
            int ty2 = (gy + gh) - (int)(tnorm2 * (gh - 4)) - 2;
            gfx_line(buf, x1, ty1, x2, ty2, DTEC_COLOR_WHITE);
        }
    }

    // Legend Strip
    gfx_panel(buf, gx, 134, gw, 18, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);

    char leg[36];
    if (g_dtec.has_boost) {
        snprintf(leg, sizeof(leg), "B:%dkPa R:%d T:%d%%", (int)g_dtec.boost_kpa,
                 g_dtec.has_rpm ? (int)g_dtec.rpm : 0,
                 g_dtec.has_tps ? (int)g_dtec.tps_pct : 0);
    } else {
        snprintf(leg, sizeof(leg), "RPM:%d TPS:%d%%",
                 g_dtec.has_rpm ? (int)g_dtec.rpm : 0,
                 g_dtec.has_tps ? (int)g_dtec.tps_pct : 0);
    }
    gfx_draw_str_clipped(buf, gx + 4, 139, gw - 8, leg, DTEC_COLOR_CYAN, 1);

    gfx_draw_str_clipped(buf, 4, 168, 248, "[SELECT] Next   [START] Poll", DTEC_COLOR_GRAY, 1);
}

static void render_boost_setup(uint16_t *buf) {
    // Session stats: every value here comes from live OBD replies.
    draw_header(buf, "STATS");

    gfx_panel(buf, 6, 22, 244, 138, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);

    gfx_draw_str_clipped(buf, 12, 28, 230, "DSOBD SESSION STATS", DTEC_COLOR_CYAN, 1);

    struct {
        const char *name;
        char val_str[16];
        const char *desc;
    } rows[4];

    if (g_dtec.has_boost) {
        snprintf(rows[0].val_str, 16, "%.0f kPa", g_dtec.peak_boost);
        rows[0].name = "PEAK BOOST";
        rows[0].desc = "This connection";
    } else {
        if (g_dtec.has_coolant) snprintf(rows[0].val_str, 16, "%d C", g_dtec.coolant_c);
        else snprintf(rows[0].val_str, 16, "-- C");
        rows[0].name = "COOLANT";
        rows[0].desc = "Live temperature";
    }

    if (g_dtec.has_rpm) snprintf(rows[1].val_str, 16, "%d RPM", (int)g_dtec.peak_rpm);
    else snprintf(rows[1].val_str, 16, "--");
    rows[1].name = "PEAK RPM";
    rows[1].desc = "Highest since power on";

    if (g_dtec.has_map) snprintf(rows[2].val_str, 16, "%d kPa", g_dtec.map_kpa);
    else snprintf(rows[2].val_str, 16, "--");
    rows[2].name = "MAP READING";
    rows[2].desc = "Live manifold absolute";

    snprintf(rows[3].val_str, 16, "%s", g_dtec.auto_poll ? "ACTIVE" : "PAUSED");
    rows[3].name = "AUTO-POLL";
    rows[3].desc = "Periodic PID queries";

    for (int r = 0; r < 4; r++) {
        int ry = 44 + r * 27;
        gfx_rect_fill(buf, 10, ry, 236, 24, DTEC_COLOR_DARK_GRAY);
        gfx_rect(buf, 10, ry, 236, 24, DTEC_COLOR_BORDER);

        gfx_draw_str_clipped(buf, 14, ry + 4, 130, rows[r].name, DTEC_COLOR_WHITE, 1);
        gfx_draw_str_clipped(buf, 14, ry + 14, 130, rows[r].desc, DTEC_COLOR_GRAY, 1);
        gfx_draw_str_clipped(buf, 150, ry + 8, 90, rows[r].val_str, DTEC_COLOR_CYAN, 1);
    }

    gfx_draw_str_clipped(buf, 6, 172, 248, "[START] Poll  [SELECT] Next", DTEC_COLOR_WHITE, 1);
}

static void render_obd_terminal(uint16_t *buf) {
    draw_header(buf, "TERMINAL");

    gfx_rect_fill(buf, 4, 20, 248, 140, DTEC_COLOR_BLACK);
    gfx_rect(buf, 4, 20, 248, 140, DTEC_COLOR_BORDER);

    gfx_draw_str_clipped(buf, 8, 24, 240, "-- LIVE OBD-II PACKET LOG --", DTEC_COLOR_YELLOW, 1);

    for (int i = 0; i < OBD_LOG_LINES && i < g_dtec.log_count; i++) {
        int ly = 38 + i * 12;
        uint16_t col = DTEC_COLOR_WHITE;
        if (g_dtec.log_lines[i][0] == '>') col = DTEC_COLOR_GREEN;
        else if (strstr(g_dtec.log_lines[i], "41")) col = DTEC_COLOR_CYAN;
        gfx_draw_str_clipped(buf, 8, ly, 238, g_dtec.log_lines[i], col, 1);
    }

    gfx_rect_fill(buf, 0, 164, SCREEN_W, 28, DTEC_COLOR_PANEL);
    gfx_line(buf, 0, 164, SCREEN_W - 1, 164, DTEC_COLOR_BORDER);
    gfx_draw_str_clipped(buf, 4, 168, 248, "[SELECT] Next   [START] Poll", DTEC_COLOR_WHITE, 1);
    gfx_draw_str_clipped(buf, 4, 179, 248, "[A] ATRV   [B] 010C   [X] 010D", DTEC_COLOR_GRAY, 1);
}

static void render_car_select(uint16_t *buf) {
    draw_header(buf, "VEHICLE");
    int sel = g_dtec.selected_profile;
    if (sel < 0 || sel >= DTEC_PROFILE_COUNT) sel = 0;
    const DTEC_CarProfile *p = &g_dtec_profiles[sel];
    gfx_draw_str_clipped(buf, 8, 26, 240, p->brand, DTEC_COLOR_CYAN, 1);
    gfx_draw_str_clipped(buf, 8, 40, 240, p->name, DTEC_COLOR_WHITE, 1);
    gfx_draw_str_clipped(buf, 8, 54, 240, p->desc, DTEC_COLOR_GRAY, 1);
    gfx_panel(buf, 4, 78, 248, 70, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);
    gfx_draw_str(buf, 12, 85, "ADAPTER CONNECTION", DTEC_COLOR_CYAN, 1);
    char connection[40];
    snprintf(connection, sizeof(connection), "PROTOCOL  %s", p->protocol_cmd ? p->protocol_cmd : "AUTO");
    gfx_draw_str_clipped(buf, 12, 101, 232, connection, DTEC_COLOR_WHITE, 1);
    snprintf(connection, sizeof(connection), "HEADER    %s", p->header_cmd ? p->header_cmd : "BROADCAST");
    gfx_draw_str_clipped(buf, 12, 116, 232, connection, DTEC_COLOR_WHITE, 1);
    snprintf(connection, sizeof(connection), "%s:%u", g_dtec.target_ip, g_dtec.target_port);
    gfx_draw_str_clipped(buf, 12, 132, 232, connection, DTEC_COLOR_GRAY, 1);
    gfx_panel(buf, 4, 160, 248, 28, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);
    char line[40];
    snprintf(line, sizeof(line), "ECU %s  /  %s", p->protocol_cmd ? p->protocol_cmd : "AUTO",
        p->header_cmd ? p->header_cmd : "BROADCAST");
    gfx_draw_str_clipped(buf, 10, 164, 236, line, DTEC_COLOR_CYAN, 1);
    gfx_draw_str(buf, 10, 177, "D-PAD / CHOOSE   A / CONNECT", DTEC_COLOR_WHITE, 1);
}

static void render_settings(uint16_t *buf) {
    draw_header(buf, "SETUP");
    gfx_draw_str(buf, 12, 30, "SETTINGS", DTEC_COLOR_WHITE, 2);
    gfx_draw_str(buf, 12, 58, "SOUND EFFECTS", DTEC_COLOR_CYAN, 1);
    gfx_panel(buf, 12, 82, 232, 50, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);
    gfx_draw_str(buf, 22, 92, g_dtec.sound_enabled ? "ON" : "OFF", DTEC_COLOR_WHITE, 2);
    gfx_draw_str(buf, 22, 116, "MENU + CONNECTION CUES", DTEC_COLOR_GRAY, 1);
    gfx_draw_str(buf, 12, 154, "A / TOGGLE   X / TEST", DTEC_COLOR_WHITE, 1);
    gfx_draw_str(buf, 12, 174, "B / RETURN TO PREVIOUS VIEW", DTEC_COLOR_GRAY, 1);
}

// ---- Racing HUD modes -------------------------------------------------------
// Values ease toward the latest OBD reading every frame, so bars and arcs glide
// at 60 FPS between replies instead of jumping when each one lands.
#define HUD_REDLINE 7000.0f
#define HUD_RPM_MAX 8500.0f

static float s_rpm_ease;
static float s_spd_ease;
static uint32_t s_hud_frame;

static void ease_values(void) {
    float rpm = g_dtec.has_rpm ? g_dtec.rpm : 0.0f;
    float spd = g_dtec.has_speed ? (float)g_dtec.speed_kmh : 0.0f;
    s_rpm_ease += (rpm - s_rpm_ease) * 0.35f;
    s_spd_ease += (spd - s_spd_ease) * 0.35f;
    s_hud_frame++;
}

static float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static int speed_or_dash(void) {
    return g_dtec.has_speed ? (int)g_dtec.speed_kmh : -1;
}

// Row of shift LEDs: green, then yellow, then red; all flash at the redline.
static void draw_shift_lights(uint16_t *buf, int y, int h) {
    const int n = 16;
    int lit = (int)(clamp01(s_rpm_ease / HUD_REDLINE) * n + 0.5f);
    bool flash = s_rpm_ease >= HUD_REDLINE;
    for (int i = 0; i < n; i++) {
        uint16_t col = i < 6 ? DTEC_COLOR_GREEN : (i < 11 ? DTEC_COLOR_YELLOW : DTEC_COLOR_RED);
        if (flash) col = (s_hud_frame & 4) ? DTEC_COLOR_CYAN : DTEC_COLOR_RED;
        else if (i >= lit) col = DTEC_COLOR_DARK_GRAY;
        if (flash && (s_hud_frame & 2)) col = DTEC_COLOR_DARK_GRAY;
        gfx_rect_fill(buf, 2 + i * 16, y, 12, h, col);
    }
}

static void draw_link_dot(uint16_t *buf, int x, int y) {
    uint16_t col = DTEC_COLOR_RED;
    if (g_dtec.net_status == NET_OBD_CONNECTED) col = DTEC_COLOR_GREEN;
    else if (g_dtec.net_status == NET_TCP_CONNECTING || g_dtec.net_status == NET_WIFI_CONNECTING) col = DTEC_COLOR_YELLOW;
    gfx_rect_fill(buf, x, y, 4, 4, col);
}

static void draw_hbar(uint16_t *buf, int x, int y, int w, int h, float frac, uint16_t col) {
    gfx_rect_fill(buf, x, y, w, h, DTEC_COLOR_DARK_GRAY);
    gfx_rect_fill(buf, x, y, (int)(w * clamp01(frac)), h, col);
}

static void draw_vbar(uint16_t *buf, int x, int y, int w, int h, float frac, uint16_t col) {
    int fill = (int)(h * clamp01(frac));
    gfx_rect_fill(buf, x, y, w, h, DTEC_COLOR_DARK_GRAY);
    gfx_rect_fill(buf, x, y + h - fill, w, fill, col);
    gfx_rect(buf, x - 1, y - 1, w + 2, h + 2, DTEC_COLOR_BORDER);
}

// Neon arcade: shift lights, giant slanted speed digits, RPM sweep underneath.
static void render_digital_speed(uint16_t *buf) {
    draw_shift_lights(buf, 3, 9);

    uint16_t on = s_rpm_ease >= HUD_REDLINE ? DTEC_COLOR_RED : DTEC_COLOR_WHITE;
    gfx_seg_number(buf, 8, 22, 42, 96, 10, 3, speed_or_dash(), on, DTEC_RGB(2, 3, 4), 3);
    gfx_draw_str(buf, 186, 106, "km/h", DTEC_COLOR_CYAN, 2);

    // RPM sweep: segmented, colored by zone.
    int segs = 42, lit = (int)(clamp01(s_rpm_ease / HUD_RPM_MAX) * segs);
    for (int i = 0; i < segs; i++) {
        float f = (float)i / segs;
        uint16_t col = f < 0.6f ? DTEC_COLOR_CYAN : (f < 0.82f ? DTEC_COLOR_YELLOW : DTEC_COLOR_RED);
        gfx_rect_fill(buf, 2 + i * 6, 128, 5, 8, i < lit ? col : DTEC_COLOR_DARK_GRAY);
    }

    gfx_draw_str(buf, 4, 146, "RPM", DTEC_COLOR_GRAY, 1);
    gfx_seg_number(buf, 4, 158, 14, 26, 3, 4, g_dtec.has_rpm ? (int)g_dtec.rpm : -1,
                   DTEC_COLOR_CYAN, DTEC_RGB(1, 3, 4), 2);

    gfx_draw_str(buf, 104, 146, "THROTTLE", DTEC_COLOR_GRAY, 1);
    draw_hbar(buf, 104, 160, 100, 10, g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f, DTEC_COLOR_GREEN);
    char tps[8];
    if (g_dtec.has_tps) snprintf(tps, sizeof(tps), "%d%%", (int)g_dtec.tps_pct);
    else snprintf(tps, sizeof(tps), "--%%");
    gfx_draw_str(buf, 212, 161, tps, DTEC_COLOR_WHITE, 1);
    gfx_draw_str(buf, 104, 178, "DIGITAL", DTEC_COLOR_DARK_GRAY, 1);
    draw_link_dot(buf, 250, 186);
}

// Integer polar lookup in half-degree steps (Q12): the ARM9 has no FPU, so the
// gauge modes never call sinf/cosf per frame. y points down, 0 deg points right.
static int16_t s_cos[720], s_sin[720];
static bool s_lut_ready;

static void lut_init(void) {
    for (int i = 0; i < 720; i++) {
        float a = i * (float)M_PI / 360.0f;
        s_cos[i] = (int16_t)(cosf(a) * 4096.0f);
        s_sin[i] = (int16_t)(sinf(a) * 4096.0f);
    }
    s_lut_ready = true;
}

static void polar(int cx, int cy, int r, int hd, int *x, int *y) {
    hd %= 720;
    if (hd < 0) hd += 720;
    *x = cx + ((r * s_cos[hd]) >> 12);
    *y = cy + ((r * s_sin[hd]) >> 12);
}

static void radial(uint16_t *buf, int cx, int cy, int r0, int r1, int hd, uint16_t col) {
    int x0, y0, x1, y1;
    polar(cx, cy, r0, hd, &x0, &y0);
    polar(cx, cy, r1, hd, &x1, &y1);
    gfx_line(buf, x0, y0, x1, y1, col);
}

// Radial stroke doubled one pixel over, so adjacent strokes of a band leave no holes.
static void radial_fill(uint16_t *buf, int cx, int cy, int r0, int r1, int hd, uint16_t col) {
    radial(buf, cx, cy, r0, r1, hd, col);
    radial(buf, cx + 1, cy, r0, r1, hd, col);
}

// Half-degree angle of `frac` along a dial that starts at start_deg and sweeps sweep_deg.
static int dial_hd(int start_deg, int sweep_deg, float frac) {
    return start_deg * 2 + (int)(clamp01(frac) * sweep_deg * 2);
}

// Solid band between two radii covering fractions f0..f1 of the dial.
static void arc_band(uint16_t *buf, int cx, int cy, int r0, int r1, int start_deg, int sweep_deg,
                     float f0, float f1, uint16_t col) {
    int end = dial_hd(start_deg, sweep_deg, f1);
    for (int hd = dial_hd(start_deg, sweep_deg, f0); hd <= end && f1 > f0; hd++) {
        radial_fill(buf, cx, cy, r0, r1, hd, col);
    }
}

static void draw_centered(uint16_t *buf, int cx, int y, const char *str, uint16_t col, int scale) {
    gfx_draw_str(buf, cx - (int)strlen(str) * 4 * scale, y, str, col, scale);
}

static void dial_labels(uint16_t *buf, int cx, int cy, int r, int start_deg, int sweep_deg, int every) {
    for (int k = 0; k * 1000 <= HUD_RPM_MAX; k += every) {
        int x, y;
        char d[2] = { (char)('0' + k), '\0' };
        polar(cx, cy, r, dial_hd(start_deg, sweep_deg, k * 1000 / HUD_RPM_MAX), &x, &y);
        gfx_draw_str(buf, x - 4, y - 4, d, k * 1000 >= HUD_REDLINE ? DTEC_COLOR_RED : DTEC_COLOR_WHITE, 1);
    }
}

static uint16_t zone_color(float f) {
    return f < 0.6f ? DTEC_COLOR_CYAN : (f < 0.82f ? DTEC_COLOR_YELLOW : DTEC_COLOR_RED);
}

// Readable across a room: speed fills the whole screen, white on black with no
// unlit segments, plus a thin RPM strip along the bottom edge.
static void render_big_speed(uint16_t *buf) {
    gfx_seg_number(buf, 2, 2, 64, 174, 14, 3, speed_or_dash(), DTEC_COLOR_WHITE, DTEC_COLOR_BLACK, 0);
    float f = clamp01(s_rpm_ease / HUD_RPM_MAX);
    uint16_t col = zone_color(f);
    if (s_rpm_ease >= HUD_REDLINE && (s_hud_frame & 4)) col = DTEC_COLOR_WHITE;
    gfx_rect_fill(buf, 0, 184, SCREEN_W, 8, DTEC_COLOR_DARK_GRAY);
    gfx_rect_fill(buf, 0, 184, (int)(SCREEN_W * f), 8, col);
}

// Street-racer arc tach wrapped around a digital speed readout.
static void render_arc_tach(uint16_t *buf) {
    // 240 degree sweep from lower-left, over the top, to lower-right; 1.5 deg wedges.
    gfx_dial(buf, 128, 108, 78);
    int lit = dial_hd(150, 240, s_rpm_ease / HUD_RPM_MAX);
    for (int hd = 300; hd <= 780; hd++) {
        if ((hd - 300) % 15 >= 12) continue; // gap between wedges
        uint16_t col = hd < lit ? zone_color((hd - 300) / 480.0f) : DTEC_COLOR_DARK_GRAY;
        radial_fill(buf, 128, 108, 72, 92, hd, col);
    }
    dial_labels(buf, 128, 108, 62, 150, 240, 1);

    gfx_seg_number(buf, 80, 72, 24, 48, 6, 3, speed_or_dash(), DTEC_COLOR_WHITE, DTEC_RGB(2, 3, 4), 2);
    gfx_draw_str(buf, 112, 124, "km/h", DTEC_COLOR_CYAN, 1);

    char rpm[16];
    if (g_dtec.has_rpm) snprintf(rpm, sizeof(rpm), "%d RPM", (int)g_dtec.rpm);
    else snprintf(rpm, sizeof(rpm), "--- RPM");
    gfx_draw_str(buf, 128 - (int)strlen(rpm) * 4, 138, rpm,
                 s_rpm_ease >= HUD_REDLINE ? DTEC_COLOR_RED : DTEC_COLOR_YELLOW, 1);

    gfx_draw_str(buf, 4, 168, "TPS", DTEC_COLOR_GRAY, 1);
    draw_hbar(buf, 32, 168, 80, 8, g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f, DTEC_COLOR_GREEN);
    if (g_dtec.has_boost) {
        gfx_draw_str(buf, 140, 168, "BST", DTEC_COLOR_GRAY, 1);
        draw_hbar(buf, 168, 168, 80, 8, g_dtec.boost_kpa / 200.0f, DTEC_COLOR_CYAN);
    } else {
        gfx_draw_str(buf, 140, 168, "LOD", DTEC_COLOR_GRAY, 1);
        draw_hbar(buf, 168, 168, 80, 8, g_dtec.has_load ? g_dtec.engine_load_pct / 100.0f : 0.0f, DTEC_COLOR_PURPLE);
    }
    draw_link_dot(buf, 250, 186);
}

// Endurance-wheel amber LCD: shift lights, pedal bars, data boxes.
#define LCD_ON    DTEC_RGB(31, 20, 2)
#define LCD_GHOST DTEC_RGB(4, 3, 0)
#define LCD_BG    DTEC_RGB(2, 1, 0)

static void render_sim_dash(uint16_t *buf) {
    draw_shift_lights(buf, 2, 8);

    bool redline = s_rpm_ease >= HUD_REDLINE;
    gfx_rect_fill(buf, 40, 16, 176, 114, LCD_BG);
    gfx_rect(buf, 40, 16, 176, 114, (redline && (s_hud_frame & 4)) ? DTEC_COLOR_RED : DTEC_COLOR_ORANGE);
    gfx_seg_number(buf, 50, 26, 40, 76, 8, 3, speed_or_dash(), LCD_ON, LCD_GHOST, 0);
    gfx_draw_str(buf, 112, 112, "KM/H", LCD_ON, 1);

    draw_vbar(buf, 8, 18, 24, 110, g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f, DTEC_COLOR_GREEN);
    gfx_draw_str(buf, 8, 134, "THR", DTEC_COLOR_GRAY, 1);
    if (g_dtec.has_boost) {
        draw_vbar(buf, 224, 18, 24, 110, g_dtec.boost_kpa / 200.0f, DTEC_COLOR_CYAN);
        gfx_draw_str(buf, 224, 134, "BST", DTEC_COLOR_GRAY, 1);
    } else {
        draw_vbar(buf, 224, 18, 24, 110, g_dtec.has_load ? g_dtec.engine_load_pct / 100.0f : 0.0f, DTEC_COLOR_PURPLE);
        gfx_draw_str(buf, 220, 134, "LOAD", DTEC_COLOR_GRAY, 1);
    }

    // RPM | coolant | volts
    for (int b = 0; b < 3; b++) {
        int bx = 4 + b * 84;
        gfx_rect_fill(buf, bx, 146, 80, 42, LCD_BG);
        gfx_rect(buf, bx, 146, 80, 42, DTEC_COLOR_BORDER);
    }
    gfx_draw_str(buf, 8, 149, "RPM", DTEC_COLOR_GRAY, 1);
    gfx_seg_number(buf, 10, 160, 13, 24, 3, 4, g_dtec.has_rpm ? (int)g_dtec.rpm : -1,
                   redline ? DTEC_COLOR_RED : LCD_ON, LCD_GHOST, 0);

    char cool[12];
    if (g_dtec.has_coolant) snprintf(cool, sizeof(cool), "%dC", g_dtec.coolant_c);
    else snprintf(cool, sizeof(cool), "--C");
    gfx_draw_str(buf, 92, 149, "WATER", DTEC_COLOR_GRAY, 1);
    gfx_draw_str(buf, 96, 166, cool, (g_dtec.has_coolant && g_dtec.coolant_c > 104) ? DTEC_COLOR_RED : LCD_ON, 2);

    gfx_draw_str(buf, 176, 149, "BATT", DTEC_COLOR_GRAY, 1);
    gfx_draw_str_clipped(buf, 176, 168, 76, g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V",
                         g_dtec.has_voltage ? LCD_ON : DTEC_COLOR_GRAY, 1);
    draw_link_dot(buf, 250, 186);
}

static float tps_frac(void) { return g_dtec.has_tps ? g_dtec.tps_pct / 100.0f : 0.0f; }

// Gran Turismo style: center-out rev strip, clean 270 deg tach, thin white numerals.
#define GT_BLUE DTEC_RGB(8, 18, 31)

static void render_gt_style(uint16_t *buf) {
    float frac = s_rpm_ease / HUD_RPM_MAX;
    bool redline = s_rpm_ease >= HUD_REDLINE;

    int half = (int)(clamp01(s_rpm_ease / HUD_REDLINE) * 100);
    uint16_t strip = s_rpm_ease > HUD_REDLINE * 0.85f ? DTEC_COLOR_RED : GT_BLUE;
    if (redline && (s_hud_frame & 4)) strip = DTEC_COLOR_WHITE;
    gfx_rect_fill(buf, 28, 4, 200, 4, DTEC_COLOR_DARK_GRAY);
    gfx_rect_fill(buf, 128 - half, 4, half * 2, 4, strip);

    const int cx = 128, cy = 104;
    gfx_dial(buf, cx, cy, 84);
    arc_band(buf, cx, cy, 80, 81, 135, 270, 0.0f, 1.0f, DTEC_COLOR_GRAY);
    arc_band(buf, cx, cy, 70, 78, 135, 270, HUD_REDLINE / HUD_RPM_MAX, 1.0f, DTEC_RGB(9, 1, 2));
    arc_band(buf, cx, cy, 70, 78, 135, 270, 0.0f, frac, redline ? DTEC_COLOR_RED : DTEC_COLOR_WHITE);
    for (int k = 0; k * 1000 <= HUD_RPM_MAX; k++) {
        radial(buf, cx, cy, 64, 69, dial_hd(135, 270, k * 1000 / HUD_RPM_MAX), DTEC_COLOR_WHITE);
    }
    dial_labels(buf, cx, cy, 55, 135, 270, 1);
    draw_centered(buf, cx, cy - 34, "x1000r/min", DTEC_COLOR_GRAY, 1);

    gfx_seg_number(buf, 89, 82, 20, 38, 4, 3, speed_or_dash(), DTEC_COLOR_WHITE, DTEC_COLOR_BLACK, 0);
    draw_centered(buf, cx, 126, "km/h", DTEC_COLOR_GRAY, 1);

    char rpm[16];
    if (g_dtec.has_rpm) snprintf(rpm, sizeof(rpm), "%d rpm", (int)g_dtec.rpm);
    else snprintf(rpm, sizeof(rpm), "--- rpm");
    draw_centered(buf, cx, 142, rpm, GT_BLUE, 1);

    // Slim pedal-style throttle gauge under the dial.
    gfx_draw_str(buf, 52, 178, "THR", DTEC_COLOR_GRAY, 1);
    draw_hbar(buf, 80, 180, 124, 4, tps_frac(), GT_BLUE);
    draw_link_dot(buf, 250, 186);
}

// Forza style: dark round dial, white rim progress, 500 rpm ticks, orange needle.
static void render_forza_style(uint16_t *buf) {
    const int cx = 128, cy = 98, r = 88;
    float frac = s_rpm_ease / HUD_RPM_MAX;
    float red_f = HUD_REDLINE / HUD_RPM_MAX;

    gfx_dial(buf, cx, cy, r);
    arc_band(buf, cx, cy, r - 6, r - 2, 135, 270, red_f, 1.0f, DTEC_RGB(12, 2, 2));
    arc_band(buf, cx, cy, r - 6, r - 2, 135, 270, 0.0f, frac < red_f ? frac : red_f, DTEC_COLOR_WHITE);
    arc_band(buf, cx, cy, r - 6, r - 2, 135, 270, red_f, frac, DTEC_COLOR_RED);
    for (int k = 0; k * 500 <= HUD_RPM_MAX; k++) {
        uint16_t col = k * 500 >= HUD_REDLINE ? DTEC_COLOR_RED : DTEC_COLOR_WHITE;
        radial(buf, cx, cy, (k & 1) ? r - 13 : r - 18, r - 9, dial_hd(135, 270, k * 500 / HUD_RPM_MAX), col);
    }
    dial_labels(buf, cx, cy, r - 28, 135, 270, 1);

    // Needle: three parallel strokes from the hub.
    int nx, ny, hd = dial_hd(135, 270, frac);
    polar(cx, cy, r - 10, hd, &nx, &ny);
    for (int o = -1; o <= 1; o++) {
        gfx_line(buf, cx + o, cy, nx + o, ny, DTEC_COLOR_ORANGE);
        gfx_line(buf, cx, cy + o, nx, ny + o, DTEC_COLOR_ORANGE);
    }
    gfx_circle_fill(buf, cx, cy, 14, DTEC_RGB(1, 1, 2));
    gfx_circle(buf, cx, cy, 14, DTEC_COLOR_ORANGE);

    char spd[8];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)g_dtec.speed_kmh);
    else snprintf(spd, sizeof(spd), "---");
    draw_centered(buf, cx, 126, spd, DTEC_COLOR_WHITE, 3);
    draw_centered(buf, cx, 152, "KM/H", DTEC_COLOR_GRAY, 1);

    char tps[12];
    if (g_dtec.has_tps) snprintf(tps, sizeof(tps), "THR %d%%", (int)g_dtec.tps_pct);
    else snprintf(tps, sizeof(tps), "THR --%%");
    gfx_draw_str(buf, 2, 2, tps, DTEC_COLOR_WHITE, 1);
    if (g_dtec.has_boost) {
        char bst[16];
        snprintf(bst, sizeof(bst), "%+.0fkPa", g_dtec.boost_kpa);
        gfx_draw_str(buf, 254 - (int)strlen(bst) * 8, 2, bst, DTEC_COLOR_CYAN, 1);
    }
    draw_link_dot(buf, 250, 186);
}

// BeamNG style: dark app panels with an orange title rule, round tacho, pedal bars.
#define BNG_PANEL DTEC_RGB(3, 3, 4)

static void bng_panel(uint16_t *buf, int x, int y, int w, int h, const char *title) {
    gfx_rect_fill(buf, x, y, w, h, BNG_PANEL);
    gfx_rect_fill(buf, x, y, w, 2, DTEC_COLOR_ORANGE);
    gfx_draw_str(buf, x + 4, y + 5, title, DTEC_COLOR_GRAY, 1);
}

static void render_beamng_style(uint16_t *buf) {
    bng_panel(buf, 2, 2, 138, 188, "TACHO");
    const int cx = 71, cy = 96;
    int lit = dial_hd(135, 270, s_rpm_ease / HUD_RPM_MAX);
    for (int hd = 270; hd <= 810; hd++) {
        uint16_t col = DTEC_COLOR_DARK_GRAY;
        if (hd < lit) {
            float f = (hd - 270) / 540.0f;
            col = f < 0.6f ? DTEC_COLOR_WHITE : (f < 0.82f ? DTEC_COLOR_ORANGE : DTEC_COLOR_RED);
        }
        radial_fill(buf, cx, cy, 50, 60, hd, col);
    }
    dial_labels(buf, cx, cy, 41, 135, 270, 2);

    char spd[8];
    if (g_dtec.has_speed) snprintf(spd, sizeof(spd), "%u", (unsigned)g_dtec.speed_kmh);
    else snprintf(spd, sizeof(spd), "---");
    draw_centered(buf, cx, 80, spd, DTEC_COLOR_WHITE, 3);
    draw_centered(buf, cx, 106, "km/h", DTEC_COLOR_GRAY, 1);

    char rpm[16];
    if (g_dtec.has_rpm) snprintf(rpm, sizeof(rpm), "%d rpm", (int)g_dtec.rpm);
    else snprintf(rpm, sizeof(rpm), "--- rpm");
    draw_centered(buf, cx, 162, rpm, DTEC_COLOR_ORANGE, 1);

    // Engine readouts
    bng_panel(buf, 144, 2, 110, 64, "ENGINE");
    char line[24];
    if (g_dtec.has_coolant) snprintf(line, sizeof(line), "Temp %dC", g_dtec.coolant_c);
    else snprintf(line, sizeof(line), "Temp --C");
    gfx_draw_str(buf, 148, 20, line, DTEC_COLOR_WHITE, 1);
    snprintf(line, sizeof(line), "Batt %.6s", g_dtec.has_voltage ? g_dtec.battery_voltage : "--.-V");
    gfx_draw_str(buf, 148, 34, line, DTEC_COLOR_WHITE, 1);
    if (g_dtec.has_boost) snprintf(line, sizeof(line), "Boost %+.0f", g_dtec.boost_kpa);
    else if (g_dtec.has_load) snprintf(line, sizeof(line), "Load %d%%", (int)g_dtec.engine_load_pct);
    else snprintf(line, sizeof(line), "Load --%%");
    gfx_draw_str(buf, 148, 48, line, DTEC_COLOR_WHITE, 1);

    // Pedal-input bars (throttle green, load blue)
    bng_panel(buf, 144, 70, 110, 120, "INPUTS");
    draw_vbar(buf, 162, 88, 22, 82, tps_frac(), DTEC_COLOR_GREEN);
    draw_vbar(buf, 212, 88, 22, 82, g_dtec.has_load ? g_dtec.engine_load_pct / 100.0f : 0.0f, GT_BLUE);
    gfx_draw_str(buf, 161, 176, "THR", DTEC_COLOR_GRAY, 1);
    gfx_draw_str(buf, 211, 176, "LOD", DTEC_COLOR_GRAY, 1);
    draw_link_dot(buf, 250, 186);
}

// ---- Diagnostics ------------------------------------------------------------
// PIDs 1C and 51 are SAE J1979 enumerations, not measurements.
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
    snprintf(out, n, "%.*f%s", p->decimals, v, p->unit);
}

static void render_codes(uint16_t *buf) {
    draw_header(buf, "CODES");
    char line[64], a[16], b[16];
    bool mil = g_dtec.has_monitors && g_dtec.mil_on;
    gfx_panel(buf, 4, 20, 248, 36, DTEC_COLOR_PANEL, mil ? DTEC_COLOR_ORANGE : DTEC_COLOR_BORDER);
    gfx_circle_fill(buf, 21, 38, 9, mil ? DTEC_COLOR_ORANGE : DTEC_COLOR_DARK_GRAY);
    gfx_draw_str(buf, 38, 25, g_dtec.scanning ? "READING ECU..." : !g_dtec.has_monitors ? "NOT SCANNED" :
        mil ? "CHECK ENGINE ON" : "CHECK ENGINE OFF", mil ? DTEC_COLOR_ORANGE : DTEC_COLOR_WHITE, 1);
    int counts[3] = {0};
    for (int i = 0; i < g_dtec.dtc_count; i++) counts[g_dtec.dtc_kind[i] % 3]++;
    snprintf(line, sizeof(line), "%d STORED %d PENDING %d PERM", counts[0], counts[1], counts[2]);
    gfx_draw_str_clipped(buf, 38, 40, 210, line, DTEC_COLOR_GRAY, 1);

    int total = __builtin_popcount(g_dtec.mon_supported);
    int ready = __builtin_popcount(g_dtec.mon_supported & ~g_dtec.mon_incomplete);
    if (g_dtec.has_monitors) snprintf(line, sizeof(line), "READINESS %d/%d MONITORS READY", ready, total);
    else snprintf(line, sizeof(line), "READINESS --");
    gfx_draw_str_clipped(buf, 8, 61, 240, line, !g_dtec.has_monitors ? DTEC_COLOR_GRAY :
        ready == total ? DTEC_COLOR_GREEN : DTEC_COLOR_YELLOW, 1);
    snprintf(line, sizeof(line), "VIN %s", g_dtec.vin[0] ? g_dtec.vin : "--");
    gfx_draw_str_clipped(buf, 8, 73, 240, line, DTEC_COLOR_WHITE, 1);
    gfx_draw_str_clipped(buf, 8, 85, 240, g_dtec.protocol[0] ? g_dtec.protocol : "PROTOCOL --", DTEC_COLOR_GRAY, 1);
    pid_text(dtec_obd_pid_index(0x31), false, a, sizeof(a));
    pid_text(dtec_obd_pid_index(0x21), false, b, sizeof(b));
    snprintf(line, sizeof(line), "CLEARED %s AGO  MIL %s", a, b);
    gfx_draw_str_clipped(buf, 8, 97, 240, line, DTEC_COLOR_GRAY, 1);

    gfx_panel(buf, 4, 109, 248, 63, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);
    if (g_dtec.ff_dtc) { dtec_obd_format_dtc(g_dtec.ff_dtc, a); snprintf(line, sizeof(line), "FREEZE FRAME  %s", a); }
    else snprintf(line, sizeof(line), "FREEZE FRAME  NONE");
    gfx_draw_str(buf, 10, 113, line, DTEC_COLOR_CYAN, 1);
    for (int i = 0, row = 0; i < DTEC_PID_COUNT && row < 4; i++) {
        if (!g_dtec.ff_has[i]) continue;
        int y = 126 + row++ * 11;
        gfx_draw_str_clipped(buf, 10, y, 140, g_dtec_pids[i].name, DTEC_COLOR_GRAY, 1);
        pid_text(i, true, a, sizeof(a));
        gfx_draw_str(buf, 246 - (int)strlen(a) * 8, y, a, DTEC_COLOR_WHITE, 1);
    }
    gfx_draw_str_clipped(buf, 6, 178, 244, g_dtec.clear_armed ? "ENGINE OFF + Y AGAIN TO ERASE" :
        "[A] SCAN   [Y] CLEAR", g_dtec.clear_armed ? DTEC_COLOR_RED : DTEC_COLOR_WHITE, 1);
}

static void render_sensors(uint16_t *buf) {
    draw_header(buf, "SENSORS");
    int list[DTEC_PID_COUNT], n = dtec_obd_sensor_list(list);
    int pages = n ? (n + DTEC_SENSOR_ROWS - 1) / DTEC_SENSOR_ROWS : 1;
    int page = g_dtec.sensor_page % pages;
    char v[48];
    for (int r = 0; r < DTEC_SENSOR_ROWS && page * DTEC_SENSOR_ROWS + r < n; r++) {
        int i = list[page * DTEC_SENSOR_ROWS + r], y = 21 + r * 18;
        gfx_panel(buf, 4, y, 248, 16, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);
        gfx_draw_str_clipped(buf, 10, y + 4, 120, g_dtec_pids[i].name, DTEC_COLOR_GRAY, 1);
        pid_text(i, false, v, sizeof(v));
        gfx_draw_str(buf, 246 - (int)strlen(v) * 8, y + 4, v, g_dtec.pid_has[i] ? DTEC_COLOR_WHITE : DTEC_COLOR_GRAY, 1);
    }
    snprintf(v, sizeof(v), "PAGE %d/%d  %d PIDS", page + 1, pages, n);
    gfx_draw_str(buf, 6, 172, v, DTEC_COLOR_CYAN, 1);
    gfx_draw_str(buf, 6, 182, "[UP/DOWN] PAGE  [SELECT] NEXT", DTEC_COLOR_GRAY, 1);
}

static void render_ecu_info(uint16_t *buf) {
    draw_header(buf, "ECU");
    char line[64], v[32];
    gfx_panel(buf, 4, 20, 248, 64, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);
    const char *rows[][2] = {{"VIN", g_dtec.vin}, {"ECU", g_dtec.ecu_name}, {"CAL", g_dtec.cal_id}};
    for (int r = 0; r < 3; r++) {
        snprintf(line, sizeof(line), "%s %s", rows[r][0], rows[r][1][0] ? rows[r][1] : "--");
        gfx_draw_str_clipped(buf, 10, 25 + r * 11, 236, line, DTEC_COLOR_WHITE, 1);
    }
    pid_text(dtec_obd_pid_index(0x1C), false, v, sizeof(v));
    snprintf(line, sizeof(line), "OBD %s", v);
    gfx_draw_str_clipped(buf, 10, 58, 130, line, DTEC_COLOR_GRAY, 1);
    pid_text(dtec_obd_pid_index(0x51), false, v, sizeof(v));
    gfx_draw_str_clipped(buf, 246 - (int)strlen(v) * 8, 58, 100, v, DTEC_COLOR_GRAY, 1);
    pid_text(dtec_obd_pid_index(0xA6), false, v, sizeof(v));
    snprintf(line, sizeof(line), "ODOMETER %s", v);
    gfx_draw_str_clipped(buf, 10, 69, 236, line, DTEC_COLOR_GRAY, 1);

    gfx_panel(buf, 4, 88, 248, 84, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);
    gfx_draw_str(buf, 10, 92, "MISFIRE COUNTS", DTEC_COLOR_CYAN, 1);
    if (!g_dtec.misfire_cyls) {
        const char *msg = g_dtec.scanning ? "READING ECU..." : !g_dtec.codes_read ? "A / SCAN TO READ" :
            "NOT REPORTED BY THIS CAR";
        gfx_draw_str(buf, 128 - (int)strlen(msg) * 4, 126, msg, DTEC_COLOR_GRAY, 1);
    } else {
        gfx_draw_str(buf, 10, 103, "CYL DRIVE  AVG", DTEC_COLOR_GRAY, 1);
        if (__builtin_popcount(g_dtec.misfire_cyls) > 6) gfx_draw_str(buf, 130, 103, "CYL DRIVE  AVG", DTEC_COLOR_GRAY, 1);
        for (int c = 0, k = 0; c < DTEC_MAX_CYLINDERS; c++) {
            if (!(g_dtec.misfire_cyls & (1u << c))) continue;
            snprintf(line, sizeof(line), "C%-2d%6u%6u", c + 1, g_dtec.misfire_drive[c], g_dtec.misfire_avg[c]);
            gfx_draw_str(buf, 10 + (k / 6) * 120, 114 + (k % 6) * 9, line,
                g_dtec.misfire_drive[c] ? DTEC_COLOR_ORANGE : DTEC_COLOR_WHITE, 1);
            k++;
        }
    }
    gfx_draw_str_clipped(buf, 6, 178, 244, "[A] SCAN  AVG: LAST 10 DRIVES", DTEC_COLOR_WHITE, 1);
}

void dtec_render_top(uint16_t *buf) {
    if (!buf) return;

    bool hud = g_dtec.screen_mode >= DTEC_MODE_DIGITAL_SPEED && g_dtec.screen_mode <= DTEC_MODE_BEAMNG_STYLE;
    gfx_clear(buf, hud ? DTEC_COLOR_BLACK : DTEC_COLOR_BG);
    if (g_dtec.screen_mode != DTEC_MODE_BIG_SPEED) gfx_background(buf);
    if (!s_lut_ready) lut_init();
    ease_values();

    switch (g_dtec.screen_mode) {
        case DTEC_MODE_SETTINGS:       render_settings(buf); break;
        case DTEC_MODE_CAR_SELECT:     render_car_select(buf); break;
        case DTEC_MODE_MULTI_GAUGE:    render_multi_gauge(buf); break;
        case DTEC_MODE_DIGITAL_SPEED:  render_digital_speed(buf); break;
        case DTEC_MODE_BIG_SPEED:      render_big_speed(buf); break;
        case DTEC_MODE_ARC_TACH:       render_arc_tach(buf); break;
        case DTEC_MODE_SIM_DASH:       render_sim_dash(buf); break;
        case DTEC_MODE_GT_STYLE:       render_gt_style(buf); break;
        case DTEC_MODE_FORZA_STYLE:    render_forza_style(buf); break;
        case DTEC_MODE_BEAMNG_STYLE:   render_beamng_style(buf); break;
        case DTEC_MODE_ANALOG_DIALS:   render_analog_dials(buf); break;
        case DTEC_MODE_REALTIME_GRAPH: render_realtime_graph(buf); break;
        case DTEC_MODE_BOOST_SETUP:    render_boost_setup(buf); break;
        case DTEC_MODE_CODES:          render_codes(buf); break;
        case DTEC_MODE_SENSORS:        render_sensors(buf); break;
        case DTEC_MODE_ECU_INFO:       render_ecu_info(buf); break;
        case DTEC_MODE_OBD_TERMINAL:   render_obd_terminal(buf); break;
        default: break;
    }
}
