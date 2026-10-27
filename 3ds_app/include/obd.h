#ifndef OBD_H
#define OBD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void obd_init(void);
void obd_on_connected(void);
void obd_send_cmd(const char *cmd);
void obd_feed_rx_data(const uint8_t *data, size_t len);
void obd_poll_tick(void);

#endif // OBD_H
