#include <stdio.h>
#include <string.h>

#include "dtec_common.h"
#include "dtec_net.h"
#include "dtec_obd.h"
#include "dtec_audio.h"
#include "dtec_settings.h"
#include "dtec_logger.h"
#include "gfx3ds.h"
#include "platform_3ds.h"

static const char *const s_mode_names[DTEC_MODE_COUNT] = {
    "VEHICLE", "MULTI", "DIGITAL", "BIG SPEED", "ARC TACH", "SIM LCD", "GT STYLE", "FORZA STYLE",
    "BEAMNG STYLE", "ANALOG", "LOGGER", "STATS", "CODES", "SENSORS", "ECU INFO", "TERMINAL", "SETTINGS"
};

static DTEC_ScreenMode s_previous_mode = DTEC_MODE_MULTI_GAUGE;

static void change_mode(int delta) {
    DTEC_ScreenMode next = (DTEC_ScreenMode)((g_dtec.screen_mode + DTEC_MODE_COUNT + delta) % DTEC_MODE_COUNT);
    if (next == DTEC_MODE_SETTINGS) s_previous_mode = g_dtec.screen_mode;
    g_dtec.screen_mode = next;
    g_dtec.clear_armed = false;
    dtec_audio_play(DTEC_SFX_MOVE);
}

static void toggle_settings(void) {
    if (g_dtec.screen_mode == DTEC_MODE_SETTINGS) {
        g_dtec.screen_mode = s_previous_mode;
    } else {
        s_previous_mode = g_dtec.screen_mode;
        g_dtec.screen_mode = DTEC_MODE_SETTINGS;
    }
    dtec_audio_play(DTEC_SFX_MOVE);
}

static void confirm_vehicle(void) {
    g_dtec.profile_confirmed = true;
    dtec_obd_apply_profile(g_dtec.selected_profile);
    char msg[48];
    snprintf(msg, sizeof(msg), "Car: %s", g_dtec_profiles[g_dtec.selected_profile].name);
    dtec_log(msg);
    dtec_net_reconnect();
    g_dtec.screen_mode = DTEC_MODE_MULTI_GAUGE;
    dtec_audio_play(DTEC_SFX_CONFIRM);
}

void dtec_3ds_handle_keys(uint32_t keys) {
    if (keys & (KEY_SELECT | KEY_RIGHT)) { change_mode(1); return; }
    if (keys & KEY_LEFT) { change_mode(-1); return; }

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
        if (keys & KEY_A) {
            dtec_obd_scan_codes(false);
            dtec_audio_play(DTEC_SFX_CONFIRM);
        }
        return;
    }

    if (g_dtec.screen_mode == DTEC_MODE_CODES) {
        if (keys & KEY_A) {
            dtec_obd_scan_codes(false);
            dtec_audio_play(DTEC_SFX_CONFIRM);
        } else if (keys & KEY_Y) {
            if (g_dtec.clear_armed) {
                g_dtec.clear_armed = false;
                dtec_obd_scan_codes(true);
            } else {
                g_dtec.clear_armed = true;
                dtec_log("Press CLEAR again to erase codes");
            }
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

void dtec_3ds_handle_touch(int tx, int ty, bool is_down) {
    if (!is_down || (unsigned)tx >= BOT_W || (unsigned)ty >= BOT_H) return;

    if (ty >= 4 && ty < 32) {
        if (tx >= 4 && tx < 58) change_mode(-1);
        else if (tx >= 262 && tx < 316) change_mode(1);
        else if (tx >= 62 && tx < 258) toggle_settings();
        return;
    }

    if (g_dtec.screen_mode == DTEC_MODE_SETTINGS) {
        if (tx >= 8 && tx < 312 && ty >= 42 && ty < 106) dtec_settings_toggle_sound();
        else if (tx >= 8 && tx < 312 && ty >= 114 && ty < 152) dtec_audio_play(DTEC_SFX_CONNECTED);
        else if (tx >= 8 && tx < 312 && ty >= 188 && ty < 230) toggle_settings();
        return;
    }

    if (g_dtec.screen_mode == DTEC_MODE_CAR_SELECT) {
        int car_page = g_dtec.selected_profile / 4;
        int max_page = (DTEC_PROFILE_COUNT - 1) / 4;
        for (int r = 0; r < 4; r++) {
            int ry = 38 + r * 38;
            if (tx >= 6 && tx < 314 && ty >= ry && ty < ry + 34) {
                int idx = car_page * 4 + r;
                if (idx < DTEC_PROFILE_COUNT) {
                    if (g_dtec.selected_profile == idx) confirm_vehicle();
                    else { dtec_obd_apply_profile(idx); dtec_audio_play(DTEC_SFX_MOVE); }
                }
                return;
            }
        }
        if (ty >= 194 && ty < 232) {
            if (tx >= 6 && tx < 80) car_page = (car_page + max_page) % (max_page + 1);
            else if (tx >= 148 && tx < 222) car_page = (car_page + 1) % (max_page + 1);
            else if (tx >= 228 && tx < 314) { confirm_vehicle(); return; }
            else return;
            int idx = car_page * 4 + g_dtec.selected_profile % 4;
            dtec_obd_apply_profile(idx < DTEC_PROFILE_COUNT ? idx : DTEC_PROFILE_COUNT - 1);
            dtec_audio_play(DTEC_SFX_MOVE);
        }
        return;
    }

    if (ty >= 38 && ty < 68) {
        if (g_dtec.screen_mode == DTEC_MODE_CODES) {
            if (tx >= 6 && tx < 157) dtec_3ds_handle_keys(KEY_A);
            else if (tx >= 163 && tx < 314) dtec_3ds_handle_keys(KEY_Y);
        } else if (g_dtec.screen_mode == DTEC_MODE_ECU_INFO) {
            if (tx >= 6 && tx < 157) dtec_3ds_handle_keys(KEY_A);
            else if (tx >= 163 && tx < 314) dtec_3ds_handle_keys(KEY_R);
        } else if (g_dtec.screen_mode == DTEC_MODE_SENSORS) {
            if (tx >= 6 && tx < 105) dtec_3ds_handle_keys(KEY_UP);
            else if (tx >= 110 && tx < 210) dtec_3ds_handle_keys(KEY_DOWN);
            else if (tx >= 215 && tx < 314) dtec_3ds_handle_keys(KEY_R);
        } else {
            if (tx >= 6 && tx < 79) dtec_3ds_handle_keys(KEY_START);
            else if (tx >= 84 && tx < 157) dtec_3ds_handle_keys(KEY_B);
            else if (tx >= 162 && tx < 235) dtec_3ds_handle_keys(KEY_Y);
            else if (tx >= 240 && tx < 314) {
                dtec_log("User requested reconnect");
                dtec_net_reconnect();
                dtec_audio_play(DTEC_SFX_CONFIRM);
            }
        }
    }
}

static void render_code_list(void) {
    static const char *const kinds[] = {"STORED", "PENDING", "PERM"};
    const u32 colors[] = {CLR_RED, CLR_YELLOW, CLR_ORANGE};

    ui_panel(6.0f, 74.0f, 308.0f, 158.0f, CLR_BLACK, CLR_BORDER);
    if (g_dtec.dtc_count == 0) {
        const char *msg = g_dtec.scanning ? "READING ECU..." :
            !g_dtec.codes_read ? "TAP [A / SCAN] TO READ CODES" : "NO TROUBLE CODES";
        ui_text(160.0f, 144.0f, 0.56f, g_dtec.codes_read ? CLR_GREEN : CLR_GRAY, C2D_AlignCenter, msg);
        return;
    }

    for (int i = 0; i < g_dtec.dtc_count; i++) {
        char code[8];
        int kind = g_dtec.dtc_kind[i] % 3;
        dtec_obd_format_dtc(g_dtec.dtc[i], code);
        float lx = 16.0f + (float)(i / 8) * 148.0f;
        float ly = 84.0f + (float)(i % 8) * 18.0f;
        ui_textf(lx, ly, 0.50f, colors[kind], 0, "%s  %s", code, kinds[kind]);
    }
}

void dtec_3ds_render_bottom(void) {
    ui_draw_deck_bottom();

    /* Top navigation bar */
    ui_button(4.0f, 4.0f, 54.0f, 28.0f, "<", CLR_CYAN, false);
    ui_panel(62.0f, 4.0f, 196.0f, 28.0f, CLR_PANEL, CLR_BORDER_HL);
    const char *name = s_mode_names[g_dtec.screen_mode % DTEC_MODE_COUNT];
    ui_text(152.0f, 10.0f, 0.54f, CLR_WHITE, C2D_AlignCenter, name);
    ui_draw_gear(234.0f, 9.0f, 18.0f, CLR_CYAN);
    ui_button(262.0f, 4.0f, 54.0f, 28.0f, ">", CLR_CYAN, false);

    if (g_dtec.screen_mode == DTEC_MODE_SETTINGS) {
        ui_text(12.0f, 36.0f, 0.44f, CLR_GRAY, 0, "AUDIO PREFERENCES");
        ui_panel(8.0f, 42.0f, 304.0f, 64.0f, CLR_PANEL, CLR_BORDER_HL);
        ui_text(18.0f, 52.0f, 0.58f, CLR_WHITE, 0, "SOUND EFFECTS");
        ui_text(18.0f, 76.0f, 0.46f, CLR_GRAY, 0, "MENU + CONNECTION CUES");
        ui_panel(236.0f, 56.0f, 64.0f, 26.0f,
                 g_dtec.sound_enabled ? CLR_GREEN_BG : CLR_DARK_GRAY,
                 g_dtec.sound_enabled ? CLR_GREEN : CLR_BORDER);
        ui_text(268.0f, 61.0f, 0.54f, g_dtec.sound_enabled ? CLR_GREEN : CLR_GRAY,
                C2D_AlignCenter, g_dtec.sound_enabled ? "ON" : "OFF");

        ui_button(8.0f, 114.0f, 304.0f, 38.0f, "X / TEST SOUND",
                  g_dtec.sound_enabled ? CLR_CYAN : CLR_GRAY, false);
        ui_text(160.0f, 162.0f, 0.48f, CLR_GRAY, C2D_AlignCenter,
                g_dtec.settings_saved ? "SAVED TO sdmc:/dsobd-settings.ini" :
                (g_dtec.settings_session_only ? "SESSION ONLY / SD UNAVAILABLE" : "A / TOGGLE SOUND"));
        ui_button(8.0f, 188.0f, 304.0f, 42.0f, "B / BACK", CLR_WHITE, false);
        return;
    }

    if (g_dtec.screen_mode == DTEC_MODE_CAR_SELECT) {
        int car_page = g_dtec.selected_profile / 4;
        int max_page = (DTEC_PROFILE_COUNT - 1) / 4;
        for (int r = 0; r < 4; r++) {
            int idx = car_page * 4 + r;
            float ry = 38.0f + (float)r * 38.0f;
            if (idx >= DTEC_PROFILE_COUNT) continue;
            bool selected = (g_dtec.selected_profile == idx);
            ui_panel(6.0f, ry, 308.0f, 34.0f, selected ? CLR_PANEL_HI : CLR_PANEL,
                     selected ? CLR_CYAN : CLR_BORDER);
            if (selected) C2D_DrawRectSolid(8.0f, ry + 6.0f, 0.0f, 3.0f, 22.0f, CLR_CYAN);
            const DTEC_CarProfile *p = &g_dtec_profiles[idx];
            ui_text_clipped(16.0f, ry + 3.0f, 210.0f, 0.50f, selected ? CLR_WHITE : CLR_GRAY, p->name);
            ui_text_clipped(16.0f, ry + 18.0f, 210.0f, 0.42f, CLR_GRAY, p->desc);
            ui_text(306.0f, ry + 10.0f, 0.48f, CLR_CYAN, C2D_AlignRight,
                    p->protocol_cmd ? p->protocol_cmd : "AUTO");
        }
        ui_button(6.0f, 194.0f, 74.0f, 38.0f, "< PREV", CLR_WHITE, false);
        ui_textf(114.0f, 204.0f, 0.56f, CLR_CYAN, C2D_AlignCenter, "%d/%d", car_page + 1, max_page + 1);
        ui_button(148.0f, 194.0f, 74.0f, 38.0f, "NEXT >", CLR_WHITE, false);
        ui_button(228.0f, 194.0f, 86.0f, 38.0f, "CONNECT", CLR_GREEN, true);
        return;
    }

    if (g_dtec.screen_mode == DTEC_MODE_CODES) {
        ui_button(6.0f, 38.0f, 151.0f, 30.0f, g_dtec.scanning ? "SCANNING..." : "A / SCAN", CLR_GREEN, false);
        ui_button(163.0f, 38.0f, 151.0f, 30.0f, g_dtec.clear_armed ? "Y / CONFIRM" : "Y / CLEAR",
                  g_dtec.clear_armed ? CLR_RED : CLR_ORANGE, g_dtec.clear_armed);
        render_code_list();
        return;
    }

    const char *rec = g_dtec.logging ? "R / STOP" : "R / REC";
    u32 rec_col = g_dtec.logging ? CLR_RED : CLR_WHITE;
    if (g_dtec.screen_mode == DTEC_MODE_SENSORS) {
        ui_button(6.0f, 38.0f, 99.0f, 30.0f, "UP", CLR_WHITE, false);
        ui_button(110.0f, 38.0f, 100.0f, 30.0f, "DOWN", CLR_WHITE, false);
        ui_button(215.0f, 38.0f, 99.0f, 30.0f, rec, rec_col, g_dtec.logging);
    } else if (g_dtec.screen_mode == DTEC_MODE_ECU_INFO) {
        ui_button(6.0f, 38.0f, 151.0f, 30.0f, g_dtec.scanning ? "SCANNING..." : "A / SCAN", CLR_GREEN, false);
        ui_button(163.0f, 38.0f, 151.0f, 30.0f, rec, rec_col, g_dtec.logging);
    } else {
        ui_button(6.0f, 38.0f, 73.0f, 30.0f, g_dtec.auto_poll ? "PAUSE" : "POLL",
                  g_dtec.auto_poll ? CLR_GREEN : CLR_GRAY, g_dtec.auto_poll);
        ui_button(84.0f, 38.0f, 73.0f, 30.0f, "RPM", CLR_WHITE, false);
        ui_button(162.0f, 38.0f, 73.0f, 30.0f, "MAP", CLR_WHITE, false);
        ui_button(240.0f, 38.0f, 74.0f, 30.0f, "LINK", CLR_CYAN, false);
    }

    /* Status & Network Panel */
    ui_panel(6.0f, 74.0f, 308.0f, 62.0f, CLR_PANEL, CLR_BORDER);
    u32 link_col = g_dtec.net_status == NET_OBD_CONNECTED ? CLR_GREEN : CLR_YELLOW;
    C2D_DrawCircleSolid(20.0f, 88.0f, 0.0f, 4.0f, link_col);
    ui_text_clipped(32.0f, 80.0f, 274.0f, 0.54f, CLR_WHITE,
                    g_dtec.status_msg[0] ? g_dtec.status_msg : "Waiting for adapter");
    ui_text_clipped(16.0f, 100.0f, 288.0f, 0.48f, CLR_CYAN,
                    g_dtec.wifi_ssid[0] ? g_dtec.wifi_ssid : "3DS WiFi");
    if (g_dtec.logging) {
        ui_textf(16.0f, 116.0f, 0.46f, CLR_RED, 0, "REC %s (%lu rows)",
                 g_dtec.log_file, (unsigned long)g_dtec.log_rows);
    } else {
        ui_textf(16.0f, 116.0f, 0.46f, CLR_GRAY, 0, "%s:%u  •  Local %s",
                 g_dtec.target_ip, (unsigned)g_dtec.target_port,
                 g_dtec.local_ip[0] ? g_dtec.local_ip : "--");
    }

    /* Live OBD-II Log Panel */
    ui_panel(6.0f, 142.0f, 308.0f, 90.0f, CLR_BLACK, CLR_BORDER);
    ui_text(14.0f, 147.0f, 0.44f, CLR_GRAY, 0, "LIVE / OBD-II");
    ui_textf(306.0f, 147.0f, 0.44f, CLR_CYAN, C2D_AlignRight, "RX %lu", (unsigned long)g_dtec.lines_rx);
    int first = g_dtec.log_count > 5 ? g_dtec.log_count - 5 : 0;
    for (int i = first; i < g_dtec.log_count; i++) {
        u32 col = g_dtec.log_lines[i][0] == '>' ? CLR_GREEN : CLR_GRAY;
        if (strstr(g_dtec.log_lines[i], "41")) col = CLR_CYAN;
        ui_text_clipped(14.0f, 162.0f + (float)(i - first) * 13.0f, 292.0f, 0.44f, col, g_dtec.log_lines[i]);
    }
}
