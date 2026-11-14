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
#include <errno.h>

#include "net.h"
#include "common.h"
#include "config.h"
#include "ui.h"
#include "obd.h"

static int s_sock = -1;
static uint32_t s_reconnect_timer = 0;
static bool s_tried_fallback_ap = false;
static int s_wifi_search_ticks = 0;
static int s_dhcp_ticks = 0;
static int s_tcp_connect_ticks = 0;

#define WIFI_ASSOC_TIMEOUT_FRAMES  900  // ~15 s
#define TCP_CONNECT_TIMEOUT_FRAMES 300  // ~5 s

static void set_status(NetStatus st, const char *msg) {
    g_state.net_status = st;
    ui_set_status(msg);
    if (msg) {
        ui_log_system(msg);
    }
}

NetStatus net_get_status(void) {
    return g_state.net_status;
}

static void close_socket(void) {
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }
}

int net_get_rssi_pct(int16_t rssi) {
    if (rssi == 0) return 0;
    if (g_state.dsi_mode || rssi < 0) {
        // DSi mode or negative dBm: -40 dBm is ~100%, -90 dBm is ~0%
        int pct = (rssi + 90) * 2;
        if (pct < 5) pct = 5;
        if (pct > 100) pct = 100;
        return pct;
    } else {
        // DS mode: 0 to 255
        int pct = (rssi * 100) / 255;
        if (pct > 100) pct = 100;
        return pct;
    }
}

static void add_or_update_scan_ap(const char *ssid, int16_t rssi, uint8_t channel,
                                  const uint8_t *bssid, uint8_t security_type,
                                  uint16_t flags, bool is_preset) {
    if (!ssid || ssid[0] == '\0') return;

    for (int i = 0; i < g_state.scan_ap_count; i++) {
        if (strcmp(g_state.scan_aps[i].ssid, ssid) == 0) {
            if (!is_preset || g_state.scan_aps[i].is_preset) {
                g_state.scan_aps[i].rssi = rssi;
                g_state.scan_aps[i].channel = channel;
                g_state.scan_aps[i].security_type = security_type;
                g_state.scan_aps[i].flags = flags;
                if (bssid) memcpy(g_state.scan_aps[i].bssid, bssid, 6);
                g_state.scan_aps[i].is_preset = is_preset;
            }
            return;
        }
    }

    if (g_state.scan_ap_count < MAX_SCAN_APS) {
        ScanAPItem *it = &g_state.scan_aps[g_state.scan_ap_count++];
        memset(it, 0, sizeof(*it));
        snprintf(it->ssid, sizeof(it->ssid), "%s", ssid);
        it->rssi = rssi;
        it->channel = channel;
        it->security_type = security_type;
        it->flags = flags;
        if (bssid) memcpy(it->bssid, bssid, 6);
        it->is_preset = is_preset;
    }
}

void net_start_scan(void) {
    close_socket();
    Wifi_DisconnectAP();

    g_state.ui_mode = UI_MODE_WIFI_SCAN;
    g_state.scan_in_progress = true;
    g_state.scan_ticks = 0;
    g_state.scan_ap_count = 0;
    g_state.scan_selected_idx = 0;
    g_state.scan_scroll_offset = 0;
    set_status(NET_STATUS_DISCONNECTED, "Scanning Wi-Fi Networks...");

    // Put DSWifi into scan mode
    Wifi_ScanMode();

    // Pre-populate presets so user immediately has choices even before first scan tick
    if (g_config.wifi_ssid[0] != '\0') {
        add_or_update_scan_ap(g_config.wifi_ssid, 0, 1, NULL, AP_SECURITY_OPEN, 0, true);
    }
    add_or_update_scan_ap("WiFi_OBDII", 0, 1, NULL, AP_SECURITY_OPEN, 0, true);
    add_or_update_scan_ap("OBDII", 0, 1, NULL, AP_SECURITY_OPEN, 0, true);
    add_or_update_scan_ap("V-LINK", 0, 1, NULL, AP_SECURITY_OPEN, 0, true);
    add_or_update_scan_ap("CLKDevices", 0, 1, NULL, AP_SECURITY_OPEN, 0, true);
    add_or_update_scan_ap("melonAP", 0, 1, NULL, AP_SECURITY_OPEN, 0, true);
}

void net_update_scan(void) {
    if (!g_state.scan_in_progress) return;

    g_state.scan_ticks++;

    int num_aps = Wifi_GetNumAP();
    for (int i = 0; i < num_aps; i++) {
        Wifi_AccessPoint ap;
        if (Wifi_GetAPData(i, &ap) == WIFI_RETURN_OK && ap.ssid_len > 0) {
            if (ap.ssid[0] != '\0') {
                add_or_update_scan_ap(ap.ssid, ap.rssi, ap.channel, ap.bssid,
                                      (uint8_t)ap.security_type, ap.flags, false);
            }
        }
    }
}

void net_stop_scan(void) {
    g_state.scan_in_progress = false;
    g_state.ui_mode = UI_MODE_TERMINAL;
}

bool net_connect_to_ap_index(int index) {
    if (index < 0 || index >= g_state.scan_ap_count) return false;

    ScanAPItem *it = &g_state.scan_aps[index];

    // Save chosen SSID to config and SD card
    config_set_ssid(it->ssid);
    snprintf(g_state.wifi_ssid, sizeof(g_state.wifi_ssid), "%s", it->ssid);

    net_stop_scan();
    ui_clear_log();

    char msg[64];
    snprintf(msg, sizeof(msg), "Connecting to '%s'...", it->ssid);
    ui_log_system(msg);

    Wifi_AccessPoint ap;
    memset(&ap, 0, sizeof(ap));
    snprintf((char *)ap.ssid, sizeof(ap.ssid), "%s", it->ssid);
    ap.ssid_len = strlen((char *)ap.ssid);
    if (it->bssid[0] != 0 || it->bssid[1] != 0 || it->bssid[2] != 0) {
        memcpy(ap.bssid, it->bssid, 6);
    }
    ap.channel = it->channel;

    Wifi_SetIP(0, 0, 0, 0, 0);
    if (it->security_type == AP_SECURITY_OPEN || g_config.wifi_key[0] == '\0') {
        Wifi_ConnectOpenAP(&ap);
    } else {
        Wifi_ConnectSecureAP(&ap, (const unsigned char *)g_config.wifi_key, strlen(g_config.wifi_key));
    }

    set_status(NET_STATUS_WIFI_CONNECTING, "Associating with selected AP...");
    s_wifi_search_ticks = 0;
    s_dhcp_ticks = 0;
    return true;
}

static bool start_tcp_connection(void) {
    close_socket();

    s_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (s_sock < 0) {
        set_status(NET_STATUS_ERROR, "Socket create failed");
        return false;
    }

    int one = 1;
    setsockopt(s_sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    int flags = fcntl(s_sock, F_GETFL, 0);
    fcntl(s_sock, F_SETFL, flags | O_NONBLOCK);

    const char *target_ip = g_config.obd_ip;

    snprintf(g_state.target_ip, sizeof(g_state.target_ip), "%s", target_ip);
    g_state.target_port = g_config.obd_port;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(g_config.obd_port);
    serv_addr.sin_addr.s_addr = inet_addr(target_ip);

    char msg[64];
    snprintf(msg, sizeof(msg), "Connecting to %s:%u...", target_ip, (unsigned)g_config.obd_port);
    ui_log_system(msg);

    s_tcp_connect_ticks = 0;
    int res = connect(s_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (res == 0 || (res < 0 && errno == EINPROGRESS)) {
        set_status(NET_STATUS_TCP_CONNECTING, "Connecting TCP...");
        return true;
    }

    close_socket();
    set_status(NET_STATUS_ERROR, "Socket connect failed");
    s_reconnect_timer = 120; // 2 seconds
    return false;
}

bool net_init(void) {
    static bool s_wifi_hw_inited = false;
    close_socket();
    s_reconnect_timer = 0;
    s_wifi_search_ticks = 0;
    s_dhcp_ticks = 0;
    s_tried_fallback_ap = false;

    set_status(NET_STATUS_WIFI_CONNECTING, "Initializing WiFi Hardware...");

    if (!s_wifi_hw_inited) {
        if (!Wifi_InitDefault(INIT_ONLY | WIFI_ATTEMPT_DSI_MODE)) {
            set_status(NET_STATUS_ERROR, "DSWifi Init Failed");
            return false;
        }
        s_wifi_hw_inited = true;
    }

    Wifi_SetIP(0, 0, 0, 0, 0);

    if (g_config.wifi_ssid[0] != '\0') {
        Wifi_AccessPoint ap;
        memset(&ap, 0, sizeof(ap));
        snprintf((char *)ap.ssid, sizeof(ap.ssid), "%s", g_config.wifi_ssid);
        ap.ssid_len = strlen((char *)ap.ssid);

        if (g_config.wifi_key[0] != '\0') {
            Wifi_ConnectSecureAP(&ap, (const unsigned char *)g_config.wifi_key, strlen(g_config.wifi_key));
        } else {
            Wifi_ConnectOpenAP(&ap);
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "Associating with '%s'...", g_config.wifi_ssid);
        set_status(NET_STATUS_WIFI_CONNECTING, msg);
    } else {
        Wifi_AutoConnect();
        set_status(NET_STATUS_WIFI_CONNECTING, "Auto-connecting WFC...");
    }

    return true;
}

void net_reconnect(void) {
    close_socket();
    if (Wifi_AssocStatus() == ASSOCSTATUS_ASSOCIATED) {
        start_tcp_connection();
    } else {
        net_init();
    }
}

bool net_send(const void *data, size_t len) {
    if (g_state.net_status != NET_STATUS_TCP_CONNECTED || s_sock < 0 || !data || len == 0) {
        return false;
    }

    int sent = send(s_sock, data, len, 0);
    if (sent > 0) {
        g_state.stats.bytes_tx += (uint32_t)sent;
    }
    // A partial send or EAGAIN counts as a failure; the OBD core logs it and retries.
    return sent == (int)len;
}

bool net_send_cmd(const char *cmd_str) {
    if (!cmd_str) return false;

    size_t len = strlen(cmd_str);
    char buf[128];
    if (len + 2 >= sizeof(buf)) return false;

    memcpy(buf, cmd_str, len);
    buf[len] = '\r'; // ELM327 carriage return terminator
    buf[len + 1] = '\0';

    return net_send(buf, len + 1);
}

void net_update(void) {
    // When in Wi-Fi scanning mode, update beacon scan results
    if (g_state.ui_mode == UI_MODE_WIFI_SCAN) {
        net_update_scan();
        return;
    }

    switch (g_state.net_status) {
        case NET_STATUS_WIFI_CONNECTING: {
            int assoc = Wifi_AssocStatus();
            s_wifi_search_ticks++;

            if (assoc == ASSOCSTATUS_ACQUIRINGDHCP) {
                u32 cur_ip = Wifi_GetIP();
                if (cur_ip != 0 && cur_ip != INADDR_NONE) {
                    // VP01 DHCP sends DNS 0.0.0.0, which keeps dswifi in ACQUIRINGDHCP unless DNS is set.
                    struct in_addr gw = {0}, sn = {0}, dns1 = {0}, dns2 = {0};
                    struct in_addr my_ip = Wifi_GetIPInfo(&gw, &sn, &dns1, &dns2);
                    u32 dns = (gw.s_addr != 0) ? gw.s_addr : my_ip.s_addr;
                    Wifi_SetIP(my_ip.s_addr, gw.s_addr, sn.s_addr, dns, 0);
                    assoc = ASSOCSTATUS_ASSOCIATED;
                } else if (strcmp(g_config.obd_ip, "192.168.0.10") == 0 && ++s_dhcp_ticks > 180) {
                    u32 my_ip = inet_addr("192.168.0.11");
                    u32 gw = inet_addr(g_config.obd_ip);
                    u32 mask = inet_addr("255.255.255.0");
                    Wifi_SetIP(my_ip, gw, mask, gw, 0);
                    ui_log_system("DHCP timeout, static 192.168.0.11");
                    assoc = ASSOCSTATUS_ASSOCIATED;
                }
            }

            if (assoc == ASSOCSTATUS_ASSOCIATED) {
                struct in_addr gw = {0}, sn = {0}, dns1 = {0}, dns2 = {0};
                struct in_addr my_ip = Wifi_GetIPInfo(&gw, &sn, &dns1, &dns2);

                snprintf(g_state.local_ip, sizeof(g_state.local_ip), "%s", inet_ntoa(my_ip));
                if (gw.s_addr != 0) {
                    snprintf(g_state.gateway_ip, sizeof(g_state.gateway_ip), "%s", inet_ntoa(gw));
                }

                char msg[64];
                snprintf(msg, sizeof(msg), "WiFi OK: IP %s", g_state.local_ip);
                set_status(NET_STATUS_WIFI_CONNECTED, msg);

                start_tcp_connection();
            } else if (assoc == ASSOCSTATUS_CANNOTCONNECT && !s_tried_fallback_ap) {
                s_tried_fallback_ap = true;
                s_wifi_search_ticks = 0;

                Wifi_AccessPoint melon_ap;
                memset(&melon_ap, 0, sizeof(melon_ap));
                strcpy((char *)melon_ap.ssid, "melonAP");
                melon_ap.ssid_len = 7;
                Wifi_ConnectOpenAP(&melon_ap);

                set_status(NET_STATUS_WIFI_CONNECTING, "Retrying with melonAP...");
            } else if ((assoc == ASSOCSTATUS_CANNOTCONNECT && s_tried_fallback_ap) ||
                       s_wifi_search_ticks > WIFI_ASSOC_TIMEOUT_FRAMES) {
                set_status(NET_STATUS_ERROR, assoc == ASSOCSTATUS_CANNOTCONNECT ?
                           "Cannot associate with AP" : "Wi-Fi association timed out");
                ui_log_system("Press [SELECT] to choose a Wi-Fi AP");
                s_reconnect_timer = 180; // 3 seconds
            }
            break;
        }

        case NET_STATUS_WIFI_CONNECTED: {
            if (s_sock < 0) {
                start_tcp_connection();
            }
            break;
        }

        case NET_STATUS_TCP_CONNECTING: {
            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(s_sock, &write_fds);

            struct timeval tv = {0, 0};
            int sel = select(s_sock + 1, NULL, &write_fds, NULL, &tv);
            if (sel <= 0 && ++s_tcp_connect_ticks > TCP_CONNECT_TIMEOUT_FRAMES) {
                close_socket();
                set_status(NET_STATUS_ERROR, "TCP connect timed out");
                s_reconnect_timer = 120; // 2 seconds
                break;
            }
            if (sel > 0) {
                int err = 0;
                socklen_t len = sizeof(err);
                getsockopt(s_sock, SOL_SOCKET, SO_ERROR, &err, &len);
                if (err == 0) {
                    set_status(NET_STATUS_TCP_CONNECTED, "OBDLink Connected!");
                    obd_on_connected();
                } else {
                    if (strcmp(g_state.target_ip, "192.168.0.10") == 0 &&
                        g_state.gateway_ip[0] != '\0' &&
                        strcmp(g_state.gateway_ip, "10.0.2.2") == 0) {
                        ui_log_system("Trying emulator gateway 10.0.2.2...");
                        strncpy(g_config.obd_ip, "10.0.2.2", sizeof(g_config.obd_ip) - 1);
                        start_tcp_connection();
                        break;
                    }

                    close_socket();
                    set_status(NET_STATUS_ERROR, "TCP Connect Error");
                    s_reconnect_timer = 120; // 2 seconds
                }
            }
            break;
        }

        case NET_STATUS_TCP_CONNECTED: {
            uint8_t rx_buf[512];
            int loops = 0;

            while (loops++ < 8) {
                int n = recv(s_sock, rx_buf, sizeof(rx_buf), 0);
                if (n > 0) {
                    g_state.stats.bytes_rx += (uint32_t)n;
                    g_state.stats.rx_packets++;
                    obd_feed_rx_data(rx_buf, (size_t)n);
                } else if (n == 0) {
                    close_socket();
                    set_status(NET_STATUS_ERROR, "Adapter closed socket");
                    s_reconnect_timer = 120;
                    break;
                } else {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        close_socket();
                        set_status(NET_STATUS_ERROR, "Socket error");
                        s_reconnect_timer = 120;
                    }
                    break;
                }
            }
            break;
        }

        case NET_STATUS_ERROR:
        case NET_STATUS_DISCONNECTED: {
            if (s_reconnect_timer > 0) {
                s_reconnect_timer--;
                if (s_reconnect_timer == 0) {
                    if (Wifi_AssocStatus() == ASSOCSTATUS_ASSOCIATED) {
                        start_tcp_connection();
                    } else {
                        net_init();
                    }
                }
            }
            break;
        }
    }
}
