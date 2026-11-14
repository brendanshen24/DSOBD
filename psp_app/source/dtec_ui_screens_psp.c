#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pspctrl.h>
#include <pspkernel.h>

#include "ui_internal_psp.h"
#include "dtec_obd.h"
#include "dtec_net.h"
#include "dtec_audio.h"
#include "dtec_settings.h"
#include "dtec_logger.h"
#include "ini.h"

/* Screen navigation & interactive widget state */
static DTEC_ScreenMode s_prev_mode = DTEC_MODE_MULTI_GAUGE;
static bool s_quick_menu = false;
static int s_quick_cursor = 0;

static int s_dtc_scroll = 0;
static int s_term_cmd = 0;
static int s_setting_row = 0;

static int s_scope_cursor = 0;
static bool s_scope_scrub = false;

static bool s_wifi_editing = false;
static int s_ip_field = 0; /* 0..3 = octets, 4 = port */
static int s_ip_octets[4] = {192, 168, 0, 10};
static int s_edit_port = 35000;

static uint32_t s_analog_cooldown = 0;
static bool s_was_over_shift = false;

static const char *const s_term_cmds[] = {
    "ATZ", "010C", "010D", "010B", "0105", "ATRV", "0100", "ATDP", "0133"
};
static const char *const s_term_labels[] = {
    "ATZ  Reset", "010C RPM", "010D Speed", "010B MAP", "0105 Temp",
    "ATRV Volts", "0100 PIDs", "ATDP Proto", "0133 Baro"
};
#define TERM_CMD_COUNT ((int)(sizeof(s_term_cmds) / sizeof(s_term_cmds[0])))

#define SETTINGS_ROW_COUNT 6

static void apply_psp_pref_kv(const char *key, const char *val, void *user) {
    (void)user;
    if (strcasecmp(key, "shift_rpm") == 0) {
        int r = atoi(val);
        if (r >= 5000 && r <= 8500) g_psp.shift_rpm = (uint16_t)r;
    } else if (strcasecmp(key, "shift_beep") == 0) {
        g_psp.shift_beep = ini_parse_bool(val);
    } else if (strcasecmp(key, "imperial") == 0) {
        g_psp.use_imperial = ini_parse_bool(val);
    } else if (strcasecmp(key, "keep_awake") == 0) {
        g_psp.keep_awake = ini_parse_bool(val);
    } else if (strcasecmp(key, "clean_hud") == 0) {
        g_psp.clean_hud = ini_parse_bool(val);
    }
}

void psp_prefs_load(void) {
    g_psp.shift_rpm = 7000;
    g_psp.shift_beep = true;
    g_psp.use_imperial = false;
    g_psp.keep_awake = true;
    g_psp.clean_hud = false;

    const char *paths[] = {"dsobd-psp.ini", "ms0:/PSP/GAME/DSOBD/dsobd-psp.ini", "ms0:/dsobd-psp.ini"};
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        if (ini_parse_file(paths[i], apply_psp_pref_kv, NULL)) break;
    }
}

bool psp_prefs_save(void) {
    FILE *f = fopen("ms0:/PSP/GAME/DSOBD/dsobd-psp.ini", "w");
    if (!f) f = fopen("dsobd-psp.ini", "w");
    if (!f) f = fopen("ms0:/dsobd-psp.ini", "w");
    if (!f) return false;
    fprintf(f, "shift_rpm=%u\n", (unsigned)g_psp.shift_rpm);
    fprintf(f, "shift_beep=%d\n", g_psp.shift_beep ? 1 : 0);
    fprintf(f, "imperial=%d\n", g_psp.use_imperial ? 1 : 0);
    fprintf(f, "keep_awake=%d\n", g_psp.keep_awake ? 1 : 0);
    fprintf(f, "clean_hud=%d\n", g_psp.clean_hud ? 1 : 0);
    fclose(f);
    return true;
}

static const char *enum_text(uint8_t pid, int v) {
    static const char *const standards[] = {
        NULL, "OBD-II CARB", "OBD EPA", "OBD+OBD-II", "OBD-I", "NON-OBD",
        "EOBD", "EOBD+OBD-II", "EOBD+OBD", "EOBD+OBD+II", "JOBD", "JOBD+OBD-II", "JOBD+EOBD", "JOBD+EOBD+II"
    };
    static const char *const fuels[] = {
        NULL, "GASOLINE", "METHANOL", "ETHANOL", "DIESEL", "LPG", "CNG", "PROPANE", "ELECTRIC"
    };
    if (pid == 0x1C) return (v > 0 && v < 14) ? standards[v] : NULL;
    if (v > 0 && v < 9) return fuels[v];
    return ((v >= 9 && v <= 16) || v == 23) ? "BI-FUEL" : (v >= 17 && v <= 22 ? "HYBRID" : NULL);
}

void psp_pid_text(int i, bool freeze, char *out, size_t n) {
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
    if (g_psp.use_imperial) {
        if (strcmp(p->unit, "C") == 0) {
            snprintf(out, n, "%.*f °F", p->decimals, v * 1.8f + 32.0f);
            return;
        }
        if (strcmp(p->unit, "km/h") == 0) {
            snprintf(out, n, "%.0f mph", v * 0.621371f);
            return;
        }
        if (strcmp(p->unit, "kPa") == 0) {
            snprintf(out, n, "%.1f psi", v * 0.145038f);
            return;
        }
        if (strcmp(p->unit, "km") == 0) {
            snprintf(out, n, "%.0f mi", v * 0.621371f);
            return;
        }
    } else if (strcmp(p->unit, "C") == 0) {
        snprintf(out, n, "%.*f °C", p->decimals, v);
        return;
    }
    snprintf(out, n, "%.*f %s", p->decimals, v, p->unit);
}

void psp_render_car_select(void) {
    psp_draw_header("VEHICLE PROFILES");

    int sel = g_dtec.selected_profile;
    if (sel < 0 || sel >= DTEC_PROFILE_COUNT) sel = 0;

    const int visible = 6;
    int first = sel - visible / 2;
    if (first < 0) first = 0;
    if (first > DTEC_PROFILE_COUNT - visible) first = DTEC_PROFILE_COUNT - visible;
    if (first < 0) first = 0;

    ui_panel(8.0f, 29.0f, 254.0f, 213.0f, CLR_PANEL_DARK, CLR_BORDER);
    for (int r = 0; r < visible && first + r < DTEC_PROFILE_COUNT; r++) {
        int idx = first + r;
        float ry = 34.0f + (float)r * 34.0f;
        bool is_sel = (idx == sel);
        const DTEC_CarProfile *p = &g_dtec_profiles[idx];

        ui_panel(13.0f, ry, 244.0f, 31.0f, is_sel ? CLR_PANEL_HI : CLR_PANEL,
                 is_sel ? CLR_CYAN : CLR_BORDER);
        if (is_sel) ui_rect_solid(15.0f, ry + 5.0f, 3.0f, 21.0f, CLR_CYAN);

        ui_text_clipped(23.0f, ry + 4.0f, 168.0f, 0.62f, is_sel ? CLR_WHITE : CLR_GRAY, p->name);
        ui_text_clipped(23.0f, ry + 17.0f, 168.0f, 0.50f, CLR_GRAY, p->desc);
        ui_text(249.0f, ry + 9.0f, 0.56f, CLR_CYAN, UI_ALIGN_RIGHT,
                p->protocol_cmd ? p->protocol_cmd : "AUTO");
    }

    const DTEC_CarProfile *sp = &g_dtec_profiles[sel];
    ui_panel(268.0f, 29.0f, 204.0f, 213.0f, CLR_PANEL, CLR_BORDER);
    ui_text(278.0f, 36.0f, 0.72f, CLR_CYAN, UI_ALIGN_LEFT, sp->brand);
    ui_text_clipped(278.0f, 56.0f, 184.0f, 0.76f, CLR_WHITE, sp->name);
    ui_text_clipped(278.0f, 76.0f, 184.0f, 0.56f, CLR_GRAY, sp->desc);

    ui_rect_solid(278.0f, 96.0f, 184.0f, 1.0f, CLR_BORDER);
    ui_textf(278.0f, 104.0f, 0.58f, CLR_WHITE, UI_ALIGN_LEFT, "PROTOCOL : %s",
             sp->protocol_cmd ? sp->protocol_cmd : "AUTO");
    ui_textf(278.0f, 121.0f, 0.58f, CLR_WHITE, UI_ALIGN_LEFT, "HEADER   : %s",
             sp->header_cmd ? sp->header_cmd : "BROADCAST");

    ui_rect_solid(278.0f, 140.0f, 184.0f, 1.0f, CLR_BORDER);
    ui_text(278.0f, 148.0f, 0.56f, CLR_CYAN, UI_ALIGN_LEFT, "PSP WI-FI LINK");
    ui_text_clipped(278.0f, 164.0f, 184.0f, 0.56f, CLR_WHITE,
                    g_dtec.wifi_ssid[0] ? g_dtec.wifi_ssid : "--");
    ui_textf(278.0f, 180.0f, 0.54f, CLR_GRAY, UI_ALIGN_LEFT, "%s:%u",
             g_dtec.target_ip, (unsigned)g_dtec.target_port);

    ui_panel(278.0f, 204.0f, 184.0f, 28.0f, CLR_GREEN_BG, CLR_GREEN);
    ui_text(370.0f, 211.0f, 0.66f, CLR_GREEN, UI_ALIGN_CENTER, "[X] APPLY & CONNECT");

    psp_draw_footer("VEHICLE");
}

void psp_render_codes(int dtc_scroll) {
    psp_draw_header("TROUBLE CODES");

    bool mil = g_dtec.has_monitors && g_dtec.mil_on;
    ui_panel(8.0f, 29.0f, 464.0f, 36.0f, CLR_PANEL,
             g_dtec.clear_armed ? CLR_RED : (mil ? CLR_ORANGE : CLR_BORDER));
    ui_draw_mil(16.0f, 35.0f, 24.0f, mil ? CLR_ORANGE : CLR_DARK_GRAY);

    ui_text(48.0f, 33.0f, 0.72f, mil ? CLR_ORANGE : CLR_WHITE, UI_ALIGN_LEFT,
            g_dtec.scanning ? "READING ECU..." :
            !g_dtec.has_monitors ? "NOT SCANNED" :
            mil ? "CHECK ENGINE ON" : "CHECK ENGINE OFF");

    int counts[3] = {0};
    for (int i = 0; i < g_dtec.dtc_count; i++) counts[g_dtec.dtc_kind[i] % 3]++;
    ui_textf(48.0f, 50.0f, 0.54f, CLR_GRAY, UI_ALIGN_LEFT,
             "%d STORED  •  %d PENDING  •  %d PERM", counts[0], counts[1], counts[2]);

    if (g_dtec.clear_armed) {
        ui_panel(312.0f, 34.0f, 152.0f, 26.0f, CLR_RED_BG, CLR_RED);
        ui_text(388.0f, 41.0f, 0.58f, CLR_RED, UI_ALIGN_CENTER, "PRESS [^] TO ERASE!");
    } else {
        ui_panel(354.0f, 34.0f, 110.0f, 26.0f, CLR_PANEL_HI, CLR_BORDER_HL);
        ui_text(409.0f, 41.0f, 0.58f, CLR_CYAN, UI_ALIGN_CENTER, "[X] SCAN ECU");
    }

    /* Left: Readiness + Vehicle Identity */
    ui_panel(8.0f, 70.0f, 148.0f, 172.0f, CLR_PANEL, CLR_BORDER);
    int total = __builtin_popcount(g_dtec.mon_supported);
    int ready = __builtin_popcount(g_dtec.mon_supported & ~g_dtec.mon_incomplete);
    if (g_dtec.has_monitors) {
        ui_textf(16.0f, 78.0f, 0.58f, ready == total ? CLR_GREEN : CLR_YELLOW, UI_ALIGN_LEFT,
                 "READY  %d / %d", ready, total);
        ui_hbar(16.0f, 94.0f, 132.0f, 5.0f, total ? (float)ready / (float)total : 0.0f,
                ready == total ? CLR_GREEN : CLR_YELLOW, CLR_DARK_GRAY);
    } else {
        ui_text(16.0f, 78.0f, 0.58f, CLR_GRAY, UI_ALIGN_LEFT, "READY  --");
        ui_hbar(16.0f, 94.0f, 132.0f, 5.0f, 0.0f, CLR_GRAY, CLR_DARK_GRAY);
    }

    char a[24], b[24], line[64];
    snprintf(line, sizeof(line), "VIN: %s", g_dtec.vin[0] ? g_dtec.vin : "--");
    ui_text_clipped(16.0f, 108.0f, 132.0f, 0.54f, CLR_WHITE, line);
    ui_text_clipped(16.0f, 128.0f, 132.0f, 0.52f, CLR_GRAY,
                    g_dtec.protocol[0] ? g_dtec.protocol : "PROTO --");
    psp_pid_text(dtec_obd_pid_index(0x31), false, a, sizeof(a));
    psp_pid_text(dtec_obd_pid_index(0x21), false, b, sizeof(b));
    snprintf(line, sizeof(line), "CLR: %s", a);
    ui_text_clipped(16.0f, 152.0f, 132.0f, 0.54f, CLR_GRAY, line);
    snprintf(line, sizeof(line), "MIL: %s", b);
    ui_text_clipped(16.0f, 172.0f, 132.0f, 0.54f, CLR_GRAY, line);
    ui_text(16.0f, 206.0f, 0.48f, CLR_GRAY, UI_ALIGN_LEFT, "Engine OFF to clear");

    /* Middle: Unified Scrollable Trouble Code List (PSP single-screen specialization) */
    ui_panel(162.0f, 70.0f, 162.0f, 172.0f, CLR_BLACK, CLR_BORDER);
    ui_text(170.0f, 78.0f, 0.58f, CLR_CYAN, UI_ALIGN_LEFT, "DTC LIST");
    if (g_dtec.dtc_count == 0) {
        const char *msg = g_dtec.scanning ? "SCANNING..." :
            !g_dtec.codes_read ? "PRESS [X] TO SCAN" : "NO CODES";
        ui_text(243.0f, 148.0f, 0.62f, g_dtec.codes_read ? CLR_GREEN : CLR_GRAY, UI_ALIGN_CENTER, msg);
    } else {
        static const char *const kinds[] = {"STORED", "PENDING", "PERM"};
        const uint32_t kcols[] = {CLR_RED, CLR_YELLOW, CLR_ORANGE};
        const int vis = 7;
        int start = dtc_scroll;
        if (start > g_dtec.dtc_count - vis) start = g_dtec.dtc_count - vis;
        if (start < 0) start = 0;
        for (int r = 0; r < vis && start + r < g_dtec.dtc_count; r++) {
            int idx = start + r;
            char code[8];
            int kind = g_dtec.dtc_kind[idx] % 3;
            dtec_obd_format_dtc(g_dtec.dtc[idx], code);
            float ry = 100.0f + (float)r * 20.0f;
            ui_text(170.0f, ry, 0.64f, kcols[kind], UI_ALIGN_LEFT, code);
            ui_text(316.0f, ry, 0.54f, kcols[kind], UI_ALIGN_RIGHT, kinds[kind]);
        }
    }

    /* Right: Freeze Frame */
    ui_panel(330.0f, 70.0f, 142.0f, 172.0f, CLR_PANEL, CLR_BORDER);
    if (g_dtec.ff_dtc) {
        dtec_obd_format_dtc(g_dtec.ff_dtc, a);
        ui_textf(338.0f, 78.0f, 0.58f, CLR_CYAN, UI_ALIGN_LEFT, "FREEZE %s", a);
    } else {
        ui_text(338.0f, 78.0f, 0.58f, CLR_CYAN, UI_ALIGN_LEFT, "FREEZE NONE");
    }
    for (int i = 0, row = 0; i < DTEC_PID_COUNT && row < 6; i++) {
        if (!g_dtec.ff_has[i]) continue;
        float ry = 102.0f + (float)(row++) * 22.0f;
        ui_text_clipped(338.0f, ry, 72.0f, 0.52f, CLR_GRAY, g_dtec_pids[i].name);
        psp_pid_text(i, true, a, sizeof(a));
        ui_text(464.0f, ry, 0.56f, CLR_WHITE, UI_ALIGN_RIGHT, a);
    }

    psp_draw_footer("CODES");
}

void psp_render_sensors(void) {
    psp_draw_header("LIVE SENSORS");

    int list[DTEC_PID_COUNT];
    int n = dtec_obd_sensor_list(list);
    int pages = n ? (n + DTEC_SENSOR_ROWS - 1) / DTEC_SENSOR_ROWS : 1;
    int page = g_dtec.sensor_page % pages;
    char v[48];

    for (int r = 0; r < DTEC_SENSOR_ROWS && page * DTEC_SENSOR_ROWS + r < n; r++) {
        int i = list[page * DTEC_SENSOR_ROWS + r];
        float x = (r < 4) ? 8.0f : 244.0f;
        float y = 29.0f + (float)(r % 4) * 48.0f;
        ui_panel(x, y, 228.0f, 44.0f, CLR_PANEL, CLR_BORDER);
        ui_text_clipped(x + 8.0f, y + 6.0f, 168.0f, 0.54f, CLR_GRAY, g_dtec_pids[i].name);
        ui_textf(x + 220.0f, y + 6.0f, 0.50f, CLR_CYAN, UI_ALIGN_RIGHT, "01%02X", g_dtec_pids[i].pid);
        psp_pid_text(i, false, v, sizeof(v));
        ui_text(x + 220.0f, y + 22.0f, 0.74f, g_dtec.pid_has[i] ? CLR_WHITE : CLR_GRAY, UI_ALIGN_RIGHT, v);
    }

    if (n == 0) {
        ui_panel(8.0f, 29.0f, 464.0f, 188.0f, CLR_PANEL, CLR_BORDER);
        ui_text(240.0f, 114.0f, 0.74f, CLR_GRAY, UI_ALIGN_CENTER,
                "WAITING FOR SUPPORTED PID BITMAP (0100)");
    }

    ui_textf(10.0f, 226.0f, 0.60f, CLR_CYAN, UI_ALIGN_LEFT,
             "PAGE %d / %d   •   %d PIDS SUPPORTED", page + 1, pages, n);
    ui_text(470.0f, 226.0f, 0.58f, CLR_GRAY, UI_ALIGN_RIGHT,
            "[UP/DOWN] PAGE   •   [[]] LOG CSV");

    psp_draw_footer("SENSORS");
}

void psp_render_ecu_info(void) {
    psp_draw_header("ECU INFO");

    char line[64], v[32];
    ui_panel(8.0f, 29.0f, 228.0f, 213.0f, CLR_PANEL, CLR_BORDER);
    ui_text(18.0f, 36.0f, 0.64f, CLR_CYAN, UI_ALIGN_LEFT, "ECU IDENTITY");

    const char *rows[][2] = {{"VIN", g_dtec.vin}, {"ECU", g_dtec.ecu_name}, {"CAL", g_dtec.cal_id}};
    for (int r = 0; r < 3; r++) {
        snprintf(line, sizeof(line), "%s: %s", rows[r][0], rows[r][1][0] ? rows[r][1] : "--");
        ui_text_clipped(18.0f, 60.0f + (float)r * 24.0f, 208.0f, 0.60f, CLR_WHITE, line);
    }

    psp_pid_text(dtec_obd_pid_index(0x1C), false, v, sizeof(v));
    snprintf(line, sizeof(line), "OBD: %s", v);
    ui_text_clipped(18.0f, 134.0f, 208.0f, 0.58f, CLR_GRAY, line);

    psp_pid_text(dtec_obd_pid_index(0x51), false, v, sizeof(v));
    snprintf(line, sizeof(line), "FUEL: %s", v);
    ui_text_clipped(18.0f, 156.0f, 208.0f, 0.58f, CLR_GRAY, line);

    psp_pid_text(dtec_obd_pid_index(0xA6), false, v, sizeof(v));
    snprintf(line, sizeof(line), "ODO: %s", v);
    ui_text_clipped(18.0f, 178.0f, 208.0f, 0.58f, CLR_GRAY, line);

    snprintf(line, sizeof(line), "PROTO: %s", g_dtec.protocol[0] ? g_dtec.protocol : "--");
    ui_text_clipped(18.0f, 200.0f, 208.0f, 0.54f, CLR_CYAN, line);

    ui_panel(244.0f, 29.0f, 228.0f, 213.0f, CLR_PANEL, CLR_BORDER);
    ui_text(254.0f, 36.0f, 0.64f, CLR_CYAN, UI_ALIGN_LEFT, "MISFIRE MONITORS (M06)");
    if (!g_dtec.misfire_cyls) {
        const char *msg = g_dtec.scanning ? "READING ECU..." :
            !g_dtec.codes_read ? "PRESS [X] TO SCAN" : "NOT REPORTED";
        ui_text(358.0f, 128.0f, 0.64f, CLR_GRAY, UI_ALIGN_CENTER, msg);
    } else {
        ui_text(254.0f, 58.0f, 0.52f, CLR_GRAY, UI_ALIGN_LEFT, "CYL  DRV  AVG      CYL  DRV  AVG");
        for (int c = 0, k = 0; c < DTEC_MAX_CYLINDERS && k < 12; c++) {
            if (!(g_dtec.misfire_cyls & (1u << c))) continue;
            float cx = 254.0f + (float)(k / 6) * 108.0f;
            float cy = 78.0f + (float)(k % 6) * 24.0f;
            ui_textf(cx, cy, 0.56f, g_dtec.misfire_drive[c] ? CLR_ORANGE : CLR_WHITE, UI_ALIGN_LEFT,
                     "C%-2d  %3u  %3u", c + 1, g_dtec.misfire_drive[c], g_dtec.misfire_avg[c]);
            k++;
        }
    }

    psp_draw_footer("ECU INFO");
}

void psp_render_terminal(int cmd_cursor) {
    psp_draw_header("ELM327 TERMINAL");

    ui_panel(8.0f, 29.0f, 322.0f, 213.0f, CLR_BLACK, CLR_BORDER);
    ui_text(16.0f, 35.0f, 0.58f, CLR_YELLOW, UI_ALIGN_LEFT, "LIVE OBD-II STREAM");
    ui_textf(322.0f, 35.0f, 0.54f, CLR_CYAN, UI_ALIGN_RIGHT, "RX %luB • TX %luB",
             (unsigned long)g_dtec.bytes_rx, (unsigned long)g_dtec.bytes_tx);

    for (int i = 0; i < OBD_LOG_LINES && i < g_dtec.log_count; i++) {
        float ly = 56.0f + (float)i * 16.5f;
        uint32_t col = CLR_WHITE;
        if (g_dtec.log_lines[i][0] == '>') col = CLR_GREEN;
        else if (strstr(g_dtec.log_lines[i], "41")) col = CLR_CYAN;
        ui_text_clipped(16.0f, ly, 306.0f, 0.56f, col, g_dtec.log_lines[i]);
    }
    ui_text_clipped(16.0f, 224.0f, 306.0f, 0.52f, CLR_GRAY,
                    g_dtec.elm_version[0] ? g_dtec.elm_version : "ELM327 awaiting handshake...");

    ui_panel(336.0f, 29.0f, 136.0f, 213.0f, CLR_PANEL, CLR_BORDER);
    ui_text(344.0f, 35.0f, 0.58f, CLR_CYAN, UI_ALIGN_LEFT, "SEND COMMAND");
    for (int i = 0; i < TERM_CMD_COUNT; i++) {
        float ry = 54.0f + (float)i * 20.5f;
        bool sel = (i == cmd_cursor);
        if (sel) {
            ui_panel(342.0f, ry - 1.0f, 124.0f, 19.0f, CLR_PANEL_HI, CLR_CYAN);
        }
        ui_text_clipped(348.0f, ry + 2.0f, 112.0f, 0.54f, sel ? CLR_WHITE : CLR_GRAY, s_term_labels[i]);
    }

    psp_draw_footer("TERMINAL");
}

static const char *apctl_state_str(int st) {
    switch (st) {
        case 0: return "DISCONNECTED";
        case 1: return "SCANNING";
        case 2: return "JOINING AP";
        case 3: return "GETTING IP";
        case 4: return "GOT IP";
        default: return "WLAN";
    }
}

void psp_render_wifi(bool editing_ip, int ip_field, const int ip_octets[4], int port_val) {
    psp_draw_header("PSP WI-FI LINK");

    ui_panel(8.0f, 29.0f, 464.0f, 30.0f, CLR_PANEL, g_psp.wlan_switch_on ? CLR_BORDER : CLR_RED);
    ui_draw_wifi(14.0f, 34.0f, 20.0f, g_psp.wlan_switch_on ? CLR_CYAN : CLR_RED);
    ui_textf(40.0f, 37.0f, 0.58f, g_psp.wlan_switch_on ? CLR_GREEN : CLR_RED, UI_ALIGN_LEFT,
             "WLAN %s", g_psp.wlan_switch_on ? "ON" : "OFF");
    ui_textf(122.0f, 37.0f, 0.56f, CLR_WHITE, UI_ALIGN_LEFT, "APCTL: %s",
             apctl_state_str(g_psp.apctl_state));
    ui_textf(280.0f, 37.0f, 0.56f, CLR_GRAY, UI_ALIGN_LEFT, "IP: %s",
             g_psp.local_ip[0] ? g_psp.local_ip : "--");
    ui_textf(464.0f, 37.0f, 0.56f, CLR_CYAN, UI_ALIGN_RIGHT, "SIG %d%%", g_psp.signal_strength);

    /* Left: XMB Saved NetParam Slots + OBD Adapter Presets */
    ui_panel(8.0f, 64.0f, 272.0f, 178.0f, CLR_PANEL_DARK, CLR_BORDER);
    ui_text(16.0f, 70.0f, 0.56f, CLR_CYAN, UI_ALIGN_LEFT, "SAVED XMB PROFILES & PRESETS");

    const int vis = 5;
    int first = g_psp.wifi_selected - vis / 2;
    if (first < 0) first = 0;
    if (first > g_psp.wifi_count - vis) first = g_psp.wifi_count - vis;
    if (first < 0) first = 0;

    for (int r = 0; r < vis && first + r < g_psp.wifi_count; r++) {
        int idx = first + r;
        const PspWifiItem *it = &g_psp.wifi_items[idx];
        float ry = 88.0f + (float)r * 30.0f;
        bool sel = (!editing_ip && idx == g_psp.wifi_selected);

        ui_panel(13.0f, ry, 262.0f, 27.0f, sel ? CLR_PANEL_HI : CLR_PANEL,
                 sel ? CLR_CYAN : CLR_BORDER);
        if (sel) ui_rect_solid(15.0f, ry + 5.0f, 3.0f, 17.0f, CLR_CYAN);
        ui_text_clipped(22.0f, ry + 3.0f, 152.0f, 0.58f, sel ? CLR_WHITE : CLR_GRAY, it->ssid);
        ui_text_clipped(22.0f, ry + 15.0f, 152.0f, 0.46f, CLR_GRAY, it->profile_name);
        if (it->netparam_id > 0 && !it->is_preset) {
            ui_textf(268.0f, ry + 7.0f, 0.52f, CLR_GREEN, UI_ALIGN_RIGHT, "XMB #%d", it->netparam_id);
        } else {
            ui_text(268.0f, ry + 7.0f, 0.52f, CLR_CYAN, UI_ALIGN_RIGHT, it->target_ip);
        }
    }

    /* Right: Selected Target + Inline IP/Port Editor */
    ui_panel(286.0f, 64.0f, 186.0f, 178.0f, CLR_PANEL, editing_ip ? CLR_CYAN : CLR_BORDER);
    if (editing_ip) {
        ui_text(294.0f, 70.0f, 0.60f, CLR_CYAN, UI_ALIGN_LEFT, "EDIT TARGET IP & PORT");
        ui_text(294.0f, 88.0f, 0.50f, CLR_GRAY, UI_ALIGN_LEFT, "[◄►] FIELD   [▲▼] VALUE");

        for (int f = 0; f < 4; f++) {
            float fx = 294.0f + (float)f * 43.0f;
            bool active = (ip_field == f);
            ui_panel(fx, 112.0f, 39.0f, 28.0f, active ? CLR_PANEL_HI : CLR_PANEL_DARK,
                     active ? CLR_CYAN : CLR_BORDER);
            ui_textf(fx + 19.5f, 120.0f, 0.66f, active ? CLR_WHITE : CLR_GRAY,
                     UI_ALIGN_CENTER, "%d", ip_octets[f]);
        }

        bool port_act = (ip_field == 4);
        ui_text(294.0f, 152.0f, 0.54f, CLR_GRAY, UI_ALIGN_LEFT, "TCP PORT");
        ui_panel(294.0f, 168.0f, 170.0f, 28.0f, port_act ? CLR_PANEL_HI : CLR_PANEL_DARK,
                 port_act ? CLR_CYAN : CLR_BORDER);
        ui_textf(379.0f, 176.0f, 0.68f, port_act ? CLR_WHITE : CLR_GRAY,
                 UI_ALIGN_CENTER, "%d", port_val);

        ui_text(379.0f, 212.0f, 0.56f, CLR_GREEN, UI_ALIGN_CENTER, "[X] APPLY   [O] CANCEL");
    } else {
        ui_text(294.0f, 70.0f, 0.58f, CLR_CYAN, UI_ALIGN_LEFT, "ACTIVE TARGET");
        ui_text_clipped(294.0f, 90.0f, 170.0f, 0.64f, CLR_WHITE,
                        g_dtec.wifi_ssid[0] ? g_dtec.wifi_ssid : "--");
        ui_textf(294.0f, 110.0f, 0.58f, CLR_CYAN, UI_ALIGN_LEFT, "%s:%u",
                 g_dtec.target_ip, (unsigned)g_dtec.target_port);
        ui_text_clipped(294.0f, 130.0f, 170.0f, 0.52f, CLR_GRAY,
                        g_dtec.status_msg[0] ? g_dtec.status_msg : "Idle");

        ui_panel(294.0f, 162.0f, 170.0f, 30.0f, CLR_GREEN_BG, CLR_GREEN);
        ui_text(379.0f, 171.0f, 0.62f, CLR_GREEN, UI_ALIGN_CENTER, "[X] CONNECT SELECTED");

        ui_panel(294.0f, 198.0f, 170.0f, 30.0f, CLR_PANEL_DARK, CLR_BORDER_HL);
        ui_text(379.0f, 207.0f, 0.60f, CLR_CYAN, UI_ALIGN_CENTER, "[^] EDIT IP / PORT");
    }

    psp_draw_footer("WI-FI LINK");
}

void psp_render_settings(int setting_row) {
    psp_draw_header("PSP SETTINGS");

    ui_panel(8.0f, 29.0f, 266.0f, 213.0f, CLR_PANEL, CLR_BORDER);
    ui_text(18.0f, 35.0f, 0.60f, CLR_CYAN, UI_ALIGN_LEFT, "HARDWARE & CLUSTER CONFIG");

    const char *labels[SETTINGS_ROW_COUNT] = {
        "SOUND EFFECTS",
        "SHIFT LIGHT RPM",
        "SHIFT TONE ALERT",
        "DISPLAY UNITS",
        "KEEP SCREEN AWAKE",
        "RECONNECT ADAPTER"
    };
    char vals[SETTINGS_ROW_COUNT][24];
    uint32_t vcols[SETTINGS_ROW_COUNT];

    snprintf(vals[0], 24, "%s", g_dtec.sound_enabled ? "ON" : "OFF");
    vcols[0] = g_dtec.sound_enabled ? CLR_GREEN : CLR_GRAY;

    snprintf(vals[1], 24, "%u RPM", (unsigned)g_psp.shift_rpm);
    vcols[1] = CLR_YELLOW;

    snprintf(vals[2], 24, "%s", g_psp.shift_beep ? "ON" : "OFF");
    vcols[2] = g_psp.shift_beep ? CLR_GREEN : CLR_GRAY;

    snprintf(vals[3], 24, "%s", g_psp.use_imperial ? "IMPERIAL" : "METRIC");
    vcols[3] = CLR_CYAN;

    snprintf(vals[4], 24, "%s", g_psp.keep_awake ? "ON" : "OFF");
    vcols[4] = g_psp.keep_awake ? CLR_GREEN : CLR_GRAY;

    snprintf(vals[5], 24, "LINK NOW");
    vcols[5] = CLR_CYAN;

    for (int i = 0; i < SETTINGS_ROW_COUNT; i++) {
        float ry = 55.0f + (float)i * 30.0f;
        bool sel = (i == setting_row);
        ui_panel(16.0f, ry, 250.0f, 27.0f, sel ? CLR_PANEL_HI : CLR_PANEL_DARK,
                 sel ? CLR_CYAN : CLR_BORDER);
        if (sel) ui_rect_solid(18.0f, ry + 5.0f, 3.0f, 17.0f, CLR_CYAN);
        ui_text(26.0f, ry + 7.0f, 0.56f, sel ? CLR_WHITE : CLR_GRAY, UI_ALIGN_LEFT, labels[i]);
        ui_text(258.0f, ry + 7.0f, 0.58f, vcols[i], UI_ALIGN_RIGHT, vals[i]);
    }

    ui_panel(280.0f, 29.0f, 192.0f, 213.0f, CLR_PANEL, CLR_BORDER);
    ui_draw_gear(290.0f, 35.0f, 16.0f, CLR_CYAN);
    ui_text(312.0f, 36.0f, 0.60f, CLR_CYAN, UI_ALIGN_LEFT, "PSP SYSTEM");

    ui_text(290.0f, 60.0f, 0.54f, CLR_WHITE, UI_ALIGN_LEFT, "L / R     : Switch Screen");
    ui_text(290.0f, 76.0f, 0.54f, CLR_WHITE, UI_ALIGN_LEFT, "SELECT    : Quick View Grid");
    ui_text(290.0f, 92.0f, 0.54f, CLR_WHITE, UI_ALIGN_LEFT, "START     : Toggle Auto-Poll");
    ui_text(290.0f, 108.0f, 0.54f, CLR_WHITE, UI_ALIGN_LEFT, "SQUARE    : Record CSV Log");
    ui_text(290.0f, 124.0f, 0.54f, CLR_WHITE, UI_ALIGN_LEFT, "CIRCLE    : Clean Cockpit HUD");
    ui_text(290.0f, 140.0f, 0.54f, CLR_WHITE, UI_ALIGN_LEFT, "TRIANGLE  : Test Audio / Peak");

    ui_rect_solid(290.0f, 162.0f, 172.0f, 1.0f, CLR_BORDER);
    ui_text(290.0f, 170.0f, 0.52f, CLR_GRAY, UI_ALIGN_LEFT, "CPU: 333 MHz  GU: 60 Hz");
    ui_text(290.0f, 186.0f, 0.52f, g_dtec.settings_saved ? CLR_GREEN : CLR_YELLOW, UI_ALIGN_LEFT,
            g_dtec.settings_saved ? "MS0 SETTINGS SAVED" :
            (g_dtec.settings_session_only ? "SESSION ONLY (NO MS0)" : "MS0 READY"));
    ui_text(290.0f, 202.0f, 0.50f, CLR_GRAY, UI_ALIGN_LEFT, "CSV: ms0:/dsobd-log-XXX.csv");
    ui_text(290.0f, 218.0f, 0.50f, CLR_GRAY, UI_ALIGN_LEFT, "INI: ms0:/ (dsobd.ini)");

    psp_draw_footer("SETTINGS");
}

static void render_quick_menu(void) {
    ui_rect_solid(0.0f, 0.0f, PSP_SCR_W, PSP_SCR_H, PSP_RGBA(2, 6, 14, 196));
    ui_panel(26.0f, 24.0f, 428.0f, 224.0f, CLR_PANEL, CLR_CYAN);
    ui_text(240.0f, 32.0f, 0.72f, CLR_CYAN, UI_ALIGN_CENTER, "PSP QUICK VIEW LAUNCHER");

    for (int i = 0; i < PSP_MODE_COUNT; i++) {
        int col = i % 3;
        int row = i / 3;
        float bx = 38.0f + (float)col * 136.0f;
        float by = 56.0f + (float)row * 27.0f;
        bool cur = (i == (int)g_dtec.screen_mode);
        bool hov = (i == s_quick_cursor);
        uint32_t bg = hov ? CLR_PANEL_HI : (cur ? CLR_GREEN_BG : CLR_PANEL_DARK);
        uint32_t bd = hov ? CLR_CYAN : (cur ? CLR_GREEN : CLR_BORDER);
        ui_panel(bx, by, 130.0f, 23.0f, bg, bd);
        ui_text(bx + 65.0f, by + 5.0f, 0.56f,
                hov ? CLR_WHITE : (cur ? CLR_GREEN : CLR_GRAY),
                UI_ALIGN_CENTER, g_psp_mode_names[i]);
    }

    ui_text(240.0f, 224.0f, 0.56f, CLR_WHITE, UI_ALIGN_CENTER,
            "[D-PAD] PICK   •   [X] OPEN   •   [O / SEL] CLOSE");
}

static void change_mode(int delta) {
    int next = ((int)g_dtec.screen_mode + PSP_MODE_COUNT + delta) % PSP_MODE_COUNT;
    if (next == DTEC_MODE_SETTINGS) s_prev_mode = g_dtec.screen_mode;
    g_dtec.screen_mode = (DTEC_ScreenMode)next;
    g_dtec.clear_armed = false;
    s_wifi_editing = false;
    s_scope_scrub = false;
    if (next == (int)PSP_MODE_WIFI) psp_net_refresh_list();
    dtec_audio_play(DTEC_SFX_MOVE);
}

static void confirm_vehicle(void) {
    g_dtec.profile_confirmed = true;
    dtec_obd_apply_profile(g_dtec.selected_profile);
    char msg[48];
    snprintf(msg, sizeof(msg), "Car: %s", g_dtec_profiles[g_dtec.selected_profile].name);
    dtec_log(msg);
    psp_net_save_ini();
    dtec_net_reconnect();
    g_dtec.screen_mode = DTEC_MODE_MULTI_GAUGE;
    dtec_audio_play(DTEC_SFX_CONFIRM);
}

static void enter_ip_editor(void) {
    int a = 192, b = 168, c = 0, d = 10;
    if (sscanf(g_dtec.target_ip, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
        s_ip_octets[0] = a; s_ip_octets[1] = b; s_ip_octets[2] = c; s_ip_octets[3] = d;
    }
    s_edit_port = g_dtec.target_port ? g_dtec.target_port : 35000;
    s_ip_field = 0;
    s_wifi_editing = true;
}

void psp_ui_init(void) {
    psp_prefs_load();
    s_quick_menu = false;
    s_quick_cursor = (int)g_dtec.screen_mode;
    s_was_over_shift = false;
}

void psp_ui_fini(void) {
    psp_prefs_save();
}

void psp_ui_handle_input(uint32_t k_down, uint32_t k_held, int8_t lx, int8_t ly) {
    (void)k_held;

    if (s_analog_cooldown > 0) s_analog_cooldown--;

    /* Quick View Launcher overlay via SELECT */
    if (k_down & PSP_CTRL_SELECT) {
        s_quick_menu = !s_quick_menu;
        if (s_quick_menu) s_quick_cursor = (int)g_dtec.screen_mode;
        dtec_audio_play(DTEC_SFX_MOVE);
        return;
    }

    if (s_quick_menu) {
        int col = s_quick_cursor % 3;
        int row = s_quick_cursor / 3;
        bool moved = false;

        if ((k_down & PSP_CTRL_LEFT) || (s_analog_cooldown == 0 && lx < -64)) {
            col = (col + 2) % 3; moved = true; s_analog_cooldown = 10;
        } else if ((k_down & PSP_CTRL_RIGHT) || (s_analog_cooldown == 0 && lx > 64)) {
            col = (col + 1) % 3; moved = true; s_analog_cooldown = 10;
        }
        if ((k_down & PSP_CTRL_UP) || (s_analog_cooldown == 0 && ly < -64)) {
            row = (row + 5) % 6; moved = true; s_analog_cooldown = 10;
        } else if ((k_down & PSP_CTRL_DOWN) || (s_analog_cooldown == 0 && ly > 64)) {
            row = (row + 1) % 6; moved = true; s_analog_cooldown = 10;
        }
        if (moved) {
            int next = row * 3 + col;
            if (next >= PSP_MODE_COUNT) next = PSP_MODE_COUNT - 1;
            s_quick_cursor = next;
            dtec_audio_play(DTEC_SFX_MOVE);
        }
        if (k_down & PSP_CTRL_CROSS) {
            g_dtec.screen_mode = (DTEC_ScreenMode)s_quick_cursor;
            if (g_dtec.screen_mode == PSP_MODE_WIFI) psp_net_refresh_list();
            s_quick_menu = false;
            s_wifi_editing = false;
            s_scope_scrub = false;
            dtec_audio_play(DTEC_SFX_CONFIRM);
        } else if (k_down & PSP_CTRL_CIRCLE) {
            s_quick_menu = false;
            dtec_audio_play(DTEC_SFX_MOVE);
        }
        return;
    }

    /* Shoulder triggers L/R ALWAYS cycle screens cleanly */
    if (k_down & PSP_CTRL_LTRIGGER) { change_mode(-1); return; }
    if (k_down & PSP_CTRL_RTRIGGER) { change_mode(+1); return; }

    /* Wi-Fi Link screen */
    if (g_dtec.screen_mode == PSP_MODE_WIFI) {
        if (s_wifi_editing) {
            if (k_down & PSP_CTRL_LEFT) { s_ip_field = (s_ip_field + 4) % 5; dtec_audio_play(DTEC_SFX_MOVE); }
            if (k_down & PSP_CTRL_RIGHT) { s_ip_field = (s_ip_field + 1) % 5; dtec_audio_play(DTEC_SFX_MOVE); }
            if (k_down & (PSP_CTRL_UP | PSP_CTRL_DOWN)) {
                int delta = (k_down & PSP_CTRL_UP) ? 1 : -1;
                if (s_ip_field < 4) {
                    s_ip_octets[s_ip_field] = (s_ip_octets[s_ip_field] + 256 + delta) % 256;
                } else {
                    if (delta > 0) s_edit_port = (s_edit_port == 23) ? 35000 : s_edit_port + 1;
                    else s_edit_port = (s_edit_port <= 23) ? 35000 : s_edit_port - 1;
                }
                dtec_audio_play(DTEC_SFX_MOVE);
            }
            if (k_down & PSP_CTRL_CROSS) {
                char ipbuf[24];
                snprintf(ipbuf, sizeof(ipbuf), "%d.%d.%d.%d",
                         s_ip_octets[0], s_ip_octets[1], s_ip_octets[2], s_ip_octets[3]);
                s_wifi_editing = false;
                psp_net_set_target(ipbuf, (uint16_t)s_edit_port);
                dtec_audio_play(DTEC_SFX_CONFIRM);
            } else if (k_down & PSP_CTRL_CIRCLE) {
                s_wifi_editing = false;
                dtec_audio_play(DTEC_SFX_MOVE);
            }
            return;
        }
        if ((k_down & PSP_CTRL_UP) && g_psp.wifi_selected > 0) {
            g_psp.wifi_selected--;
            dtec_audio_play(DTEC_SFX_MOVE);
        }
        if ((k_down & PSP_CTRL_DOWN) && g_psp.wifi_selected + 1 < g_psp.wifi_count) {
            g_psp.wifi_selected++;
            dtec_audio_play(DTEC_SFX_MOVE);
        }
        if (k_down & PSP_CTRL_CROSS) {
            psp_net_connect_item(g_psp.wifi_selected);
            dtec_audio_play(DTEC_SFX_CONFIRM);
        } else if (k_down & PSP_CTRL_TRIANGLE) {
            enter_ip_editor();
            dtec_audio_play(DTEC_SFX_MOVE);
        } else if (k_down & PSP_CTRL_SQUARE) {
            psp_net_refresh_list();
            dtec_audio_play(DTEC_SFX_CONFIRM);
        } else if (k_down & PSP_CTRL_LEFT) {
            change_mode(-1);
        } else if (k_down & PSP_CTRL_RIGHT) {
            change_mode(1);
        }
        return;
    }

    /* Settings screen */
    if (g_dtec.screen_mode == DTEC_MODE_SETTINGS) {
        if ((k_down & PSP_CTRL_UP) && s_setting_row > 0) {
            s_setting_row--;
            dtec_audio_play(DTEC_SFX_MOVE);
        }
        if ((k_down & PSP_CTRL_DOWN) && s_setting_row + 1 < SETTINGS_ROW_COUNT) {
            s_setting_row++;
            dtec_audio_play(DTEC_SFX_MOVE);
        }
        if (k_down & PSP_CTRL_TRIANGLE) {
            dtec_audio_play(DTEC_SFX_CONNECTED);
        }
        if (k_down & PSP_CTRL_CIRCLE) {
            g_dtec.screen_mode = s_prev_mode;
            dtec_audio_play(DTEC_SFX_MOVE);
            return;
        }
        bool act = (k_down & (PSP_CTRL_CROSS | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) != 0;
        if (act) {
            switch (s_setting_row) {
                case 0: dtec_settings_toggle_sound(); break;
                case 1: {
                    int step = (k_down & PSP_CTRL_LEFT) ? -250 : 250;
                    int next = (int)g_psp.shift_rpm + step;
                    if (next > 8500) next = 5000;
                    if (next < 5000) next = 8500;
                    g_psp.shift_rpm = (uint16_t)next;
                    psp_prefs_save();
                    dtec_audio_play(DTEC_SFX_MOVE);
                    break;
                }
                case 2:
                    g_psp.shift_beep = !g_psp.shift_beep;
                    psp_prefs_save();
                    dtec_audio_play(DTEC_SFX_CONFIRM);
                    break;
                case 3:
                    g_psp.use_imperial = !g_psp.use_imperial;
                    psp_prefs_save();
                    dtec_audio_play(DTEC_SFX_CONFIRM);
                    break;
                case 4:
                    g_psp.keep_awake = !g_psp.keep_awake;
                    psp_prefs_save();
                    dtec_audio_play(DTEC_SFX_CONFIRM);
                    break;
                case 5:
                    dtec_net_reconnect();
                    dtec_audio_play(DTEC_SFX_CONFIRM);
                    break;
            }
        }
        return;
    }

    /* Vehicle profile selector */
    if (g_dtec.screen_mode == DTEC_MODE_CAR_SELECT) {
        int sel = g_dtec.selected_profile;
        if ((k_down & PSP_CTRL_UP) && sel > 0) sel--;
        if ((k_down & PSP_CTRL_DOWN) && sel < DTEC_PROFILE_COUNT - 1) sel++;
        if (sel != g_dtec.selected_profile) {
            dtec_obd_apply_profile(sel);
            dtec_audio_play(DTEC_SFX_MOVE);
        }
        if (k_down & (PSP_CTRL_CROSS | PSP_CTRL_START)) confirm_vehicle();
        else if (k_down & PSP_CTRL_TRIANGLE) {
            g_dtec.screen_mode = PSP_MODE_WIFI;
            psp_net_refresh_list();
            dtec_audio_play(DTEC_SFX_MOVE);
        } else if (k_down & PSP_CTRL_LEFT) change_mode(-1);
        else if (k_down & PSP_CTRL_RIGHT) change_mode(1);
        return;
    }

    /* Square = Global CSV Log Toggle on all gauge/diagnostic screens */
    if (k_down & PSP_CTRL_SQUARE) {
        dtec_logger_toggle();
        dtec_audio_play(DTEC_SFX_CONFIRM);
        return;
    }

    if (k_down & PSP_CTRL_START) {
        g_dtec.auto_poll = !g_dtec.auto_poll;
        dtec_log(g_dtec.auto_poll ? "Auto-Poll: ON" : "Auto-Poll: OFF");
        dtec_audio_play(DTEC_SFX_CONFIRM);
    }

    /* Screen-specific actions */
    if (g_dtec.screen_mode == DTEC_MODE_CODES) {
        if ((k_down & PSP_CTRL_UP) && s_dtc_scroll > 0) s_dtc_scroll--;
        if ((k_down & PSP_CTRL_DOWN) && s_dtc_scroll + 7 < g_dtec.dtc_count) s_dtc_scroll++;
        if (k_down & PSP_CTRL_CROSS) {
            s_dtc_scroll = 0;
            dtec_obd_scan_codes(false);
            dtec_audio_play(DTEC_SFX_CONFIRM);
        } else if (k_down & PSP_CTRL_TRIANGLE) {
            if (g_dtec.clear_armed) {
                g_dtec.clear_armed = false;
                dtec_obd_scan_codes(true);
            } else {
                g_dtec.clear_armed = true;
                dtec_log("Press [^] again to erase codes");
            }
            dtec_audio_play(DTEC_SFX_CONFIRM);
        } else if (k_down & PSP_CTRL_CIRCLE) {
            g_dtec.clear_armed = false;
            dtec_audio_play(DTEC_SFX_MOVE);
        } else if (k_down & PSP_CTRL_LEFT) change_mode(-1);
        else if (k_down & PSP_CTRL_RIGHT) change_mode(1);
        return;
    }

    if (g_dtec.screen_mode == DTEC_MODE_ECU_INFO) {
        if (k_down & PSP_CTRL_CROSS) {
            dtec_obd_scan_codes(false);
            dtec_audio_play(DTEC_SFX_CONFIRM);
        } else if (k_down & PSP_CTRL_LEFT) change_mode(-1);
        else if (k_down & PSP_CTRL_RIGHT) change_mode(1);
        return;
    }

    if (g_dtec.screen_mode == DTEC_MODE_SENSORS) {
        if (k_down & (PSP_CTRL_UP | PSP_CTRL_DOWN)) {
            int list[DTEC_PID_COUNT];
            int pages = (dtec_obd_sensor_list(list) + DTEC_SENSOR_ROWS - 1) / DTEC_SENSOR_ROWS;
            if (pages < 1) pages = 1;
            g_dtec.sensor_page = (g_dtec.sensor_page % pages + pages + ((k_down & PSP_CTRL_DOWN) ? 1 : -1)) % pages;
            dtec_audio_play(DTEC_SFX_MOVE);
        } else if (k_down & PSP_CTRL_LEFT) change_mode(-1);
        else if (k_down & PSP_CTRL_RIGHT) change_mode(1);
        return;
    }

    if (g_dtec.screen_mode == DTEC_MODE_OBD_TERMINAL) {
        if (k_down & PSP_CTRL_UP) {
            s_term_cmd = (s_term_cmd + TERM_CMD_COUNT - 1) % TERM_CMD_COUNT;
            dtec_audio_play(DTEC_SFX_MOVE);
        } else if (k_down & PSP_CTRL_DOWN) {
            s_term_cmd = (s_term_cmd + 1) % TERM_CMD_COUNT;
            dtec_audio_play(DTEC_SFX_MOVE);
        } else if (k_down & PSP_CTRL_CROSS) {
            dtec_obd_send_cmd(s_term_cmds[s_term_cmd]);
            dtec_audio_play(DTEC_SFX_CONFIRM);
        } else if (k_down & PSP_CTRL_CIRCLE) {
            dtec_obd_send_cmd("010C");
            dtec_audio_play(DTEC_SFX_MOVE);
        } else if (k_down & PSP_CTRL_TRIANGLE) {
            dtec_obd_send_cmd("ATRV");
            dtec_audio_play(DTEC_SFX_MOVE);
        } else if (k_down & PSP_CTRL_LEFT) change_mode(-1);
        else if (k_down & PSP_CTRL_RIGHT) change_mode(1);
        return;
    }

    if (g_dtec.screen_mode == DTEC_MODE_REALTIME_GRAPH) {
        if (g_dtec.hist_count > 0 && (lx < -40 || lx > 40)) {
            if (!s_scope_scrub) {
                s_scope_scrub = true;
                s_scope_cursor = g_dtec.hist_count - 1;
            }
            if (s_analog_cooldown == 0) {
                s_scope_cursor += (lx > 0) ? 1 : -1;
                if (s_scope_cursor < 0) s_scope_cursor = 0;
                if (s_scope_cursor >= g_dtec.hist_count) s_scope_cursor = g_dtec.hist_count - 1;
                s_analog_cooldown = (lx < -100 || lx > 100) ? 2 : 5;
            }
        }
        if (k_down & PSP_CTRL_TRIANGLE) {
            s_scope_scrub = false;
            dtec_audio_play(DTEC_SFX_MOVE);
        } else if (k_down & PSP_CTRL_CROSS) {
            g_dtec.hist_count = 0;
            s_scope_scrub = false;
            dtec_audio_play(DTEC_SFX_CONFIRM);
        } else if (k_down & PSP_CTRL_LEFT) change_mode(-1);
        else if (k_down & PSP_CTRL_RIGHT) change_mode(1);
        return;
    }

    /* Gauge screens: D-Pad L/R switches screens, Circle toggles Clean HUD, Triangle resets peaks, Cross manual burst */
    if (k_down & PSP_CTRL_LEFT) { change_mode(-1); return; }
    if (k_down & PSP_CTRL_RIGHT) { change_mode(1); return; }
    if (k_down & PSP_CTRL_CIRCLE) {
        g_psp.clean_hud = !g_psp.clean_hud;
        psp_prefs_save();
        dtec_audio_play(DTEC_SFX_MOVE);
    }
    if (k_down & PSP_CTRL_TRIANGLE) {
        g_dtec.peak_rpm = g_dtec.has_rpm ? g_dtec.rpm : 0.0f;
        g_dtec.peak_boost = g_dtec.has_boost ? g_dtec.boost_kpa : 0.0f;
        g_psp.peak_speed_kmh = g_dtec.has_speed ? g_dtec.speed_kmh : 0;
        g_psp.peak_coolant_c = g_dtec.has_coolant ? g_dtec.coolant_c : -999;
        dtec_audio_play(DTEC_SFX_CONFIRM);
    }
    if (k_down & PSP_CTRL_CROSS) {
        dtec_obd_send_cmd(g_dtec.has_voltage ? "010B" : "ATRV");
        dtec_audio_play(DTEC_SFX_MOVE);
    }
}

void psp_ui_render(void) {
    float rpm = g_dtec.has_rpm ? g_dtec.rpm : 0.0f;
    g_psp_rpm_ease += (rpm - g_psp_rpm_ease) * 0.35f;
    g_psp_hud_frame++;

    /* Track PSP-wide session peaks */
    if (g_dtec.has_speed && g_dtec.speed_kmh > g_psp.peak_speed_kmh) {
        g_psp.peak_speed_kmh = g_dtec.speed_kmh;
    }
    if (g_dtec.has_coolant && g_dtec.coolant_c > g_psp.peak_coolant_c) {
        g_psp.peak_coolant_c = g_dtec.coolant_c;
    }

    /* Shift tone alert on upward redline crossing */
    bool over_shift = g_dtec.has_rpm && (g_dtec.rpm >= (float)g_psp.shift_rpm);
    if (over_shift && !s_was_over_shift) {
        psp_audio_play_shift();
    }
    s_was_over_shift = over_shift;

    bool hud_black = g_dtec.screen_mode >= DTEC_MODE_DIGITAL_SPEED &&
                     g_dtec.screen_mode <= DTEC_MODE_BEAMNG_STYLE;

    gfx_psp_begin_frame(hud_black ? CLR_BLACK : CLR_BG);
    if (!hud_black) {
        ui_draw_deck();
    }

    switch ((int)g_dtec.screen_mode) {
        case DTEC_MODE_CAR_SELECT:     psp_render_car_select(); break;
        case DTEC_MODE_MULTI_GAUGE:    psp_render_multi_gauge(); break;
        case DTEC_MODE_DIGITAL_SPEED:  psp_render_digital_speed(); break;
        case DTEC_MODE_BIG_SPEED:      psp_render_big_speed(); break;
        case DTEC_MODE_ARC_TACH:       psp_render_arc_tach(); break;
        case DTEC_MODE_SIM_DASH:       psp_render_sim_dash(); break;
        case DTEC_MODE_GT_STYLE:       psp_render_gt_style(); break;
        case DTEC_MODE_FORZA_STYLE:    psp_render_forza_style(); break;
        case DTEC_MODE_BEAMNG_STYLE:   psp_render_beamng_style(); break;
        case DTEC_MODE_ANALOG_DIALS:   psp_render_analog_dials(); break;
        case DTEC_MODE_REALTIME_GRAPH: psp_render_realtime_graph(s_scope_cursor, s_scope_scrub); break;
        case DTEC_MODE_BOOST_SETUP:    psp_render_stats(); break;
        case DTEC_MODE_CODES:          psp_render_codes(s_dtc_scroll); break;
        case DTEC_MODE_SENSORS:        psp_render_sensors(); break;
        case DTEC_MODE_ECU_INFO:       psp_render_ecu_info(); break;
        case DTEC_MODE_OBD_TERMINAL:   psp_render_terminal(s_term_cmd); break;
        case DTEC_MODE_SETTINGS:       psp_render_settings(s_setting_row); break;
        case PSP_MODE_WIFI:            psp_render_wifi(s_wifi_editing, s_ip_field, s_ip_octets, s_edit_port); break;
        default: break;
    }

    if (s_quick_menu) {
        render_quick_menu();
    }

    gfx_psp_end_frame();
}
