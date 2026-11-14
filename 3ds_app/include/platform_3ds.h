#ifndef DSOBD_3DS_PLATFORM_H
#define DSOBD_3DS_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

void dtec_net_cleanup(void);

#define DTEC_3DS_WIFI_SLOTS 3
extern char g_3ds_slot_ssid[DTEC_3DS_WIFI_SLOTS][33];
void dtec_3ds_net_scan_slots(void);
bool dtec_3ds_net_connect_slot(int slot);
void dtec_audio_shutdown(void);

void dtec_3ds_render_top(void);
void dtec_3ds_render_bottom(void);
void dtec_3ds_handle_touch(int tx, int ty, bool is_down);
void dtec_3ds_handle_keys(uint32_t keys);

#endif // DSOBD_3DS_PLATFORM_H
