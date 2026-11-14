#ifndef DTEC_NET_H
#define DTEC_NET_H

#include "dtec_common.h"

void dtec_net_init(void);
void dtec_net_update(void);
void dtec_net_send_cmd(const char *cmd);
void dtec_net_reconnect(void);
bool dtec_net_is_connected(void);

#endif // DTEC_NET_H
