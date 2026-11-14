#include <nds.h>
#include <dswifi9.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "dtec_common.h"
#include "dtec_net.h"
#include "dtec_obd.h"

// Forward declaration of OBD hook
void dtec_obd_on_connected(void);
void dtec_obd_feed_rx(const uint8_t *data, size_t len);

static int s_sock = -1;
static uint32_t s_reconnect_timer = 0;
static bool s_wifi_hw_inited = false;
static int s_wifi_search_ticks = 0;
static int s_dhcp_ticks = 0;
static int s_tcp_connect_ticks = 0;
static int s_connect_phase = 0; // 0: WiFi_OBDII, 1: melonAP, 2: AutoConnect
static char s_gateway_ip[20] = {0};
static int s_last_assoc = -1;

static void close_socket(void) {
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }
}

static void set_status(DTEC_NetStatus st, const char *msg) {
    g_dtec.net_status = st;
    if (msg) {
        strncpy(g_dtec.status_msg, msg, sizeof(g_dtec.status_msg) - 1);
        g_dtec.status_msg[sizeof(g_dtec.status_msg) - 1] = '\0';
        dtec_log(msg);
    }
}

static const char *get_assoc_str(int assoc) {
    switch (assoc) {
        case ASSOCSTATUS_DISCONNECTED:   return "WiFi Idle";
        case ASSOCSTATUS_SEARCHING:      return "Scanning Channels...";
        case ASSOCSTATUS_AUTHENTICATING: return "802.11 Auth...";
        case ASSOCSTATUS_ASSOCIATING:    return "802.11 Assoc...";
        case ASSOCSTATUS_ACQUIRINGDHCP:  return "Acquiring DHCP...";
        case ASSOCSTATUS_ASSOCIATED:     return "Link OK";
        case ASSOCSTATUS_CANNOTCONNECT:  return "Cannot Connect";
        default:                         return "Connecting...";
    }
}

static void start_tcp_connection(void) {
    close_socket();

    char msg[64];
    snprintf(msg, sizeof(msg), "Connecting TCP %s:%u...", g_dtec.target_ip, g_dtec.target_port);
    set_status(NET_TCP_CONNECTING, msg);

    s_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (s_sock < 0) {
        snprintf(msg, sizeof(msg), "Socket fail (errno=%d)", errno);
        set_status(NET_ERROR, msg);
        return;
    }
    dtec_log("TCP socket created");

    int flags = fcntl(s_sock, F_GETFL, 0);
    fcntl(s_sock, F_SETFL, flags | O_NONBLOCK);

    int nodelay = 1;
    setsockopt(s_sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(g_dtec.target_port);
    srv.sin_addr.s_addr = inet_addr(g_dtec.target_ip);

    dtec_log("Sending TCP SYN...");
    s_tcp_connect_ticks = 0;
    int res = connect(s_sock, (struct sockaddr *)&srv, sizeof(srv));
    if (res == 0) {
        set_status(NET_OBD_CONNECTED, "OBD Link Active");
        dtec_obd_on_connected();
    } else if (errno == EINPROGRESS || errno == EWOULDBLOCK || errno == 0) {
        set_status(NET_TCP_CONNECTING, "Waiting SYN-ACK...");
    } else {
        char err[48];
        snprintf(err, sizeof(err), "Connect fail (errno=%d)", errno);
        set_status(NET_ERROR, err);
        close_socket();
    }
}

static void try_connect_current_phase(void) {
    s_wifi_search_ticks = 0;
    s_dhcp_ticks = 0;
    s_last_assoc = -1;
    Wifi_DisconnectAP();
    Wifi_SetIP(0, 0, 0, 0, 0);

    if (s_connect_phase == 0) {
        // Phase 0: Physical vehicle Veepeak VP01 / WiFi_OBDII adapter (DHCP 192.168.0.11, fallback static)
        Wifi_AccessPoint ap;
        memset(&ap, 0, sizeof(ap));
        snprintf((char *)ap.ssid, sizeof(ap.ssid), "%s", g_dtec.wifi_ssid);
        ap.ssid_len = strlen((char *)ap.ssid);
        Wifi_ConnectOpenAP(&ap);

        char msg[64];
        snprintf(msg, sizeof(msg), "Connecting '%s'...", g_dtec.wifi_ssid);
        set_status(NET_WIFI_CONNECTING, msg);
    } else if (s_connect_phase == 1) {
        // Phase 1: melonDS emulator virtual AP (uses DHCP)
        Wifi_AccessPoint melon_ap;
        memset(&melon_ap, 0, sizeof(melon_ap));
        strcpy((char *)melon_ap.ssid, "melonAP");
        melon_ap.ssid_len = 7;
        Wifi_ConnectOpenAP(&melon_ap);

        set_status(NET_WIFI_CONNECTING, "Connecting 'melonAP'...");
    } else {
        // Phase 2: Nintendo WFC firmware profile / libslirp
        Wifi_AutoConnect();
        set_status(NET_WIFI_CONNECTING, "AutoConnect WFC...");
    }
}

void dtec_net_init(void) {
    close_socket();
    s_reconnect_timer = 0;
    s_wifi_search_ticks = 0;
    s_connect_phase = 0;
    s_last_assoc = -1;

    strncpy(g_dtec.wifi_ssid, DEFAULT_OBD_SSID, sizeof(g_dtec.wifi_ssid) - 1);
    strncpy(g_dtec.target_ip, DEFAULT_OBD_IP, sizeof(g_dtec.target_ip) - 1);
    g_dtec.target_port = DEFAULT_OBD_PORT;

    char cfg[48];
    snprintf(cfg, sizeof(cfg), "SSID: %s", g_dtec.wifi_ssid);
    dtec_log(cfg);
    snprintf(cfg, sizeof(cfg), "Target: %s:%u", g_dtec.target_ip, g_dtec.target_port);
    dtec_log(cfg);

    set_status(NET_WIFI_CONNECTING, "Init WiFi hardware...");

    if (!s_wifi_hw_inited) {
        if (!Wifi_InitDefault(WIFI_ATTEMPT_DSI_MODE)) {
            set_status(NET_ERROR, "DSWifi init failed");
            return;
        }
        s_wifi_hw_inited = true;
        dtec_log("WiFi HW Ready");
    }

    try_connect_current_phase();
}

void dtec_net_reconnect(void) {
    close_socket();
    if (Wifi_AssocStatus() == ASSOCSTATUS_ASSOCIATED) {
        start_tcp_connection();
    } else {
        try_connect_current_phase();
    }
}

void dtec_net_send_cmd(const char *cmd_str) {
    if (g_dtec.net_status != NET_OBD_CONNECTED || s_sock < 0 || !cmd_str) return;

    size_t len = strlen(cmd_str);
    char buf[64];
    if (len + 2 >= sizeof(buf)) return;

    memcpy(buf, cmd_str, len);
    buf[len] = '\r';
    buf[len + 1] = '\0';

    int sent = send(s_sock, buf, len + 1, 0);
    if (sent > 0) {
        g_dtec.bytes_tx += (uint32_t)sent;
        g_dtec.cmds_sent++;
        snprintf(g_dtec.last_cmd, sizeof(g_dtec.last_cmd), "> %s", cmd_str);
    }
}

bool dtec_net_is_connected(void) {
    return (g_dtec.net_status == NET_OBD_CONNECTED);
}

void dtec_net_update(void) {
    switch (g_dtec.net_status) {
        case NET_WIFI_CONNECTING: {
            int assoc = Wifi_AssocStatus();
            s_wifi_search_ticks++;

            // Live progress update on bottom link badge with elapsed seconds
            int secs = s_wifi_search_ticks / 60;
            snprintf(g_dtec.status_msg, sizeof(g_dtec.status_msg), "WiFi:%ds %s", secs, get_assoc_str(assoc));

            if (assoc != s_last_assoc) {
                s_last_assoc = assoc;
                char msg[48];
                snprintf(msg, sizeof(msg), "[%ds] %s", secs, get_assoc_str(assoc));
                dtec_log(msg);
            }

            if (assoc == ASSOCSTATUS_ACQUIRINGDHCP) {
                u32 cur_ip = Wifi_GetIP();
                if (cur_ip != 0 && cur_ip != INADDR_NONE) {
                    // VP01 DHCP sends DNS 0.0.0.0, which keeps dswifi in ACQUIRINGDHCP unless DNS is set.
                    struct in_addr gw = {0}, sn = {0}, dns1 = {0}, dns2 = {0};
                    struct in_addr my_ip = Wifi_GetIPInfo(&gw, &sn, &dns1, &dns2);
                    u32 dns = (gw.s_addr != 0) ? gw.s_addr : my_ip.s_addr;
                    Wifi_SetIP(my_ip.s_addr, gw.s_addr, sn.s_addr, dns, 0);
                    assoc = ASSOCSTATUS_ASSOCIATED;
                } else if (s_connect_phase == 0 && ++s_dhcp_ticks > 180) {
                    u32 my_ip = inet_addr("192.168.0.11");
                    u32 gw = inet_addr(g_dtec.target_ip);
                    u32 mask = inet_addr("255.255.255.0");
                    Wifi_SetIP(my_ip, gw, mask, gw, 0);
                    dtec_log("DHCP timeout, static 192.168.0.11");
                    assoc = ASSOCSTATUS_ASSOCIATED;
                }
            }

            if (assoc == ASSOCSTATUS_ASSOCIATED) {
                struct in_addr gw = {0}, sn = {0}, dns1 = {0}, dns2 = {0};
                struct in_addr my_ip = Wifi_GetIPInfo(&gw, &sn, &dns1, &dns2);

                snprintf(g_dtec.local_ip, sizeof(g_dtec.local_ip), "%s", inet_ntoa(my_ip));
                if (gw.s_addr != 0) {
                    snprintf(s_gateway_ip, sizeof(s_gateway_ip), "%s", inet_ntoa(gw));
                }

                // If running inside melonDS virtual SLiRP network (gateway is 10.0.2.2),
                // automatically route to the host computer's port 35000 at 10.0.2.2!
                if (strcmp(s_gateway_ip, "10.0.2.2") == 0 && strcmp(g_dtec.target_ip, "192.168.0.10") == 0) {
                    strncpy(g_dtec.target_ip, "10.0.2.2", sizeof(g_dtec.target_ip) - 1);
                    dtec_log("Using host 10.0.2.2 (melonDS)");
                }

                char msg[48];
                snprintf(msg, sizeof(msg), "WiFi OK: IP %s", g_dtec.local_ip);
                set_status(NET_WIFI_CONNECTED, msg);
                snprintf(msg, sizeof(msg), "Gateway %s", inet_ntoa(gw));
                dtec_log(msg);

                start_tcp_connection();
            } else if (assoc == ASSOCSTATUS_CANNOTCONNECT || s_wifi_search_ticks > 1200) {
                // Real hardware needs several seconds to scan channels, associate and get DHCP; give each AP ~20 s
                dtec_log(assoc == ASSOCSTATUS_CANNOTCONNECT ? "Cannot connect, next AP" : "Timeout 20s, next AP");
                s_connect_phase = (s_connect_phase + 1) % 3;
                try_connect_current_phase();
            }
            break;
        }

        case NET_TCP_CONNECTING: {
            if (s_sock < 0) {
                set_status(NET_DISCONNECTED, "Socket closed");
                return;
            }

            fd_set wr_set, err_set;
            FD_ZERO(&wr_set);
            FD_ZERO(&err_set);
            FD_SET(s_sock, &wr_set);
            FD_SET(s_sock, &err_set);

            struct timeval tv = {0, 0};
            int sel = select(s_sock + 1, NULL, &wr_set, &err_set, &tv);
            if (sel <= 0 && ++s_tcp_connect_ticks > 300) {
                set_status(NET_ERROR, "TCP connect timed out");
                close_socket();
                break;
            }

            if (sel > 0) {
                if (FD_ISSET(s_sock, &err_set)) {
                    // If connecting to 192.168.0.10 failed and gateway is 10.0.2.2, retry with 10.0.2.2
                    if (strcmp(g_dtec.target_ip, "192.168.0.10") == 0 && strcmp(s_gateway_ip, "10.0.2.2") == 0) {
                        strncpy(g_dtec.target_ip, "10.0.2.2", sizeof(g_dtec.target_ip) - 1);
                        dtec_log("Switching target to 10.0.2.2");
                        start_tcp_connection();
                        break;
                    }
                    set_status(NET_ERROR, "OBD socket error");
                    close_socket();
                } else if (FD_ISSET(s_sock, &wr_set)) {
                    int err = 0;
                    socklen_t len = sizeof(err);
                    if (getsockopt(s_sock, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0) {
                        set_status(NET_OBD_CONNECTED, "OBD Link Active");
                        dtec_obd_on_connected();
                    } else {
                        if (strcmp(g_dtec.target_ip, "192.168.0.10") == 0 && strcmp(s_gateway_ip, "10.0.2.2") == 0) {
                            strncpy(g_dtec.target_ip, "10.0.2.2", sizeof(g_dtec.target_ip) - 1);
                            dtec_log("Switching target to 10.0.2.2");
                            start_tcp_connection();
                            break;
                        }
                        set_status(NET_ERROR, "OBD connection refused");
                        close_socket();
                    }
                }
            }
            break;
        }

        case NET_OBD_CONNECTED: {
            if (s_sock < 0) {
                set_status(NET_DISCONNECTED, "Socket lost");
                return;
            }

            uint8_t buf[256];
            int loops = 0;
            while (loops++ < 8) {
                int n = recv(s_sock, buf, sizeof(buf) - 1, MSG_DONTWAIT);
                if (n > 0) {
                    g_dtec.bytes_rx += (uint32_t)n;
                    dtec_obd_feed_rx(buf, (size_t)n);
                } else if (n == 0) {
                    set_status(NET_DISCONNECTED, "OBD adapter closed link");
                    close_socket();
                    break;
                } else {
                    if (errno != EWOULDBLOCK && errno != EAGAIN) {
                        set_status(NET_ERROR, "OBD socket recv error");
                        close_socket();
                    }
                    break;
                }
            }
            break;
        }

        case NET_DISCONNECTED:
        case NET_ERROR: {
            s_reconnect_timer++;
            if (s_reconnect_timer > 180) { // Retry connection every ~3 seconds
                s_reconnect_timer = 0;
                dtec_net_reconnect();
            }
            break;
        }

        default:
            break;
    }
}
