#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "config.h"
#include "net.h"
#include "obd.h"
#include "ui.h"

AppState g_state;

static void state_init(void) {
    memset(&g_state, 0, sizeof(g_state));
    g_state.ui_mode = UI_MODE_TERMINAL;
    g_state.net_status = NET_STATUS_DISCONNECTED;
    strncpy(g_state.status_msg, "Initializing...", sizeof(g_state.status_msg) - 1);
    strncpy(g_state.wifi_ssid, DEFAULT_OBD_SSID, sizeof(g_state.wifi_ssid) - 1);
    strncpy(g_state.target_ip, DEFAULT_OBD_IP, sizeof(g_state.target_ip) - 1);
    g_state.target_port = DEFAULT_OBD_PORT;
    g_state.coolant_temp_c = -999;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    // 1. Enable New 3DS 804 MHz speedup if available
    osSetSpeedupEnable(true);

    // 2. Initialize state & load configuration
    state_init();
    config_load();

    snprintf(g_state.wifi_ssid, sizeof(g_state.wifi_ssid), "%s", g_config.wifi_ssid);
    snprintf(g_state.target_ip, sizeof(g_state.target_ip), "%s", g_config.obd_ip);
    g_state.target_port = g_config.obd_port;

    // 3. Initialize dual-screen text consoles
    ui_init();

    // 4. Initialize OBD protocol engine
    obd_init();

    // 5. Initialize 3DS SOC network
    net_init();

    uint32_t frame = 0;
    touchPosition touch;

    // 6. Main 3DS APT loop
    while (aptMainLoop()) {
        hidScanInput();
        u32 k_down = hidKeysDown();

        if (k_down & KEY_START) {
            break;
        }

        if (g_state.ui_mode == UI_MODE_WIFI_SCAN) {
            if (k_down & KEY_UP) {
                if (g_state.scan_selected_idx > 0) {
                    g_state.scan_selected_idx--;
                }
            }
            if (k_down & KEY_DOWN) {
                if (g_state.scan_selected_idx + 1 < g_state.scan_ap_count) {
                    g_state.scan_selected_idx++;
                }
            }
            if (k_down & KEY_A) {
                net_connect_to_ap_index(g_state.scan_selected_idx);
            }
            if (k_down & KEY_B) {
                net_stop_scan();
                ui_clear_log();
                ui_log_system("Wi-Fi selection cancelled");
            }
            if (k_down & KEY_X) {
                net_start_scan();
            }

            if (k_down & KEY_TOUCH) {
                hidTouchRead(&touch);
                int row = touch.py / 8;
                if (row >= 2 && row <= 19) {
                    int clicked_idx = g_state.scan_scroll_offset + (row - 2);
                    if (clicked_idx >= 0 && clicked_idx < g_state.scan_ap_count) {
                        if (clicked_idx == g_state.scan_selected_idx) {
                            net_connect_to_ap_index(clicked_idx);
                        } else {
                            g_state.scan_selected_idx = clicked_idx;
                        }
                    }
                } else if (row == 21) {
                    net_connect_to_ap_index(g_state.scan_selected_idx);
                } else if (row == 22) {
                    net_start_scan();
                } else if (row == 23) {
                    net_stop_scan();
                    ui_clear_log();
                    ui_log_system("Wi-Fi selection cancelled");
                }
            }
        } else {
            if (k_down & KEY_A) {
                obd_send_cmd("ATZ");
            }
            if (k_down & KEY_B) {
                obd_send_cmd("010C");
            }
            if (k_down & KEY_X) {
                obd_send_cmd("010D");
            }
            if (k_down & KEY_Y) {
                obd_send_cmd("ATRV");
            }
            if (k_down & KEY_L) {
                obd_send_cmd("0105");
            }
            if (k_down & KEY_R) {
                obd_send_cmd("0100");
            }
            if (k_down & KEY_DRIGHT) {
                g_state.auto_poll = !g_state.auto_poll;
                ui_log_system(g_state.auto_poll ? "Auto-Polling: ENABLED" : "Auto-Polling: DISABLED");
            }
            if (k_down & KEY_SELECT) {
                net_start_scan();
            }
            if (k_down & KEY_DUP) {
                ui_clear_log();
            }
        }

        net_update();

        if (g_state.ui_mode == UI_MODE_TERMINAL) {
            obd_poll_tick();
        }

        if (g_state.ui_mode == UI_MODE_WIFI_SCAN) {
            if ((frame++ % 4) == 0) {
                ui_render_top();
                ui_render_bottom();
            }
        } else {
            if ((frame++ % 10) == 0) {
                ui_render_top();
            }
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    net_cleanup();
    gfxExit();
    return 0;
}
