#include <nds.h>
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
    g_state.dsi_mode = isDSiMode();
    g_state.ui_mode = UI_MODE_TERMINAL;
    g_state.net_status = NET_STATUS_DISCONNECTED;
    strncpy(g_state.status_msg, "Initializing...", sizeof(g_state.status_msg) - 1);
    strncpy(g_state.wifi_ssid, DEFAULT_OBD_SSID, sizeof(g_state.wifi_ssid) - 1);
    strncpy(g_state.target_ip, DEFAULT_OBD_IP, sizeof(g_state.target_ip) - 1);
    g_state.target_port = DEFAULT_OBD_PORT;
    g_state.coolant_temp_c = -999;
}

int main(void) {
    // 1. Boost CPU clock if running on DSi (134 MHz TWL mode)
    if (isDSiMode()) {
        setCpuClock(true);
    }

    // 2. Initialize application state & load config
    state_init();
    config_load();

    snprintf(g_state.wifi_ssid, sizeof(g_state.wifi_ssid), "%s", g_config.wifi_ssid);
    snprintf(g_state.target_ip, sizeof(g_state.target_ip), "%s", g_config.obd_ip);
    g_state.target_port = g_config.obd_port;

    // 3. Initialize dual-screen text consoles
    ui_init();

    // 4. Initialize OBD protocol engine
    obd_init();

    // 5. Initialize DSWifi and start AP association
    net_init();

    uint32_t frame = 0;
    touchPosition touch;

    // 6. Main loop
    while (1) {
        cothread_yield_irq(IRQ_VBLANK);

        scanKeys();
        uint32_t k_down = keysDown();

        if (g_state.ui_mode == UI_MODE_WIFI_SCAN) {
            // =========================================================
            // Wi-Fi Scanner / AP Selector Input Handling
            // =========================================================
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

            // Touch screen AP list selection
            if (k_down & KEY_TOUCH) {
                touchRead(&touch);
                int row = touch.py / 8; // 8x8 character rows (0-23)

                // Rows 2-18 correspond to the visible AP list
                if (row >= 2 && row <= 18) {
                    int clicked_idx = g_state.scan_scroll_offset + (row - 2);
                    if (clicked_idx >= 0 && clicked_idx < g_state.scan_ap_count) {
                        if (clicked_idx == g_state.scan_selected_idx) {
                            // Tapping already-selected item connects immediately
                            net_connect_to_ap_index(clicked_idx);
                        } else {
                            // First tap selects item
                            g_state.scan_selected_idx = clicked_idx;
                        }
                    }
                } else if (row == 21) {
                    // Tap on "[A / Tap] Connect to AP" row
                    net_connect_to_ap_index(g_state.scan_selected_idx);
                } else if (row == 22) {
                    // Tap on "[X] Rescan"
                    net_start_scan();
                } else if (row == 23) {
                    // Tap on "[B] Cancel"
                    net_stop_scan();
                    ui_clear_log();
                    ui_log_system("Wi-Fi selection cancelled");
                }
            }
        } else {
            // =========================================================
            // Standard Live Terminal Mode Input Handling
            // =========================================================
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
            if (k_down & KEY_START) {
                g_state.auto_poll = !g_state.auto_poll;
                ui_log_system(g_state.auto_poll ? "Auto-Polling: ENABLED" : "Auto-Polling: DISABLED");
            }
            if (k_down & KEY_SELECT) {
                // Enter Wi-Fi Network Scanner / AP Selector
                net_start_scan();
            }
            if (k_down & KEY_UP) {
                ui_clear_log();
            }
        }

        // Network and beacon scan updates
        net_update();

        // OBD auto-query polling
        if (g_state.ui_mode == UI_MODE_TERMINAL) {
            obd_poll_tick();
        }

        // Screen rendering
        if (g_state.ui_mode == UI_MODE_WIFI_SCAN) {
            // Render Wi-Fi scanner every 4 frames (15 Hz) for fast responsive navigation
            if ((frame++ % 4) == 0) {
                ui_render_top();
                ui_render_bottom();
            }
        } else {
            // Refresh top dashboard every 10 frames (~6 Hz)
            if ((frame++ % 10) == 0) {
                ui_render_top();
            }
        }
    }

    return 0;
}
