#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "common.h"
#include "config.h"
#include "net.h"

// PSP Debug Screen is 68 columns x 34 rows
// Colors in ABGR8888 format
#define COL_WHITE  0xFFFFFFFFU
#define COL_CYAN   0xFFFFFF00U
#define COL_YELLOW 0xFF00FFFFU
#define COL_GREEN  0xFF00FF00U
#define COL_RED    0xFF4040FFU
#define COL_GRAY   0xFFAAAAAAU

#define LOG_VISIBLE_LINES 18
#define LOG_LINE_MAX_CHARS 66

typedef struct {
    char text[LOG_LINE_MAX_CHARS + 1];
    uint32_t color;
} LogLine;

static LogLine s_log_lines[LOG_VISIBLE_LINES];
static int s_cur_line_len = 0;
static bool s_last_was_cr = false;

static const char *get_sec_str(uint8_t sec) {
    switch (sec) {
        case 0: return "OPEN";
        case 1: return "WEP";
        case 2: return "WPA";
        default: return "OPEN";
    }
}

static void push_new_log_line(uint32_t default_color) {
    if (s_cur_line_len == 0 && s_log_lines[LOG_VISIBLE_LINES - 1].text[0] == '\0') {
        s_log_lines[LOG_VISIBLE_LINES - 1].color = default_color;
        return;
    }
    memmove(&s_log_lines[0], &s_log_lines[1], sizeof(LogLine) * (LOG_VISIBLE_LINES - 1));
    memset(&s_log_lines[LOG_VISIBLE_LINES - 1], 0, sizeof(LogLine));
    s_log_lines[LOG_VISIBLE_LINES - 1].color = default_color;
    s_cur_line_len = 0;
}

static void append_log_char(char c, uint32_t color) {
    LogLine *cur = &s_log_lines[LOG_VISIBLE_LINES - 1];
    if (s_cur_line_len == 0) {
        cur->color = color;
    }
    if (s_cur_line_len >= LOG_LINE_MAX_CHARS) {
        push_new_log_line(color);
        cur = &s_log_lines[LOG_VISIBLE_LINES - 1];
    }
    cur->text[s_cur_line_len++] = c;
    cur->text[s_cur_line_len] = '\0';
}

static void append_full_line(const char *prefix, const char *msg, uint32_t color) {
    if (s_cur_line_len > 0) {
        push_new_log_line(color);
    }
    LogLine *cur = &s_log_lines[LOG_VISIBLE_LINES - 1];
    cur->color = color;
    snprintf(cur->text, sizeof(cur->text), "%s%s", prefix ? prefix : "", msg ? msg : "");
    s_cur_line_len = (int)strlen(cur->text);
    push_new_log_line(COL_WHITE);
}

void ui_init(void) {
    pspDebugScreenInit();
    pspDebugScreenClear();

    memset(s_log_lines, 0, sizeof(s_log_lines));
    for (int i = 0; i < LOG_VISIBLE_LINES; i++) {
        s_log_lines[i].color = COL_WHITE;
    }
    s_cur_line_len = 0;

    ui_log_system("DSOBD PSP Terminal Initialized");
    ui_log_system("Target: Veepeak OBDCheck VP01 (192.168.0.10:35000)");
    ui_log_system("Press [SELECT] to choose Wi-Fi network from list");
}

void ui_set_status(const char *msg) {
    if (msg) {
        snprintf(g_state.status_msg, sizeof(g_state.status_msg), "%s", msg);
    }
}

void ui_log_rx_bytes(const uint8_t *data, size_t len) {
    if (!data || len == 0) return;

    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];

        if (c == '\r') {
            push_new_log_line(COL_WHITE);
            s_last_was_cr = true;
        } else if (c == '\n') {
            if (!s_last_was_cr) {
                push_new_log_line(COL_WHITE);
            }
            s_last_was_cr = false;
        } else {
            s_last_was_cr = false;
            if (c == '>') {
                append_log_char('>', COL_GREEN);
                append_log_char(' ', COL_GREEN);
            } else if (c >= 32 && c <= 126) {
                append_log_char((char)c, COL_WHITE);
            } else if (c == '\t') {
                append_log_char(' ', COL_WHITE);
                append_log_char(' ', COL_WHITE);
            }
        }
    }
}

void ui_log_tx(const char *cmd) {
    if (!cmd) return;
    append_full_line("TX> ", cmd, COL_YELLOW);
}

void ui_log_system(const char *msg) {
    if (!msg) return;
    append_full_line("[*] ", msg, COL_CYAN);
}

void ui_clear_log(void) {
    memset(s_log_lines, 0, sizeof(s_log_lines));
    for (int i = 0; i < LOG_VISIBLE_LINES; i++) {
        s_log_lines[i].color = COL_WHITE;
    }
    s_cur_line_len = 0;
    if (g_state.ui_mode == UI_MODE_TERMINAL) {
        ui_log_system("Log buffer cleared");
    }
}

static void render_wifi_scan_screen(void) {
    pspDebugScreenSetXY(0, 0);

    pspDebugScreenSetTextColor(COL_CYAN);
    pspDebugScreenPrintf("================ DSOBD PSP - WI-FI NETWORK SELECTOR ================\n");

    pspDebugScreenSetTextColor(COL_WHITE);
    pspDebugScreenPrintf(" WLAN Switch: %-4s | Active SSID: %-18.18s | Found: %2d   \n",
                         g_state.wlan_switch_on ? "ON" : "OFF",
                         g_config.wifi_ssid,
                         g_state.scan_ap_count);
    pspDebugScreenSetTextColor(COL_GRAY);
    pspDebugScreenPrintf("--------------------------------------------------------------------\n");

    if (g_state.scan_selected_idx >= 0 && g_state.scan_selected_idx < g_state.scan_ap_count) {
        ScanAPItem *sel = &g_state.scan_aps[g_state.scan_selected_idx];
        pspDebugScreenSetTextColor(COL_YELLOW);
        pspDebugScreenPrintf(" SELECTED: %-20.20s  Profile: %-18.18s      \n",
                             sel->ssid, sel->profile_name);
        pspDebugScreenSetTextColor(COL_WHITE);
        pspDebugScreenPrintf(" Target  : %-15s:%-5u  Security: %-6s  Slot: #%d       \n",
                             sel->target_ip, (unsigned)sel->target_port,
                             get_sec_str(sel->security_type), sel->netparam_id);
    } else {
        pspDebugScreenPrintf(" No network selected.                                               \n\n");
    }

    pspDebugScreenSetTextColor(COL_GRAY);
    pspDebugScreenPrintf("--------------------------------------------------------------------\n");
    pspDebugScreenSetTextColor(COL_CYAN);
    pspDebugScreenPrintf("   #  SSID                 PROFILE / SOURCE     SECURITY  TARGET IP \n");
    pspDebugScreenSetTextColor(COL_GRAY);
    pspDebugScreenPrintf("--------------------------------------------------------------------\n");

    const int page_size = 18;
    if (g_state.scan_selected_idx < g_state.scan_scroll_offset) {
        g_state.scan_scroll_offset = g_state.scan_selected_idx;
    }
    if (g_state.scan_selected_idx >= g_state.scan_scroll_offset + page_size) {
        g_state.scan_scroll_offset = g_state.scan_selected_idx - (page_size - 1);
    }

    for (int r = 0; r < page_size; r++) {
        int idx = g_state.scan_scroll_offset + r;
        if (idx < g_state.scan_ap_count) {
            ScanAPItem *it = &g_state.scan_aps[idx];
            if (idx == g_state.scan_selected_idx) {
                pspDebugScreenSetTextColor(COL_YELLOW);
                pspDebugScreenPrintf(" > %2d %-20.20s %-20.20s [%-4.4s]    %-15.15s\n",
                                     idx + 1, it->ssid, it->profile_name,
                                     get_sec_str(it->security_type), it->target_ip);
            } else {
                pspDebugScreenSetTextColor(COL_WHITE);
                pspDebugScreenPrintf("   %2d %-20.20s %-20.20s [%-4.4s]    %-15.15s\n",
                                     idx + 1, it->ssid, it->profile_name,
                                     get_sec_str(it->security_type), it->target_ip);
            }
        } else {
            pspDebugScreenPrintf("                                                                    \n");
        }
    }

    pspDebugScreenSetTextColor(COL_GRAY);
    pspDebugScreenPrintf("--------------------------------------------------------------------\n");
    pspDebugScreenSetTextColor(COL_GREEN);
    pspDebugScreenPrintf(" [Cross X] Connect to Selected AP   [Square] Refresh Network List  \n");
    pspDebugScreenSetTextColor(COL_WHITE);
    pspDebugScreenPrintf(" [Up/Down] Move Cursor              [Circle O] Cancel & Return     ");
}

void ui_render(void) {
    if (g_state.ui_mode == UI_MODE_WIFI_SCAN) {
        render_wifi_scan_screen();
        return;
    }

    pspDebugScreenSetXY(0, 0);

    // Header Row 0
    pspDebugScreenSetTextColor(COL_CYAN);
    pspDebugScreenPrintf("============= DSOBD PSP - VEEPEAK OBDCheck VP01 TERMINAL ===========\n");

    // Status Rows 1-3
    uint32_t st_col = COL_WHITE;
    const char *st_str = "DISCONNECTED";
    switch (g_state.net_status) {
        case NET_STATUS_DISCONNECTED:    st_col = COL_RED;    st_str = "DISCONNECTED"; break;
        case NET_STATUS_WIFI_CONNECTING: st_col = COL_YELLOW; st_str = "JOINING WLAN"; break;
        case NET_STATUS_WIFI_CONNECTED:  st_col = COL_GREEN;  st_str = "WLAN CONNECTED"; break;
        case NET_STATUS_TCP_CONNECTING:  st_col = COL_YELLOW; st_str = "TCP CONNECTING"; break;
        case NET_STATUS_TCP_CONNECTED:   st_col = COL_GREEN;  st_str = "OBD LINK ACTIVE"; break;
        case NET_STATUS_ERROR:           st_col = COL_RED;    st_str = "LINK ERROR"; break;
    }

    pspDebugScreenSetTextColor(COL_WHITE);
    pspDebugScreenPrintf(" SSID   : %-18.18s | Local IP: %-18.18s      \n",
                         g_config.wifi_ssid,
                         g_state.local_ip[0] ? g_state.local_ip : "Acquiring...");

    pspDebugScreenPrintf(" Status : ");
    pspDebugScreenSetTextColor(st_col);
    pspDebugScreenPrintf("%-18.18s", st_str);
    pspDebugScreenSetTextColor(COL_WHITE);
    pspDebugScreenPrintf(" | Target  : %-15s:%-5u      \n",
                         g_state.target_ip, (unsigned)g_state.target_port);

    // Telemetry Rows 4-7
    pspDebugScreenSetTextColor(COL_GRAY);
    pspDebugScreenPrintf("--------------------------------------------------------------------\n");

    uint32_t speed_mph = (g_state.speed_kmh * 621) / 1000;
    char cool_str[20];
    if (g_state.coolant_temp_c != -999) {
        snprintf(cool_str, sizeof(cool_str), "%+dC (%+dF)",
                 g_state.coolant_temp_c, (g_state.coolant_temp_c * 9 / 5) + 32);
    } else {
        snprintf(cool_str, sizeof(cool_str), "--- C");
    }

    pspDebugScreenSetTextColor(COL_YELLOW);
    pspDebugScreenPrintf(" RPM: %-5u RPM | Speed: %3u km/h (%2u mph) | Volts: %-6.6s | T: %-9.9s\n",
                         (unsigned)g_state.rpm,
                         (unsigned)g_state.speed_kmh,
                         (unsigned)speed_mph,
                         g_state.battery_voltage,
                         cool_str);

    pspDebugScreenSetTextColor(COL_WHITE);
    pspDebugScreenPrintf(" ELM: %-20.20s | RX: %-6lu B (%-3lu ln) | Poll: %-3s    \n",
                         g_state.elm_version[0] ? g_state.elm_version : "Waiting for ATZ...",
                         (unsigned long)g_state.stats.bytes_rx,
                         (unsigned long)g_state.stats.lines_rx,
                         g_state.auto_poll ? "ON" : "OFF");

    pspDebugScreenSetTextColor(COL_GRAY);
    pspDebugScreenPrintf("--------------------------------------------------------------------\n");

    // Controls Rows 8-9
    pspDebugScreenSetTextColor(COL_GREEN);
    pspDebugScreenPrintf(" [X] ATZ Reset  [O] RPM 010C  [Sq] Speed 010D  [Tri] Voltage ATRV   \n");
    pspDebugScreenPrintf(" [L] Temp 0105  [R] PIDs 0100 [START] AutoPoll [SELECT] Wi-Fi List  \n");

    pspDebugScreenSetTextColor(COL_CYAN);
    pspDebugScreenPrintf("------------------ LIVE INCOMING RAW OBD-II STREAM -----------------\n");

    // Scrolling Terminal Log Rows 11-28 (18 visible lines)
    for (int i = 0; i < LOG_VISIBLE_LINES; i++) {
        pspDebugScreenSetTextColor(s_log_lines[i].color);
        pspDebugScreenPrintf(" %-66.66s\n", s_log_lines[i].text);
    }

    pspDebugScreenSetTextColor(COL_GRAY);
    pspDebugScreenPrintf("--------------------------------------------------------------------\n");
    pspDebugScreenSetTextColor(COL_WHITE);
    pspDebugScreenPrintf(" [D-Pad UP] Clear Terminal Log     | TX Bytes: %-6lu (%-4lu cmds) ",
                         (unsigned long)g_state.stats.bytes_tx,
                         (unsigned long)g_state.stats.cmds_sent);
}
