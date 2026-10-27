#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "common.h"

bool net_init(void);
void net_cleanup(void);
void net_update(void);
bool net_send(const void *data, size_t len);
bool net_send_cmd(const char *cmd_str);
void net_reconnect(void);
NetStatus net_get_status(void);

// Wi-Fi Scanner and AP Selector functions
void net_start_scan(void);
void net_update_scan(void);
void net_stop_scan(void);
bool net_connect_to_ap_index(int index);

#endif // NET_H
