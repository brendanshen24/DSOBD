#ifndef COMMON_H
#define COMMON_H

#include <pspkernel.h>
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
    char profile_name[33];
    char target_ip[20];
    uint16_t target_port;
    int netparam_id;       // 1..10 = PSP saved NetParam slot, 0 = preset
    uint8_t security_type; // 0 = OPEN, 1 = WEP, 2 = WPA
    int16_t rssi;
    bool is_preset;
} ScanAPItem;

#define MAX_SCAN_APS 16

typedef struct {
    uint32_t bytes_rx;
    uint32_t bytes_tx;
    uint32_t lines_rx;
    uint32_t cmds_sent;
    uint32_t rx_packets;  // recv() calls that returned data
} OBDStats;

typedef struct {
    UIMode ui_mode;
    NetStatus net_status;
    char status_msg[48];

    // Network configuration & detected info
    char wifi_ssid[33];
    char local_ip[24];
    char gateway_ip[20];
    char target_ip[20];
    uint16_t target_port;
    int apctl_profile_index;
    bool wlan_switch_on;

    // ELM327 / OBD state
    bool elm_inited;
    bool auto_poll;
    uint32_t poll_tick;
    uint8_t poll_phase;

    // Parsed telemetry summary
    char elm_version[32];
    char battery_voltage[16];
    uint32_t rpm;
    uint32_t speed_kmh;
    int coolant_temp_c;
    char last_raw_line[64];

    OBDStats stats;

    // Wi-Fi Scanner / Selector state
    ScanAPItem scan_aps[MAX_SCAN_APS];
    int scan_ap_count;
    int scan_selected_idx;
    int scan_scroll_offset;
    bool scan_in_progress;
    uint32_t scan_ticks;
} AppState;

extern AppState g_state;

#endif // COMMON_H
