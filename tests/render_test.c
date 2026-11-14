#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dtec_gfx.h"
#include "dtec_obd.h"
#include "dtec_audio.h"

DTEC_State g_dtec;
bool dtec_net_is_connected(void) { return true; }
void dtec_net_send_cmd(const char *cmd) { (void)cmd; }
void dtec_net_reconnect(void) {}
void dtec_audio_play(DTEC_Sound sound) { (void)sound; }
void dtec_settings_toggle_sound(void) {}
void dtec_logger_toggle(void) {}
void dtec_render_top(uint16_t *);
void dtec_render_bottom(uint16_t *);

static void save_ppm(const char *directory, int mode, const char *screen, const uint16_t *buf) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%02d-%s.ppm", directory, mode, screen);
    FILE *f = fopen(path, "wb"); assert(f);
    fprintf(f, "P6\n256 192\n255\n");
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        unsigned char rgb[] = {(buf[i] & 31) * 255 / 31, ((buf[i] >> 5) & 31) * 255 / 31, ((buf[i] >> 10) & 31) * 255 / 31};
        assert(fwrite(rgb, 1, 3, f) == 3);
    }
    assert(fclose(f) == 0);
}

// Screens whose bottom is one full-height panel or dial rather than content + bottom row.
static bool layout_exempt(int mode) {
    return mode == DTEC_MODE_CAR_SELECT || mode == DTEC_MODE_OBD_TERMINAL || mode == DTEC_MODE_SIM_DASH ||
           mode == DTEC_MODE_FORZA_STYLE || mode == DTEC_MODE_BEAMNG_STYLE;
}

// Nothing may draw in the gap between content and the bottom row.
static void check_bottom_gap(const uint16_t *buf, int mode, int scenario) {
    if (layout_exempt(mode)) return;
    static uint16_t ref[SCREEN_W * SCREEN_H];
    gfx_clear(ref, DTEC_COLOR_BLACK);
    if (mode != DTEC_MODE_BIG_SPEED) gfx_background(ref);
    for (int y = UI_CONTENT_END; y < UI_FOOT_Y; y++)
        for (int x = 0; x < SCREEN_W; x++)
            if (buf[y * SCREEN_W + x] != ref[y * SCREEN_W + x]) {
                fprintf(stderr, "mode %d scenario %d draws into bottom gap at %d,%d\n", mode, scenario, x, y);
                exit(1);
            }
}

int main(int argc, char **argv) {
    uint16_t *buf = malloc(SCREEN_W * SCREEN_H * sizeof(*buf)); assert(buf);
    for (int scenario = 0; scenario < 3; scenario++) {
        memset(&g_dtec, 0, sizeof(g_dtec)); dtec_obd_init();
        if (scenario) {
            const char *reply = "41 0C 36 B0\r41 0D 64\r41 05 84\r41 11 70\r41 04 84\r13.8V\r";
            dtec_obd_feed_rx((const uint8_t *)reply, strlen(reply));
            g_dtec.net_status = NET_OBD_CONNECTED;
            snprintf(g_dtec.status_msg, sizeof(g_dtec.status_msg), "OBD Link Active");
            snprintf(g_dtec.wifi_ssid, sizeof(g_dtec.wifi_ssid), "WiFi_OBDII");
            snprintf(g_dtec.target_ip, sizeof(g_dtec.target_ip), "192.168.0.10");
            g_dtec.target_port = 35000; g_dtec.sound_enabled = true; g_dtec.settings_saved = true;
            const char *diag = "41 0F 46\r41 10 0D 7A\r41 31 04 D2\r41 21 00 0C\r41 42 37 14\r"
                "42 02 00 01 33\r42 04 00 45\r42 05 00 84\r42 0C 00 1F 40\r42 0D 00 64\r";
            dtec_obd_feed_rx((const uint8_t *)diag, strlen(diag));
            g_dtec.codes_read = g_dtec.has_monitors = g_dtec.mil_on = true;
            g_dtec.mon_supported = 0x7F; g_dtec.mon_incomplete = 0x10;
            static const uint16_t codes[] = {0x0133, 0x0420, 0x0171, 0x0420};
            static const uint8_t kinds[] = {0, 0, 1, 2};
            for (g_dtec.dtc_count = 0; g_dtec.dtc_count < 4; g_dtec.dtc_count++) {
                g_dtec.dtc[g_dtec.dtc_count] = codes[g_dtec.dtc_count]; g_dtec.dtc_kind[g_dtec.dtc_count] = kinds[g_dtec.dtc_count];
            }
            snprintf(g_dtec.vin, sizeof(g_dtec.vin), "1HGCM82633A004352");
            snprintf(g_dtec.protocol, sizeof(g_dtec.protocol), "AUTO, ISO 15765-4 (CAN 11/500)");
            const char *info = "41 1C 01\r41 51 01\r41 A6 00 12 D6 87\r";
            dtec_obd_feed_rx((const uint8_t *)info, strlen(info));
            snprintf(g_dtec.ecu_name, sizeof(g_dtec.ecu_name), "ECM-EngineControl");
            snprintf(g_dtec.cal_id, sizeof(g_dtec.cal_id), "37805RBBA530");
            g_dtec.misfire_cyls = 0xF; g_dtec.misfire_drive[2] = 14; g_dtec.misfire_avg[2] = 6;
            g_dtec.logging = true; g_dtec.log_rows = 1234;
            snprintf(g_dtec.log_file, sizeof(g_dtec.log_file), "dsobd-log-003.csv");
        }
        if (scenario == 2) {
            g_dtec.rpm = 16383; g_dtec.speed_kmh = 255; g_dtec.coolant_c = -40;
            g_dtec.has_boost = true; g_dtec.boost_kpa = 154;
            g_dtec.hist_count = GRAPH_HISTORY_LEN;
            for (int i = 0; i < GRAPH_HISTORY_LEN; i++) { g_dtec.hist_rpm[i] = 16000; g_dtec.hist_tps[i] = 100; g_dtec.hist_boost[i] = 154; }
            for (int i = 0; i < DTEC_PID_COUNT; i++) {
                g_dtec.pid_has[i] = g_dtec.ff_has[i] = true;
                g_dtec.pid_val[i] = g_dtec.ff_val[i] = 65535 * g_dtec_pids[i].scale + g_dtec_pids[i].offset;
            }
            g_dtec.ecu_dtc_count = 127; g_dtec.ff_dtc = 0xFFFF; g_dtec.mon_supported = 0x7FF;
            for (g_dtec.dtc_count = 0; g_dtec.dtc_count < DTEC_MAX_DTCS; g_dtec.dtc_count++) {
                g_dtec.dtc[g_dtec.dtc_count] = (uint16_t)(0xFFFF - g_dtec.dtc_count); g_dtec.dtc_kind[g_dtec.dtc_count] = g_dtec.dtc_count % 3;
            }
            memset(g_dtec.protocol, 'W', sizeof(g_dtec.protocol) - 1);
            memset(g_dtec.ecu_name, 'W', sizeof(g_dtec.ecu_name) - 1);
            memset(g_dtec.cal_id, 'W', sizeof(g_dtec.cal_id) - 1);
            g_dtec.misfire_cyls = 0xFFF; g_dtec.log_rows = UINT32_MAX;
            for (int c = 0; c < DTEC_MAX_CYLINDERS; c++) g_dtec.misfire_drive[c] = g_dtec.misfire_avg[c] = 65535;
        }
        for (int mode = 0; mode < DTEC_MODE_COUNT; mode++) {
            g_dtec.screen_mode = mode;
            // Settle the existing eased gauge needles before taking a snapshot.
            for (int frame = 0; frame < 16; frame++) dtec_render_top(buf);
            check_bottom_gap(buf, mode, scenario);
            if (argc > 1 && scenario == 1) save_ppm(argv[1], mode, "top", buf);
            dtec_render_bottom(buf);
            if (argc > 1 && scenario == 1) save_ppm(argv[1], mode, "bottom", buf);
        }
    }
    // Asset primitives must clip safely at every screen edge.
    gfx_dial(buf, 0, 0, 88);
    gfx_draw_char(buf, -10, -10, 'W', DTEC_COLOR_WHITE, 3);
    gfx_seg_number(buf, -20, -20, 64, 174, 14, 3, 255, DTEC_COLOR_WHITE, DTEC_COLOR_BLACK, 2);
    free(buf);
    printf("All %d modes rendered with missing, live and maximum telemetry; bottom-row gap and asset clipping passed\n", DTEC_MODE_COUNT);
}
