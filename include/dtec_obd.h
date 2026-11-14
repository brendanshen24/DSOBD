#ifndef DTEC_OBD_H
#define DTEC_OBD_H

#include "dtec_common.h"

void dtec_obd_init(void);
void dtec_obd_apply_profile(int idx);
void dtec_obd_on_connected(void);
void dtec_obd_feed_rx(const uint8_t *data, size_t len);
void dtec_obd_poll_tick(uint32_t now_ms);
void dtec_obd_send_cmd(const char *cmd);

// Reads MIL, readiness, stored/pending/permanent codes, freeze frame, VIN and
// protocol. clear_first sends mode 04 (erase codes + freeze frame) before it.
void dtec_obd_scan_codes(bool clear_first);
int dtec_obd_pid_index(uint8_t pid);
// Fills out with g_dtec_pids indices the vehicle supports; returns the count.
int dtec_obd_sensor_list(int out[DTEC_PID_COUNT]);
void dtec_obd_format_dtc(uint16_t code, char out[8]);

#endif // DTEC_OBD_H
