#include <pspkernel.h>
#include <pspwlan.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <psputility_netparam.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
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
static int s_tcp_connect_ticks = 0;
#define TCP_CONNECT_TIMEOUT_FRAMES 300  // ~5 s
static bool s_net_hw_inited = false;
static uint32_t s_reconnect_timer = 0;
static bool s_tried_ppsspp_loopback = false;

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
        sceNetInetClose(s_sock);
        s_sock = -1;
    }
}

static void add_scan_entry(const char *ssid, const char *profile_name, const char *ip,
                           uint16_t port, int netparam_id, uint8_t sec, int16_t rssi, bool is_preset) {
    if (!ssid || ssid[0] == '\0') return;
    if (g_state.scan_ap_count >= MAX_SCAN_APS) return;

    for (int i = 0; i < g_state.scan_ap_count; i++) {
        if (strcmp(g_state.scan_aps[i].ssid, ssid) == 0 &&
            strcmp(g_state.scan_aps[i].target_ip, ip) == 0 &&
            g_state.scan_aps[i].netparam_id == netparam_id) {
            return;
        }
    }

    ScanAPItem *it = &g_state.scan_aps[g_state.scan_ap_count++];
    memset(it, 0, sizeof(*it));
    snprintf(it->ssid, sizeof(it->ssid), "%s", ssid);
    snprintf(it->profile_name, sizeof(it->profile_name), "%s",
             (profile_name && profile_name[0]) ? profile_name : ssid);
    snprintf(it->target_ip, sizeof(it->target_ip), "%s", ip);
    it->target_port = port;
    it->netparam_id = netparam_id;
    it->security_type = sec;
    it->rssi = rssi;
    it->is_preset = is_preset;
}

void net_start_scan(void) {
    close_socket();

    g_state.ui_mode = UI_MODE_WIFI_SCAN;
    g_state.scan_in_progress = true;
    g_state.scan_ticks = 0;
    g_state.scan_ap_count = 0;
    g_state.scan_selected_idx = 0;
    g_state.scan_scroll_offset = 0;

    // 1. Read saved PSP XMB Network Connections (Slots 1..10)
    for (int id = 1; id <= 10; id++) {
        if (sceUtilityCheckNetParam(id) == 0) {
            netData name_data;
            netData ssid_data;
            netData sec_data;
            memset(&name_data, 0, sizeof(name_data));
            memset(&ssid_data, 0, sizeof(ssid_data));
            memset(&sec_data, 0, sizeof(sec_data));

            sceUtilityGetNetParam(id, PSP_NETPARAM_NAME, &name_data);
            sceUtilityGetNetParam(id, PSP_NETPARAM_SSID, &ssid_data);
            sceUtilityGetNetParam(id, PSP_NETPARAM_SECURE, &sec_data);

            if (ssid_data.asString[0] != '\0') {
                add_scan_entry(ssid_data.asString, name_data.asString, "192.168.0.10", 35000,
                               id, (uint8_t)sec_data.asUint, 95, false);
            }
        }
    }

    // 2. Check active APCTL connection info if already associated
    int ap_state = 0;
    if (sceNetApctlGetState(&ap_state) == 0 && ap_state == PSP_NET_APCTL_STATE_GOT_IP) {
        SceNetApctlInfo ssid_info;
        SceNetApctlInfo str_info;
        memset(&ssid_info, 0, sizeof(ssid_info));
        memset(&str_info, 0, sizeof(str_info));
        if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_SSID, &ssid_info) == 0 && ssid_info.ssid[0] != '\0') {
            sceNetApctlGetInfo(PSP_NET_APCTL_INFO_STRENGTH, &str_info);
            add_scan_entry((const char *)ssid_info.ssid, "Active WLAN", g_config.obd_ip, g_config.obd_port,
                           g_config.apctl_profile_index, 0, str_info.strength ? str_info.strength : 100, false);
        }
    }

    // 3. Add standard OBDCheck VP01 and ELM327 presets + PPSSPP Emulator Loopback
    if (g_config.wifi_ssid[0] != '\0') {
        add_scan_entry(g_config.wifi_ssid, "Saved Config", g_config.obd_ip, g_config.obd_port,
                       g_config.apctl_profile_index, 0, 90, true);
    }
    add_scan_entry("WiFi_OBDII", "Veepeak VP01", "192.168.0.10", 35000, 1, 0, 95, true);
    add_scan_entry("OBDII", "ELM327 Generic", "192.168.0.10", 35000, 1, 0, 80, true);
    add_scan_entry("V-LINK", "vGate V-Link", "192.168.0.10", 35000, 1, 0, 75, true);
    add_scan_entry("CLKDevices", "CLK OBDWiFi", "192.168.0.10", 35000, 1, 0, 70, true);
    add_scan_entry("PPSSPP Loopback", "Emulator Host", "127.0.0.1", 35000, 0, 0, 100, true);
}

void net_update_scan(void) {
    if (!g_state.scan_in_progress) return;
    g_state.scan_ticks++;
    g_state.wlan_switch_on = (sceWlanGetSwitchState() != 0);
}

void net_stop_scan(void) {
    g_state.scan_in_progress = false;
    g_state.ui_mode = UI_MODE_TERMINAL;
}

static bool start_tcp_connection(void) {
    close_socket();

    s_sock = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
    if (s_sock < 0) {
        set_status(NET_STATUS_ERROR, "Socket create failed");
        s_reconnect_timer = 90;
        return false;
    }

    int nonblock = 1;
    sceNetInetSetsockopt(s_sock, SOL_SOCKET, SO_NONBLOCK, &nonblock, sizeof(nonblock));

    // Disable Nagle so each short ELM327 command is flushed immediately (some
    // WiFi adapters won't reply until they do). Best-effort on the PSP inet stack.
    int nodelay = 1;
    sceNetInetSetsockopt(s_sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

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

    int res = sceNetInetConnect(s_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    int err = sceNetInetGetErrno();
    if (res == 0) {
        set_status(NET_STATUS_TCP_CONNECTED, "OBDLink Connected!");
        obd_on_connected();
        return true;
    } else if (err == EINPROGRESS || err == EALREADY || err == EWOULDBLOCK) {
        s_tcp_connect_ticks = 0;
        set_status(NET_STATUS_TCP_CONNECTING, "Connecting TCP...");
        s_reconnect_timer = 120;
        return true;
    }

    close_socket();
    set_status(NET_STATUS_ERROR, "Socket connect failed");
    s_reconnect_timer = 90;
    return false;
}

bool net_connect_to_ap_index(int index) {
    if (index < 0 || index >= g_state.scan_ap_count) return false;

    ScanAPItem *it = &g_state.scan_aps[index];

    config_set_ssid_and_ip(it->ssid, it->target_ip, it->target_port, it->netparam_id);
    snprintf(g_state.wifi_ssid, sizeof(g_state.wifi_ssid), "%s", it->ssid);
    snprintf(g_state.target_ip, sizeof(g_state.target_ip), "%s", it->target_ip);
    g_state.target_port = it->target_port;
    g_state.apctl_profile_index = it->netparam_id;

    net_stop_scan();
    ui_clear_log();

    char msg[64];
    snprintf(msg, sizeof(msg), "Selected '%s' (%s:%u)", it->ssid, it->target_ip, (unsigned)it->target_port);
    ui_log_system(msg);

    s_tried_ppsspp_loopback = false;

    // Re-associate APCTL if switching PSP WLAN profile
    sceNetApctlDisconnect();
    if (sceNetApctlConnect(it->netparam_id > 0 ? it->netparam_id : 0) < 0) {
        sceNetApctlConnect(0);
    }
    set_status(NET_STATUS_WIFI_CONNECTING, "Joining selected WLAN...");
    s_reconnect_timer = 0;
    return true;
}

bool net_init(void) {
    close_socket();
    s_reconnect_timer = 0;
    s_tried_ppsspp_loopback = false;

    g_state.wlan_switch_on = (sceWlanGetSwitchState() != 0);

    if (!s_net_hw_inited) {
        sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
        sceUtilityLoadNetModule(PSP_NET_MODULE_INET);

        if (sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024) < 0) {
            set_status(NET_STATUS_ERROR, "sceNetInit Failed");
            return false;
        }
        if (sceNetInetInit() < 0) {
            set_status(NET_STATUS_ERROR, "sceNetInetInit Failed");
            return false;
        }
        if (sceNetApctlInit(0x8000, 48) < 0) {
            set_status(NET_STATUS_ERROR, "sceNetApctlInit Failed");
            return false;
        }
        s_net_hw_inited = true;
    }

    int state = 0;
    if (sceNetApctlGetState(&state) == 0 && state == PSP_NET_APCTL_STATE_GOT_IP) {
        set_status(NET_STATUS_WIFI_CONNECTED, "WLAN Ready");
        return start_tcp_connection();
    }

    if (sceNetApctlConnect(g_config.apctl_profile_index) < 0) {
        sceNetApctlConnect(0);
    }
    set_status(NET_STATUS_WIFI_CONNECTING, "Joining WLAN...");
    return true;
}

void net_reconnect(void) {
    close_socket();
    s_tried_ppsspp_loopback = false;
    if (g_state.net_status >= NET_STATUS_WIFI_CONNECTED) {
        start_tcp_connection();
    } else {
        net_init();
    }
}

bool net_send(const void *data, size_t len) {
    if (g_state.net_status != NET_STATUS_TCP_CONNECTED || s_sock < 0 || !data || len == 0) {
        return false;
    }

    int sent = (int)sceNetInetSend(s_sock, data, len, MSG_DONTWAIT);
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
    buf[len] = '\r';
    buf[len + 1] = '\0';

    return net_send(buf, len + 1);
}

void net_update(void) {
    g_state.wlan_switch_on = (sceWlanGetSwitchState() != 0);

    if (g_state.ui_mode == UI_MODE_WIFI_SCAN) {
        net_update_scan();
        return;
    }

    switch (g_state.net_status) {
        case NET_STATUS_WIFI_CONNECTING: {
            int state = 0;
            if (sceNetApctlGetState(&state) == 0 && state == PSP_NET_APCTL_STATE_GOT_IP) {
                SceNetApctlInfo ip_info;
                if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &ip_info) == 0) {
                    snprintf(g_state.local_ip, sizeof(g_state.local_ip), "%s", ip_info.ip);
                }
                SceNetApctlInfo gw_info;
                if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_GATEWAY, &gw_info) == 0 && gw_info.gateway[0]) {
                    snprintf(g_state.gateway_ip, sizeof(g_state.gateway_ip), "%s", gw_info.gateway);
                }
                set_status(NET_STATUS_WIFI_CONNECTED, "WLAN Connected");
                start_tcp_connection();
            } else {
                // In PPSSPP emulator, sockets work directly even before APCTL finishes
                s_reconnect_timer++;
                if (s_reconnect_timer > 30) {
                    s_reconnect_timer = 0;
                    snprintf(g_state.local_ip, sizeof(g_state.local_ip), "127.0.0.1 (PPSSPP)");
                    set_status(NET_STATUS_WIFI_CONNECTED, "WLAN / Loopback Ready");
                    start_tcp_connection();
                }
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
            if (++s_tcp_connect_ticks > TCP_CONNECT_TIMEOUT_FRAMES) {
                s_tcp_connect_ticks = 0;
                close_socket();
                set_status(NET_STATUS_ERROR, "TCP connect timed out");
                ui_log_system("Press [SELECT] to choose Wi-Fi network");
                s_reconnect_timer = 120;
                break;
            }

            SceNetInetPollfd pfd;
            pfd.fd = s_sock;
            pfd.events = SCE_NET_INET_POLLOUT;
            pfd.revents = 0;
            int sel = sceNetInetPoll(&pfd, 1, 0);
            if (sel > 0 && (pfd.revents & SCE_NET_INET_POLLOUT)) {
                int err = 0;
                socklen_t len = sizeof(err);
                sceNetInetGetsockopt(s_sock, SOL_SOCKET, SO_ERROR, &err, &len);
                if (err == 0) {
                    set_status(NET_STATUS_TCP_CONNECTED, "OBDLink Connected!");
                    obd_on_connected();
                } else {
                    close_socket();
                    if (!s_tried_ppsspp_loopback && strcmp(g_config.obd_ip, "192.168.0.10") == 0) {
                        s_tried_ppsspp_loopback = true;
                        ui_log_system("Trying PPSSPP loopback 127.0.0.1...");
                        snprintf(g_config.obd_ip, sizeof(g_config.obd_ip), "127.0.0.1");
                        start_tcp_connection();
                        break;
                    }
                    set_status(NET_STATUS_ERROR, "TCP Connect Refused");
                    ui_log_system("Press [SELECT] to choose Wi-Fi network");
                    s_reconnect_timer = 120;
                }
            } else {
                if (s_reconnect_timer > 0) {
                    s_reconnect_timer--;
                    if (s_reconnect_timer == 0) {
                        close_socket();
                        if (!s_tried_ppsspp_loopback && strcmp(g_config.obd_ip, "192.168.0.10") == 0) {
                            s_tried_ppsspp_loopback = true;
                            ui_log_system("Trying PPSSPP loopback 127.0.0.1...");
                            snprintf(g_config.obd_ip, sizeof(g_config.obd_ip), "127.0.0.1");
                            start_tcp_connection();
                            break;
                        }
                        set_status(NET_STATUS_ERROR, "Connect Timeout");
                        ui_log_system("Press [SELECT] to choose Wi-Fi network");
                        s_reconnect_timer = 120;
                    }
                }
            }
            break;
        }

        case NET_STATUS_TCP_CONNECTED: {
            uint8_t rx_buf[512];
            int loops = 0;

            while (loops++ < 8) {
                int n = (int)sceNetInetRecv(s_sock, rx_buf, sizeof(rx_buf), MSG_DONTWAIT);
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
                    int err = sceNetInetGetErrno();
                    if (err != EAGAIN && err != EWOULDBLOCK && err != EINPROGRESS) {
                        close_socket();
                        set_status(NET_STATUS_ERROR, "Socket Read Error");
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
                    start_tcp_connection();
                }
            }
            break;
        }
    }
}
