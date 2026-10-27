#ifndef COMMON_H
#define COMMON_H

#include <nds.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    NET_STATUS_DISCONNECTED = 0,
    NET_STATUS_WIFI_CONNECTING,
    NET_STATUS_WIFI_CONNECTED,
    NET_STATUS_TCP_CONNECTING,
    NET_STATUS_TCP_CONNECTED,
    NET_STATUS_ERROR
} NetStatus;

typedef enum {
    UI_MODE_TERMINAL = 0,
    UI_MODE_WIFI_SCAN
} UIMode;

typedef struct {
    char ssid[33];
    uint8_t bssid[6];
    uint8_t channel;
    int16_t rssi;
    uint16_t flags;
    uint8_t security_type;
    bool is_preset;
} ScanAPItem;

#define MAX_SCAN_APS 32

typedef struct {
    uint32_t bytes_rx;
    uint32_t bytes_tx;
    uint32_t lines_rx;
    uint32_t cmds_sent;
    uint32_t rx_packets;  // recv() calls that returned data
} OBDStats;

typedef struct {
    bool dsi_mode;
    UIMode ui_mode;
    NetStatus net_status;
    char status_msg[48];

    // Network configuration & detected info
    char wifi_ssid[33];
    char local_ip[20];
    char gateway_ip[20];
    char target_ip[20];
    uint16_t target_port;

    // ELM327 / OBD state
    bool elm_inited;
    bool auto_poll;
    uint32_t poll_tick;
    uint8_t poll_phase;

    // Parsed telemetry summary for top screen
    char elm_version[32];
    char battery_voltage[16];
    uint32_t rpm;
    uint32_t speed_kmh;
    int coolant_temp_c;
    char last_raw_line[64];

    OBDStats stats;

    // Wi-Fi Scanner state
    ScanAPItem scan_aps[MAX_SCAN_APS];
    int scan_ap_count;
    int scan_selected_idx;
    int scan_scroll_offset;
    bool scan_in_progress;
    uint32_t scan_ticks;
} AppState;

extern AppState g_state;

#endif // COMMON_H
