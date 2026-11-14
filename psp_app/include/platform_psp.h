#ifndef DSOBD_PSP_PLATFORM_H
#define DSOBD_PSP_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include "dtec_common.h"

/* PSP adds a dedicated XMB Wi-Fi / Target IP Manager view beyond the 17 DS modes. */
#define PSP_MODE_WIFI  ((DTEC_ScreenMode)DTEC_MODE_COUNT)
#define PSP_MODE_COUNT (DTEC_MODE_COUNT + 1)

#define MAX_SCAN_APS 16

typedef struct {
    char ssid[33];
    char profile_name[33];
    char target_ip[20];
    uint16_t target_port;
    int netparam_id;       // 1..10 = PSP saved XMB NetParam slot, 0 = preset/config
    uint8_t security_type; // 0 = OPEN, 1 = WEP, 2 = WPA
    int16_t signal;        // 0..100
    bool is_preset;
} PspWifiItem;

typedef struct {
    // Hardware & WLAN telemetry
    bool wlan_switch_on;
    int apctl_state;
    int apctl_profile_index;
    int signal_strength;
    char local_ip[24];
    char gateway_ip[20];
    uint32_t rx_packets;

    // Wi-Fi AP / OBD adapter list
    PspWifiItem wifi_items[MAX_SCAN_APS];
    int wifi_count;
    int wifi_selected;

    // PSP power / battery info (polled periodically)
    int battery_pct;
    bool battery_charging;
    bool ac_online;

    // PSP-specialized preferences
    uint16_t shift_rpm;    // 5000..8500 (default 7000)
    bool shift_beep;       // Play high chime on redline entry
    bool use_imperial;     // Display mph / °F / PSI vs km/h / °C / kPa
    bool keep_awake;       // Call scePowerTick(PSP_POWER_TICK_DISPLAY)
    bool clean_hud;        // Circle toggles bottom status/hint bar on gauge screens

    // Peak telemetry beyond g_dtec (PSP session summary)
    uint32_t peak_speed_kmh;
    int peak_coolant_c;
} PspState;

extern PspState g_psp;

uint32_t psp_elapsed_ms(void);

void psp_prefs_load(void);
bool psp_prefs_save(void);

void dtec_net_cleanup(void);
void psp_net_refresh_list(void);
bool psp_net_connect_item(int index);
void psp_net_set_target(const char *ip, uint16_t port);
void psp_net_save_ini(void);

void dtec_audio_shutdown(void);
void psp_audio_play_shift(void);

void psp_ui_init(void);
void psp_ui_fini(void);
void psp_ui_handle_input(uint32_t k_down, uint32_t k_held, int8_t lx, int8_t ly);
void psp_ui_render(void);

#endif // DSOBD_PSP_PLATFORM_H
