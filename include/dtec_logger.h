#ifndef DTEC_LOGGER_H
#define DTEC_LOGGER_H

#include <stdint.h>

// CSV log on the SD card (dsobd-log-NNN.csv): one row per 100 ms holding every
// sensor read in that window. Columns follow g_dtec_pids; blank = not read.
void dtec_logger_toggle(void);
void dtec_logger_tick(uint32_t now_ms);

#endif
