#include <3ds.h>
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

void ui_init(void) {
    gfxInitDefault();

    consoleInit(GFX_TOP, &s_top_console);
    consoleInit(GFX_BOTTOM, &s_bottom_console);

    consoleSelect(&s_top_console);
    consoleClear();

    consoleSelect(&s_bottom_console);
    consoleClear();

    ui_log_system("DSOBD 3DS Terminal Initialized");
    ui_log_system("Target: Veepeak OBDCheck VP01");
    ui_log_system("Press [SELECT] to choose Wi-Fi network");
}

void ui_set_status(const char *msg) {
    if (msg) {
        snprintf(g_state.status_msg, sizeof(g_state.status_msg), "%s", msg);
    }
}

static void ui_render_wifi_scan_top(void) {
    consoleSelect(&s_top_console);

    printf("\x1b[0;0H\x1b[36;1m====== DSOBD 3DS - WI-FI & TARGET SELECTOR =======\x1b[0m\n");
    printf("Profiles : %2d available   3DS Signal: %u/3 bars\n",
           g_state.scan_ap_count, (unsigned)g_state.wifi_bars);
    printf("Active   : %-20.20s (%s:%u)\n",
           g_config.wifi_ssid, g_config.obd_ip, (unsigned)g_config.obd_port);
    printf("--------------------------------------------------\n");

    if (g_state.scan_selected_idx >= 0 && g_state.scan_selected_idx < g_state.scan_ap_count) {
        ScanAPItem *it = &g_state.scan_aps[g_state.scan_selected_idx];

        printf("\x1b[33;1m-- HIGHLIGHTED NETWORK / PROFILE -----------------\x1b[0m\n");
        printf("SSID     : %-32.32s\n", it->ssid);
        printf("Source   : %-32.32s\n",
               it->slot_id > 0 ? "3DS System Settings Slot / Preset" : "Preset / Emulator Profile");
        printf("Security : %-10s\n", get_sec_str(it->security_type));
        printf("Target IP: %-15s   Port: %-5u\n", it->target_ip, (unsigned)it->target_port);
        printf("Quality  : %3d%%\n", (int)it->rssi);
    } else {
        printf("\x1b[33;1m-- HIGHLIGHTED NETWORK / PROFILE -----------------\x1b[0m\n");
        printf("No network selected.\n\n\n\n\n");
    }

    printf("--------------------------------------------------\n");
    printf("\x1b[32;1m-- VEEPEAK VP01 CONNECTION GUIDE -----------------\x1b[0m\n");
    printf("1. In 3DS System Settings -> Internet Settings,\n");
    printf("   add 'WiFi_OBDII' (Open) as Connection 1, 2, or 3\n");
    printf("2. Select your OBD network below and press [A]!\n");
    printf("Default Adapter IP/Port: 192.168.0.10 : 35000\n");
    printf("--------------------------------------------------\n");
    printf("\x1b[37;1mUse D-Pad / Circle Pad or Touch Screen to select.\x1b[0m\n");
    printf("Press [A] to connect  |  [B] Cancel  |  [X] Refresh");
}

static void ui_render_wifi_scan_bottom(void) {
    consoleSelect(&s_bottom_console);

    const int page_size = 18;
    if (g_state.scan_selected_idx < g_state.scan_scroll_offset) {
        g_state.scan_scroll_offset = g_state.scan_selected_idx;
    }
    if (g_state.scan_selected_idx >= g_state.scan_scroll_offset + page_size) {
        g_state.scan_scroll_offset = g_state.scan_selected_idx - (page_size - 1);
    }

    printf("\x1b[0;0H\x1b[36;1m--- CHOOSE WI-FI NETWORK / TARGET ------\x1b[0m\n");
    printf("Select AP and press [A] or Tap:\n");

    for (int r = 0; r < page_size; r++) {
        int idx = g_state.scan_scroll_offset + r;
        if (idx < g_state.scan_ap_count) {
            ScanAPItem *it = &g_state.scan_aps[idx];
            const char *sec = get_sec_str(it->security_type);

            if (idx == g_state.scan_selected_idx) {
                printf("\x1b[33;1m> %-16.16s [%-4.4s] %-13.13s\x1b[0m\n",
                       it->ssid, sec, it->target_ip);
            } else {
                printf("  %-16.16s [%-4.4s] %-13.13s\n",
                       it->ssid, sec, it->target_ip);
            }
        } else {
            printf("                                        \n");
        }
    }

    printf("----------------------------------------\n");
    printf("\x1b[32;1m[A / Tap] Connect to Selected Network\x1b[0m\n");
    printf("\x1b[37;1m[Up/Dn] Move     [X] Rescan Profiles\x1b[0m\n");
    printf("\x1b[31;1m[B] Cancel & Return to Terminal\x1b[0m");
}

void ui_render_top(void) {
    if (g_state.ui_mode == UI_MODE_WIFI_SCAN) {
        ui_render_wifi_scan_top();
        return;
    }

    consoleSelect(&s_top_console);

    printf("\x1b[0;0H\x1b[36;1m====== DSOBD - NINTENDO 3DS OBD-II TERMINAL ======\x1b[0m\n");
    printf("Adapter  : Veepeak OBDCheck VP01 (%s)\x1b[K\n",
           g_config.wifi_ssid[0] ? g_config.wifi_ssid : "Default AP");

    const char *st_col = "\x1b[37m";
    const char *st_str = "Disconnected";
    switch (g_state.net_status) {
        case NET_STATUS_DISCONNECTED:
            st_col = "\x1b[31;1m"; st_str = "DISCONNECTED"; break;
        case NET_STATUS_WIFI_CONNECTING:
            st_col = "\x1b[33;1m"; st_str = "WIFI INITIALIZING"; break;
        case NET_STATUS_WIFI_CONNECTED:
            st_col = "\x1b[32;1m"; st_str = "WIFI CONNECTED"; break;
        case NET_STATUS_TCP_CONNECTING:
            st_col = "\x1b[33;1m"; st_str = "TCP CONNECTING"; break;
        case NET_STATUS_TCP_CONNECTED:
            st_col = "\x1b[32;1m"; st_str = "OBD LINK ACTIVE"; break;
        case NET_STATUS_ERROR:
            st_col = "\x1b[31;1m"; st_str = "LINK ERROR"; break;
    }

    printf("Status   : %s%-22.22s\x1b[0m  Signal: %u/3\x1b[K\n",
           st_col, st_str, (unsigned)g_state.wifi_bars);
    printf("Local IP : %-18s\x1b[K\n", g_state.local_ip[0] ? g_state.local_ip : "Acquiring...");
    printf("Target   : %-15s:%-5u\x1b[K\n", g_state.target_ip, (unsigned)g_state.target_port);
    printf("--------------------------------------------------\n");

    printf("\x1b[33;1m-- LIVE VEHICLE TELEMETRY ------------------------\x1b[0m\n");
    printf("Battery  : %-10s\x1b[K\n", g_state.battery_voltage[0] ? g_state.battery_voltage : "--.-V");

    uint32_t speed_mph = (g_state.speed_kmh * 621) / 1000;
    printf("Speed    : %3u km/h   (%3u mph)\x1b[K\n", (unsigned)g_state.speed_kmh, (unsigned)speed_mph);
    printf("RPM      : %-6u RPM\x1b[K\n", (unsigned)g_state.rpm);

    if (g_state.coolant_temp_c != -999) {
        printf("Coolant  : %+4d C    (%+4d F)\x1b[K\n",
               g_state.coolant_temp_c,
               (g_state.coolant_temp_c * 9 / 5) + 32);
    } else {
        printf("Coolant  : --- C\x1b[K\n");
    }

    printf("ELM Info : %-35.35s\x1b[K\n",
           g_state.elm_version[0] ? g_state.elm_version : "Waiting for ATZ...");
    printf("--------------------------------------------------\n");

    printf("\x1b[32;1m-- TRAFFIC & STATS -------------------------------\x1b[0m\n");
    printf("RX Bytes : %-8lu (%lu lines)\x1b[K\n",
           (unsigned long)g_state.stats.bytes_rx,
           (unsigned long)g_state.stats.lines_rx);
    printf("TX Bytes : %-8lu (%lu cmds)\x1b[K\n",
           (unsigned long)g_state.stats.bytes_tx,
           (unsigned long)g_state.stats.cmds_sent);
    printf("Auto-Poll: %s\x1b[0m (Interval: %lums)\x1b[K\n",
           g_state.auto_poll ? "\x1b[32;1mENABLED " : "\x1b[31;1mDISABLED",
           (unsigned long)g_config.poll_interval_ms);
    printf("--------------------------------------------------\n");

    printf("\x1b[37;1m3DS CONTROLS:\x1b[0m\n");
    printf("[A] Reset ATZ      [B] Query RPM     [X] Query Speed\n");
    printf("[Y] Battery V      [L] Coolant Temp  [R] PIDs 0100\n");
    printf("[RIGHT] Auto-Poll  [UP] Clear Log    [SELECT] AP List\n");
    printf("[START] Exit to Homebrew Menu");
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
                printf("\x1b[32;1m>\x1b[0m ");
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
    printf("\x1b[33;1mTX> %s\x1b[0m\n", cmd);
}

void ui_log_system(const char *msg) {
    if (!msg) return;
    if (g_state.ui_mode == UI_MODE_WIFI_SCAN) return;
    consoleSelect(&s_bottom_console);
    printf("\x1b[36;1m[*] %s\x1b[0m\n", msg);
}

void ui_clear_log(void) {
    consoleSelect(&s_bottom_console);
    consoleClear();
    if (g_state.ui_mode == UI_MODE_TERMINAL) {
        ui_log_system("Log buffer cleared");
    }
}
