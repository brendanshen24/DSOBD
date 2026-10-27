#include <nds.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "common.h"
#include "config.h"
#include "net.h"

static PrintConsole s_top_console;
static PrintConsole s_bottom_console;
static bool s_last_was_cr = false;

static const char *get_sec_str(uint8_t sec) {
    switch (sec) {
        case 0: return "OPEN";
        case 1: return "WEP";
        case 2: return "WPA";
        case 3: return "WPA2";
        default: return "OPEN";
    }
}

static const char *get_signal_bar(int pct) {
    if (pct >= 80) return "[####]";
    if (pct >= 60) return "[### ]";
    if (pct >= 40) return "[##  ]";
    if (pct >= 20) return "[#   ]";
    return "[    ]";
}

void ui_init(void) {
    lcdMainOnTop();

    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    consoleInit(&s_top_console, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleInit(&s_bottom_console, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

    consoleSelect(&s_top_console);
    consoleClear();

    consoleSelect(&s_bottom_console);
    consoleClear();

    ui_log_system("DSOBD Terminal Initialized");
    ui_log_system("Target: Veepeak OBDCheck VP01");
    ui_log_system("Press [SELECT] to choose Wi-Fi network");
}

void ui_set_status(const char *msg) {
    if (msg) {
        strncpy(g_state.status_msg, msg, sizeof(g_state.status_msg) - 1);
    }
}

static void ui_render_wifi_scan_top(void) {
    consoleSelect(&s_top_console);

    printf("\x1b[0;0H\x1b[36;1m=== DSOBD - WI-FI SELECTOR ====\x1b[39;0m\n");
    printf("Networks: %2d found  (%s)\n",
           g_state.scan_ap_count,
           (g_state.scan_ticks % 16 < 8) ? "Scanning." : "Scanning..");
    printf("Active  : %-22.22s\n", g_config.wifi_ssid[0] ? g_config.wifi_ssid : "None");
    printf("--------------------------------\n");

    if (g_state.scan_selected_idx >= 0 && g_state.scan_selected_idx < g_state.scan_ap_count) {
        ScanAPItem *it = &g_state.scan_aps[g_state.scan_selected_idx];
        int pct = net_get_rssi_pct(it->rssi);

        printf("\x1b[33;1m-- HIGHLIGHTED NETWORK ---------\x1b[39;0m\n");
        printf("SSID    : %-22.22s\n", it->ssid);
        printf("Type    : %-22.22s\n", it->is_preset ? "Preset / Profile" : "Live Wireless Beacon");
        printf("Security: %-8s (Flags:%04X)\n", get_sec_str(it->security_type), it->flags);
        printf("Channel : Ch %u\n", (unsigned)it->channel);
        printf("Signal  : %3d%%  %s\n", pct, get_signal_bar(pct));
        printf("BSSID   : %02X:%02X:%02X:%02X:%02X:%02X\n",
               it->bssid[0], it->bssid[1], it->bssid[2],
               it->bssid[3], it->bssid[4], it->bssid[5]);
    } else {
        printf("\x1b[33;1m-- HIGHLIGHTED NETWORK ---------\x1b[39;0m\n");
        printf("No network selected.\n\n\n\n\n\n");
    }

    printf("--------------------------------\n");
    printf("\x1b[32;1m-- VP01 CONNECTION HINT --------\x1b[39;0m\n");
    printf("Veepeak default: 'WiFi_OBDII'\n");
    printf("Generic OBDII  : 'OBDII'/'V-LINK'\n");
    printf("Default IP/Port: 192.168.0.10:35000\n");
    printf("--------------------------------\n");
    printf("\x1b[37;1mTouch or D-Pad to select AP.\x1b[39;0m\n");
    printf("Press [A] to link immediately.");
}

static void ui_render_wifi_scan_bottom(void) {
    consoleSelect(&s_bottom_console);

    // Keep scroll offset aligned with selected index
    const int page_size = 17;
    if (g_state.scan_selected_idx < g_state.scan_scroll_offset) {
        g_state.scan_scroll_offset = g_state.scan_selected_idx;
    }
    if (g_state.scan_selected_idx >= g_state.scan_scroll_offset + page_size) {
        g_state.scan_scroll_offset = g_state.scan_selected_idx - (page_size - 1);
    }

    printf("\x1b[0;0H\x1b[36;1m-- AVAILABLE WI-FI NETWORKS ----\x1b[39;0m\n");
    printf("Select AP and press [A] or Tap:\n");

    for (int r = 0; r < page_size; r++) {
        int idx = g_state.scan_scroll_offset + r;
        if (idx < g_state.scan_ap_count) {
            ScanAPItem *it = &g_state.scan_aps[idx];
            int pct = net_get_rssi_pct(it->rssi);
            const char *sec = get_sec_str(it->security_type);

            if (idx == g_state.scan_selected_idx) {
                // Highlighted with cursor
                printf("\x1b[33;1m> %-16.16s [%-4.4s]%3d%%\x1b[39;0m\n", it->ssid, sec, pct);
            } else {
                printf("  %-16.16s [%-4.4s]%3d%%\n", it->ssid, sec, pct);
            }
        } else {
            // Clear empty row
            printf("                                \n");
        }
    }

    printf("--------------------------------\n");
    printf("\x1b[32;1m[A / Tap] Connect to AP\x1b[39;0m\n");
    printf("\x1b[37;1m[Up/Dn] Move   [X] Rescan\x1b[39;0m\n");
    printf("\x1b[31;1m[B] Cancel & Return to Terminal\x1b[39;0m");
}

void ui_render_top(void) {
    if (g_state.ui_mode == UI_MODE_WIFI_SCAN) {
        ui_render_wifi_scan_top();
        return;
    }

    consoleSelect(&s_top_console);

    // Header (Row 0-1). Value lines end in ESC[K so a shorter value
    // never leaves characters from the previous frame behind.
    printf("\x1b[0;0H\x1b[36;1m=== DSOBD - NINTENDO DS OBD2 ===\x1b[39;0m\n");
    printf("Adapter : Veepeak VP01 (%s)\x1b[K\n", g_config.wifi_ssid[0] ? g_config.wifi_ssid : "Default AP");

    // Connection Status (Row 2-4)
    const char *st_col = "\x1b[37;0m";
    const char *st_str = "Disconnected";
    switch (g_state.net_status) {
        case NET_STATUS_DISCONNECTED:
            st_col = "\x1b[31;1m"; st_str = "DISCONNECTED"; break;
        case NET_STATUS_WIFI_CONNECTING:
            st_col = "\x1b[33;1m"; st_str = "WIFI ASSOCIATING"; break;
        case NET_STATUS_WIFI_CONNECTED:
            st_col = "\x1b[32;1m"; st_str = "WIFI CONNECTED"; break;
        case NET_STATUS_TCP_CONNECTING:
            st_col = "\x1b[33;1m"; st_str = "TCP CONNECTING"; break;
        case NET_STATUS_TCP_CONNECTED:
            st_col = "\x1b[32;1m"; st_str = "OBD LINK ACTIVE"; break;
        case NET_STATUS_ERROR:
            st_col = "\x1b[31;1m"; st_str = "LINK ERROR"; break;
    }
    printf("Status  : %s%-16.16s\x1b[39;0m\x1b[K\n", st_col, st_str);
    printf("Local IP: %-15s\x1b[K\n", g_state.local_ip[0] ? g_state.local_ip : "Acquiring DHCP..");
    printf("Target  : %-15s:%-5u\x1b[K\n", g_state.target_ip, (unsigned)g_state.target_port);

    printf("--------------------------------\n");

    // Telemetry Summary (Row 6-12)
    printf("\x1b[33;1m-- LIVE VEHICLE TELEMETRY ------\x1b[39;0m\n");
    printf("Voltage : %-8s  Mode: %s\x1b[K\n",
           g_state.battery_voltage[0] ? g_state.battery_voltage : "--.-V",
           g_state.dsi_mode ? "DSi 134M" : "DS 67M");

    uint32_t speed_mph = (g_state.speed_kmh * 621) / 1000;
    printf("Speed   : %3u km/h  (%2u mph)\x1b[K\n", (unsigned)g_state.speed_kmh, (unsigned)speed_mph);
    printf("RPM     : %-5u RPM\x1b[K\n", (unsigned)g_state.rpm);

    if (g_state.coolant_temp_c != -999) {
        printf("Coolant : %+4d C   (%+4d F)\x1b[K\n",
               g_state.coolant_temp_c,
               (g_state.coolant_temp_c * 9 / 5) + 32);
    } else {
        printf("Coolant : --- C\x1b[K\n");
    }

    printf("ELM Info: %-22.22s\x1b[K\n", g_state.elm_version[0] ? g_state.elm_version : "Waiting for ATZ...");

    printf("--------------------------------\n");

    // Traffic Stats (Row 14-17)
    printf("\x1b[32;1m-- TRAFFIC & STATS -------------\x1b[39;0m\n");
    printf("RX: %-7lu bytes (%lu lines)\x1b[K\n",
           (unsigned long)g_state.stats.bytes_rx,
           (unsigned long)g_state.stats.lines_rx);
    printf("TX: %-7lu bytes (%lu cmds)\x1b[K\n",
           (unsigned long)g_state.stats.bytes_tx,
           (unsigned long)g_state.stats.cmds_sent);
    printf("Auto-Poll: %s\x1b[39;0m (Rate: %lums)\x1b[K\n",
           g_state.auto_poll ? "\x1b[32;1mENABLED " : "\x1b[31;1mDISABLED",
           (unsigned long)g_config.poll_interval_ms);

    printf("--------------------------------\n");

    // Controls Legend (Row 19-23)
    printf("\x1b[37;1mCONTROLS:\x1b[39;0m\n");
    printf("[A] Reset ATZ    [B] Query RPM\n");
    printf("[X] Query Speed  [Y] Battery V\n");
    printf("[L] Coolant T    [R] PIDs 0100\n");
    printf("[START] AutoPoll [SELECT] AP List");
}

void ui_render_bottom(void) {
    if (g_state.ui_mode == UI_MODE_WIFI_SCAN) {
        ui_render_wifi_scan_bottom();
    }
}

void ui_log_rx_bytes(const uint8_t *data, size_t len) {
    if (!data || len == 0) return;
    if (g_state.ui_mode == UI_MODE_WIFI_SCAN) return;

    consoleSelect(&s_bottom_console);

    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];

        if (c == '\r') {
            printf("\n");
            s_last_was_cr = true;
        } else if (c == '\n') {
            if (!s_last_was_cr) {
                printf("\n");
            }
            s_last_was_cr = false;
        } else {
            s_last_was_cr = false;
            if (c == '>') {
                // Highlight ELM327 command prompt in green
                printf("\x1b[32;1m>\x1b[39;0m ");
            } else if (c >= 32 && c <= 126) {
                printf("%c", (char)c);
            } else if (c == '\t') {
                printf("  ");
            }
        }
    }
}

void ui_log_tx(const char *cmd) {
    if (!cmd) return;
    if (g_state.ui_mode == UI_MODE_WIFI_SCAN) return;
    consoleSelect(&s_bottom_console);
    printf("\x1b[33;1mTX> %s\x1b[39;0m\n", cmd);
}

void ui_log_system(const char *msg) {
    if (!msg) return;
    if (g_state.ui_mode == UI_MODE_WIFI_SCAN) return;
    consoleSelect(&s_bottom_console);
    printf("\x1b[36;1m[*] %s\x1b[39;0m\n", msg);
}

void ui_clear_log(void) {
    consoleSelect(&s_bottom_console);
    consoleClear();
    if (g_state.ui_mode == UI_MODE_TERMINAL) {
        ui_log_system("Log buffer cleared");
    }
}
