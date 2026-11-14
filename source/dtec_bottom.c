#include <stdio.h>
#include <string.h>

#include "dtec_common.h"
#include "dtec_gfx.h"
#include "dtec_net.h"
#include "dtec_obd.h"
#include "dtec_audio.h"
#include "dtec_settings.h"
#include "dtec_logger.h"

static const char *const s_mode_names[DTEC_MODE_COUNT] = {
    "VEHICLE", "MULTI", "DIGITAL", "BIG SPEED", "ARC TACH", "SIM LCD", "GT STYLE", "FORZA STYLE",
    "BEAMNG STYLE", "ANALOG", "LOGGER", "STATS", "CODES", "SENSORS", "ECU INFO", "TERMINAL", "SETTINGS"
};

static DTEC_ScreenMode s_previous_mode = DTEC_MODE_MULTI_GAUGE;

static void change_mode(int delta) {
    DTEC_ScreenMode next = (g_dtec.screen_mode + DTEC_MODE_COUNT + delta) % DTEC_MODE_COUNT;
    if (next == DTEC_MODE_SETTINGS) s_previous_mode = g_dtec.screen_mode;
    g_dtec.screen_mode = next;
    g_dtec.clear_armed = false;
    dtec_audio_play(DTEC_SFX_MOVE);
}

static void toggle_settings(void) {
    if (g_dtec.screen_mode == DTEC_MODE_SETTINGS) g_dtec.screen_mode = s_previous_mode;
    else { s_previous_mode = g_dtec.screen_mode; g_dtec.screen_mode = DTEC_MODE_SETTINGS; }
    dtec_audio_play(DTEC_SFX_MOVE);
}

static void confirm_vehicle(void) {
    g_dtec.profile_confirmed = true;
    dtec_obd_apply_profile(g_dtec.selected_profile);
    char msg[48];
    snprintf(msg, sizeof(msg), "Car: %s", g_dtec_profiles[g_dtec.selected_profile].name);
    dtec_log(msg);
    dtec_net_reconnect();
    g_dtec.screen_mode = DTEC_MODE_OBD_TERMINAL;
    dtec_audio_play(DTEC_SFX_CONFIRM);
}

void dtec_handle_keys(uint32_t keys) {
    if (keys & KEY_SELECT) { change_mode(1); return; }
    if (g_dtec.screen_mode == DTEC_MODE_SETTINGS) {
        if (keys & KEY_B) toggle_settings();
        else if (keys & KEY_A) dtec_settings_toggle_sound();
        else if (keys & KEY_X) dtec_audio_play(DTEC_SFX_CONNECTED);
        return;
    }
    if (g_dtec.screen_mode == DTEC_MODE_CAR_SELECT) {
        int selected = g_dtec.selected_profile;
        if ((keys & KEY_UP) && selected > 0) selected--;
        if ((keys & KEY_DOWN) && selected < DTEC_PROFILE_COUNT - 1) selected++;
        if (selected != g_dtec.selected_profile) {
            dtec_obd_apply_profile(selected);
            dtec_audio_play(DTEC_SFX_MOVE);
        }
        if (keys & (KEY_A | KEY_START)) confirm_vehicle();
        return;
    }
    if (keys & KEY_R) {
        dtec_logger_toggle();
        dtec_audio_play(DTEC_SFX_CONFIRM);
        return;
    }
    if (g_dtec.screen_mode == DTEC_MODE_ECU_INFO) {
        if (keys & KEY_A) { dtec_obd_scan_codes(false); dtec_audio_play(DTEC_SFX_CONFIRM); }
        return;
    }
    if (g_dtec.screen_mode == DTEC_MODE_CODES) {
        if (keys & KEY_A) {
            dtec_obd_scan_codes(false);
            dtec_audio_play(DTEC_SFX_CONFIRM);
        } else if (keys & KEY_Y) {
            // Mode 04 also erases freeze frame and readiness: require a second press.
            if (g_dtec.clear_armed) { g_dtec.clear_armed = false; dtec_obd_scan_codes(true); }
            else { g_dtec.clear_armed = true; dtec_log("Press CLEAR again to erase codes"); }
            dtec_audio_play(DTEC_SFX_CONFIRM);
        }
        return;
    }
    if (g_dtec.screen_mode == DTEC_MODE_SENSORS && (keys & (KEY_UP | KEY_DOWN))) {
        int list[DTEC_PID_COUNT];
        int pages = (dtec_obd_sensor_list(list) + DTEC_SENSOR_ROWS - 1) / DTEC_SENSOR_ROWS;
        if (pages < 1) pages = 1;
        g_dtec.sensor_page = (g_dtec.sensor_page % pages + pages + ((keys & KEY_DOWN) ? 1 : -1)) % pages;
        dtec_audio_play(DTEC_SFX_MOVE);
    }
    if (keys & KEY_START) {
        g_dtec.auto_poll = !g_dtec.auto_poll;
        dtec_log(g_dtec.auto_poll ? "Auto-Poll: ON" : "Auto-Poll: OFF");
        dtec_audio_play(DTEC_SFX_CONFIRM);
    }
    const uint32_t buttons[] = {KEY_A, KEY_B, KEY_X, KEY_Y, KEY_L};
    const char *const commands[] = {"ATRV", "010C", "010D", "010B", "0105"};
    for (unsigned i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
        if (keys & buttons[i]) {
            dtec_obd_send_cmd(commands[i]);
            dtec_audio_play(DTEC_SFX_MOVE);
        }
    }
}

void dtec_handle_touch(int tx, int ty, bool is_down) {
    if (!is_down || (unsigned)tx >= SCREEN_W || (unsigned)ty >= SCREEN_H) return;
    if (ty >= 2 && ty <= 24) {
        if (tx < 49) change_mode(-1);
        else if (tx >= 207) change_mode(1);
        else if (tx >= 51 && tx < 205) toggle_settings();
        return;
    }
    if (g_dtec.screen_mode == DTEC_MODE_SETTINGS) {
        if (tx >= 8 && tx < 248 && ty >= 42 && ty < 93) dtec_settings_toggle_sound();
        else if (tx >= 8 && tx < 248 && ty >= 105 && ty < 135) dtec_audio_play(DTEC_SFX_CONNECTED);
        else if (tx >= 8 && tx < 248 && ty >= 158 && ty < 188) toggle_settings();
        return;
    }
    if (g_dtec.screen_mode == DTEC_MODE_CAR_SELECT) {
        int car_page = g_dtec.selected_profile / 4;
        int max_page = (DTEC_PROFILE_COUNT - 1) / 4;
        for (int r = 0; r < 4; r++) {
            int ry = 28 + r * 34;
            if (tx >= 4 && tx < 252 && ty >= ry && ty < ry + 30) {
                int idx = car_page * 4 + r;
                if (idx < DTEC_PROFILE_COUNT) {
                    if (g_dtec.selected_profile == idx) confirm_vehicle();
                    else { dtec_obd_apply_profile(idx); dtec_audio_play(DTEC_SFX_MOVE); }
                }
                return;
            }
        }
        if (ty >= 164 && ty <= 188) {
            if (tx >= 4 && tx < 62) car_page = (car_page + max_page) % (max_page + 1);
            else if (tx >= 132 && tx < 188) car_page = (car_page + 1) % (max_page + 1);
            else if (tx >= 190 && tx < 252) { confirm_vehicle(); return; }
            else return;
            int idx = car_page * 4 + g_dtec.selected_profile % 4;
            dtec_obd_apply_profile(idx < DTEC_PROFILE_COUNT ? idx : DTEC_PROFILE_COUNT - 1);
            dtec_audio_play(DTEC_SFX_MOVE);
        }
        return;
    }
    bool codes = g_dtec.screen_mode == DTEC_MODE_CODES;
    if (ty >= 28 && ty < 52 && (codes || g_dtec.screen_mode == DTEC_MODE_ECU_INFO)) {
        if (tx >= 4 && tx < 126) dtec_handle_keys(KEY_A);
        else if (tx >= 130 && tx < 252) dtec_handle_keys(codes ? KEY_Y : KEY_R);
        return;
    }
    if (ty >= 28 && ty < 52 && g_dtec.screen_mode == DTEC_MODE_SENSORS) {
        if (tx >= 4 && tx < 84) dtec_handle_keys(KEY_UP);
        else if (tx >= 88 && tx < 168) dtec_handle_keys(KEY_DOWN);
        else if (tx >= 172 && tx < 252) dtec_handle_keys(KEY_R);
        return;
    }
    if (ty >= 28 && ty < 52) {
        int button = (tx - 4) / 63;
        if (tx < 4 || tx >= 252 || (tx - 4) % 63 >= 59) return;
        if (button == 0) dtec_handle_keys(KEY_START);
        else if (button == 1) dtec_handle_keys(KEY_B);
        else if (button == 2) dtec_handle_keys(KEY_Y);
        else { dtec_log("User requested reconnect"); dtec_net_reconnect(); dtec_audio_play(DTEC_SFX_CONFIRM); }
    }
}

static void button(uint16_t *buf, int x, int y, int w, int h, const char *label, uint16_t color) {
    gfx_panel(buf, x, y, w, h, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);
    gfx_draw_str_clipped(buf, x + (w - (int)strlen(label) * 8) / 2, y + (h - 8) / 2, w - 4, label, color, 1);
}

static void render_code_list(uint16_t *buf) {
    static const char *const kinds[] = {"STORED", "PENDING", "PERM"};
    static const uint16_t colors[] = {DTEC_COLOR_RED, DTEC_COLOR_YELLOW, DTEC_COLOR_ORANGE};
    gfx_panel(buf, 4, 57, 248, 131, DTEC_COLOR_BLACK, DTEC_COLOR_BORDER);
    if (g_dtec.dtc_count == 0) {
        const char *msg = g_dtec.scanning ? "READING ECU..." : !g_dtec.codes_read ? "A / SCAN TO READ CODES" : "NO TROUBLE CODES";
        gfx_draw_str(buf, 128 - (int)strlen(msg) * 4, 118, msg, g_dtec.codes_read ? DTEC_COLOR_GREEN : DTEC_COLOR_GRAY, 1);
    }
    for (int i = 0; i < g_dtec.dtc_count; i++) {
        char code[8], line[24];
        int kind = g_dtec.dtc_kind[i] % 3;
        dtec_obd_format_dtc(g_dtec.dtc[i], code);
        snprintf(line, sizeof(line), "%s %s", code, kinds[kind]);
        gfx_draw_str(buf, 12 + (i / 8) * 120, 64 + (i % 8) * 15, line, colors[kind], 1);
    }
}

void dtec_render_bottom(uint16_t *buf) {
    if (!buf) return;
    gfx_clear(buf, DTEC_COLOR_BG);
    gfx_background(buf);
    button(buf, 1, 2, 48, 22, "<", DTEC_COLOR_CYAN);
    button(buf, 207, 2, 48, 22, ">", DTEC_COLOR_CYAN);
    gfx_panel(buf, 51, 2, 154, 22, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER_HL);
    const char *name = s_mode_names[g_dtec.screen_mode % DTEC_MODE_COUNT];
    gfx_draw_str(buf, 118 - (int)strlen(name) * 4, 9, name, DTEC_COLOR_WHITE, 1);
    gfx_gear(buf, 184, 6, DTEC_COLOR_CYAN);

    if (g_dtec.screen_mode == DTEC_MODE_SETTINGS) {
        gfx_draw_str(buf, 10, 30, "AUDIO PREFERENCES", DTEC_COLOR_GRAY, 1);
        gfx_panel(buf, 8, 42, 240, 51, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER_HL);
        gfx_draw_str(buf, 16, 52, "SOUND EFFECTS", DTEC_COLOR_WHITE, 1);
        gfx_draw_str(buf, 16, 73, "MENU + CONNECTION CUES", DTEC_COLOR_GRAY, 1);
        gfx_panel(buf, 188, 48, 52, 20, g_dtec.sound_enabled ? DTEC_RGB(2, 13, 11) : DTEC_COLOR_DARK_GRAY,
                  g_dtec.sound_enabled ? DTEC_COLOR_GREEN : DTEC_COLOR_BORDER);
        gfx_draw_str(buf, 198, 54, g_dtec.sound_enabled ? "ON" : "OFF", g_dtec.sound_enabled ? DTEC_COLOR_GREEN : DTEC_COLOR_GRAY, 1);
        button(buf, 8, 105, 240, 30, "X / TEST SOUND", g_dtec.sound_enabled ? DTEC_COLOR_CYAN : DTEC_COLOR_GRAY);
        gfx_draw_str(buf, 12, 142, g_dtec.settings_saved ? "SAVED TO SD CARD" :
            (g_dtec.settings_session_only ? "SESSION ONLY / SD UNAVAILABLE" : "A / TOGGLE SOUND"), DTEC_COLOR_GRAY, 1);
        button(buf, 8, 158, 240, 30, "B / BACK", DTEC_COLOR_WHITE);
        return;
    }
    if (g_dtec.screen_mode == DTEC_MODE_CAR_SELECT) {
        int car_page = g_dtec.selected_profile / 4;
        int max_page = (DTEC_PROFILE_COUNT - 1) / 4;
        for (int r = 0; r < 4; r++) {
            int idx = car_page * 4 + r;
            int ry = 28 + r * 34;
            if (idx >= DTEC_PROFILE_COUNT) continue;
            bool selected = g_dtec.selected_profile == idx;
            gfx_panel(buf, 4, ry, 248, 30, DTEC_COLOR_PANEL, selected ? DTEC_COLOR_CYAN : DTEC_COLOR_BORDER);
            if (selected) gfx_rect_fill(buf, 5, ry + 7, 2, 16, DTEC_COLOR_CYAN);
            const DTEC_CarProfile *p = &g_dtec_profiles[idx];
            gfx_draw_str_clipped(buf, 12, ry + 4, 168, p->name, selected ? DTEC_COLOR_WHITE : DTEC_COLOR_GRAY, 1);
            gfx_draw_str_clipped(buf, 12, ry + 16, 168, p->desc, DTEC_COLOR_GRAY, 1);
            gfx_draw_str_clipped(buf, 188, ry + 10, 60, p->protocol_cmd ? p->protocol_cmd : "AUTO", DTEC_COLOR_CYAN, 1);
        }
        button(buf, 4, 164, 58, 24, "< PREV", DTEC_COLOR_WHITE);
        char page[16]; snprintf(page, sizeof(page), "%d/%d", car_page + 1, max_page + 1);
        gfx_draw_str(buf, 80, 172, page, DTEC_COLOR_CYAN, 1);
        button(buf, 132, 164, 56, 24, "NEXT >", DTEC_COLOR_WHITE);
        button(buf, 190, 164, 62, 24, "CONNECT", DTEC_COLOR_GREEN);
        return;
    }
    bool codes = g_dtec.screen_mode == DTEC_MODE_CODES;
    if (codes) {
        button(buf, 4, 28, 122, 24, g_dtec.scanning ? "SCANNING..." : "A / SCAN", DTEC_COLOR_GREEN);
        button(buf, 130, 28, 122, 24, g_dtec.clear_armed ? "Y / CONFIRM" : "Y / CLEAR",
               g_dtec.clear_armed ? DTEC_COLOR_RED : DTEC_COLOR_ORANGE);
        render_code_list(buf);
        return;
    }
    const char *rec = g_dtec.logging ? "R / STOP" : "R / REC";
    uint16_t rec_color = g_dtec.logging ? DTEC_COLOR_RED : DTEC_COLOR_WHITE;
    if (g_dtec.screen_mode == DTEC_MODE_SENSORS) {
        button(buf, 4, 28, 80, 24, "UP", DTEC_COLOR_WHITE);
        button(buf, 88, 28, 80, 24, "DOWN", DTEC_COLOR_WHITE);
        button(buf, 172, 28, 80, 24, rec, rec_color);
    } else if (g_dtec.screen_mode == DTEC_MODE_ECU_INFO) {
        button(buf, 4, 28, 122, 24, g_dtec.scanning ? "SCANNING..." : "A / SCAN", DTEC_COLOR_GREEN);
        button(buf, 130, 28, 122, 24, rec, rec_color);
    } else {
        button(buf, 4, 28, 59, 24, g_dtec.auto_poll ? "PAUSE" : "POLL", g_dtec.auto_poll ? DTEC_COLOR_GREEN : DTEC_COLOR_GRAY);
        button(buf, 67, 28, 59, 24, "RPM", DTEC_COLOR_WHITE);
        button(buf, 130, 28, 59, 24, "MAP", DTEC_COLOR_WHITE);
        button(buf, 193, 28, 59, 24, "LINK", DTEC_COLOR_CYAN);
    }
    gfx_panel(buf, 4, 57, 248, 51, DTEC_COLOR_PANEL, DTEC_COLOR_BORDER);
    uint16_t link_color = g_dtec.net_status == NET_OBD_CONNECTED ? DTEC_COLOR_GREEN : DTEC_COLOR_YELLOW;
    gfx_circle_fill(buf, 14, 68, 3, link_color);
    gfx_draw_str_clipped(buf, 24, 64, 220, g_dtec.status_msg[0] ? g_dtec.status_msg : "Waiting for adapter", DTEC_COLOR_WHITE, 1);
    char line[40];
    snprintf(line, sizeof(line), "%s", g_dtec.wifi_ssid[0] ? g_dtec.wifi_ssid : "Searching for Wi-Fi");
    gfx_draw_str_clipped(buf, 12, 80, 230, line, DTEC_COLOR_CYAN, 1);
    if (g_dtec.logging) snprintf(line, sizeof(line), "REC %s %lu", g_dtec.log_file, (unsigned long)g_dtec.log_rows);
    else snprintf(line, sizeof(line), "%s:%u", g_dtec.target_ip, g_dtec.target_port);
    gfx_draw_str_clipped(buf, 12, 94, 230, line, g_dtec.logging ? DTEC_COLOR_RED : DTEC_COLOR_GRAY, 1);
    gfx_panel(buf, 4, 113, 248, 75, DTEC_COLOR_BLACK, DTEC_COLOR_BORDER);
    gfx_draw_str(buf, 12, 118, "LIVE / OBD-II", DTEC_COLOR_GRAY, 1);
    snprintf(line, sizeof(line), "RX %lu", (unsigned long)g_dtec.lines_rx);
    gfx_draw_str_clipped(buf, 172, 118, 72, line, DTEC_COLOR_CYAN, 1);
    int first = g_dtec.log_count > 5 ? g_dtec.log_count - 5 : 0;
    for (int i = first; i < g_dtec.log_count; i++) {
        uint16_t col = g_dtec.log_lines[i][0] == '>' ? DTEC_COLOR_GREEN : DTEC_COLOR_GRAY;
        if (strstr(g_dtec.log_lines[i], "41")) col = DTEC_COLOR_CYAN;
        gfx_draw_str_clipped(buf, 12, 132 + (i - first) * 10, 232, g_dtec.log_lines[i], col, 1);
    }
}
