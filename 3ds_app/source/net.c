#include <3ds.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>

#include "net.h"
#include "common.h"
#include "config.h"
#include "ui.h"
#include "obd.h"

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000

static u32 *s_soc_buffer = NULL;
static int s_sock = -1;
static uint32_t s_reconnect_timer = 0;
static bool s_tried_emu_loopback = false;
static bool s_use_emu_loopback = false;

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
        closesocket(s_sock);
        s_sock = -1;
    }
}

static void add_scan_entry(const char *ssid, const char *ip, uint16_t port,
                           uint8_t slot_id, uint8_t sec, int16_t rssi, bool is_preset) {
    if (!ssid || ssid[0] == '\0') return;
    if (g_state.scan_ap_count >= MAX_SCAN_APS) return;

    for (int i = 0; i < g_state.scan_ap_count; i++) {
        if (strcmp(g_state.scan_aps[i].ssid, ssid) == 0 &&
            strcmp(g_state.scan_aps[i].target_ip, ip) == 0) {
            return;
        }
    }

    ScanAPItem *it = &g_state.scan_aps[g_state.scan_ap_count++];
    memset(it, 0, sizeof(*it));
    snprintf(it->ssid, sizeof(it->ssid), "%s", ssid);
    snprintf(it->target_ip, sizeof(it->target_ip), "%s", ip);
    it->target_port = port;
    it->slot_id = slot_id;
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

    // 1. Check active 3DS AC Wi-Fi SSID if connected
    if (R_SUCCEEDED(acInit())) {
        char active_ssid[33];
        memset(active_ssid, 0, sizeof(active_ssid));
        if (R_SUCCEEDED(ACU_GetSSID(active_ssid)) && active_ssid[0] != '\0') {
            acSecurityMode sec = AC_OPEN;
            ACU_GetSecurityMode(&sec);
            add_scan_entry(active_ssid, g_config.obd_ip, g_config.obd_port, 0,
                           (sec == AC_OPEN) ? 0 : 3, 100, false);
        }

        // 2. Read 3DS System Settings Wi-Fi Slots 1, 2, 3 via ac:i if available
        for (u32 slot = 0; slot < 3; slot++) {
            char slot_ssid[33];
            memset(slot_ssid, 0, sizeof(slot_ssid));
            if (R_SUCCEEDED(ACI_LoadNetworkSetting(slot)) &&
                R_SUCCEEDED(ACI_GetNetworkWirelessEssidSecuritySsid(slot_ssid)) &&
                slot_ssid[0] != '\0') {
                add_scan_entry(slot_ssid, "192.168.0.10", 35000, (uint8_t)(slot + 1), 0, 90, false);
            }
        }
        acExit();
    }

    // 3. Populate standard OBD-II Wi-Fi adapter presets and Emulator loopback
    if (g_config.wifi_ssid[0] != '\0') {
        add_scan_entry(g_config.wifi_ssid, g_config.obd_ip, g_config.obd_port, 0, 0, 85, true);
    }
    add_scan_entry("WiFi_OBDII", "192.168.0.10", 35000, 1, 0, 95, true);
    add_scan_entry("OBDII", "192.168.0.10", 35000, 1, 0, 80, true);
    add_scan_entry("V-LINK", "192.168.0.10", 35000, 1, 0, 75, true);
    add_scan_entry("CLKDevices", "192.168.0.10", 35000, 1, 0, 70, true);
    add_scan_entry("Citra/Azahar Emu", "127.0.0.1", 35000, 0, 0, 100, true);
}

void net_update_scan(void) {
    if (!g_state.scan_in_progress) return;
    g_state.scan_ticks++;
    g_state.wifi_bars = osGetWifiStrength();
}

void net_stop_scan(void) {
    g_state.scan_in_progress = false;
    g_state.ui_mode = UI_MODE_TERMINAL;
}

static bool start_tcp_connection(void) {
    close_socket();

    s_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (s_sock < 0) {
        set_status(NET_STATUS_ERROR, "Socket create failed");
        s_reconnect_timer = 120;
        return false;
    }

    int flags = fcntl(s_sock, F_GETFL, 0);
    if (flags < 0) flags = 0;
    fcntl(s_sock, F_SETFL, flags | O_NONBLOCK);

    // Disable Nagle: the ELM327 handshake is one short line at a time, and some
    // WiFi adapters won't answer until the command's bytes are actually flushed.
    int nodelay = 1;
    setsockopt(s_sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    struct in_addr host_ip;
    host_ip.s_addr = gethostid();
    if (host_ip.s_addr != 0) {
        snprintf(g_state.local_ip, sizeof(g_state.local_ip), "%s", inet_ntoa(host_ip));
    } else {
        snprintf(g_state.local_ip, sizeof(g_state.local_ip), "127.0.0.1");
    }

    const char *target_ip = s_use_emu_loopback ? "127.0.0.1" : g_config.obd_ip;
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

    int res = connect(s_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (res == 0) {
        set_status(NET_STATUS_TCP_CONNECTED, "OBDLink Connected!");
        obd_on_connected();
        return true;
    } else if (res < 0 && errno == EINPROGRESS) {
        set_status(NET_STATUS_TCP_CONNECTING, "Connecting TCP...");
        s_reconnect_timer = 0;
        return true;
    }

    close_socket();
    set_status(NET_STATUS_ERROR, "Socket connect failed");
    s_reconnect_timer = 120;
    return false;
}

bool net_connect_to_ap_index(int index) {
    if (index < 0 || index >= g_state.scan_ap_count) return false;

    ScanAPItem *it = &g_state.scan_aps[index];

    config_set_ssid_and_ip(it->ssid, it->target_ip, it->target_port);
    snprintf(g_state.wifi_ssid, sizeof(g_state.wifi_ssid), "%s", it->ssid);
    snprintf(g_state.target_ip, sizeof(g_state.target_ip), "%s", it->target_ip);
    g_state.target_port = it->target_port;

    net_stop_scan();
    ui_clear_log();

    char msg[64];
    snprintf(msg, sizeof(msg), "Selected '%s' (%s:%u)", it->ssid, it->target_ip, (unsigned)it->target_port);
    ui_log_system(msg);

    s_tried_emu_loopback = false;
    s_use_emu_loopback = false;
    return start_tcp_connection();
}

bool net_init(void) {
    close_socket();
    s_reconnect_timer = 0;
    s_tried_emu_loopback = false;
    s_use_emu_loopback = false;

    set_status(NET_STATUS_WIFI_CONNECTING, "Initializing 3DS SOC...");

    if (!s_soc_buffer) {
        s_soc_buffer = (u32 *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
        if (!s_soc_buffer) {
            set_status(NET_STATUS_ERROR, "SOC Buffer Alloc Failed");
            return false;
        }

        Result res = socInit(s_soc_buffer, SOC_BUFFERSIZE);
        if (R_FAILED(res)) {
            set_status(NET_STATUS_ERROR, "socInit Failed");
            free(s_soc_buffer);
            s_soc_buffer = NULL;
            return false;
        }
    }

    if (R_SUCCEEDED(acInit())) {
        char active_ssid[33];
        memset(active_ssid, 0, sizeof(active_ssid));
        if (R_SUCCEEDED(ACU_GetSSID(active_ssid)) && active_ssid[0] != '\0') {
            snprintf(g_state.wifi_ssid, sizeof(g_state.wifi_ssid), "%s", active_ssid);
        }
        acExit();
    }

    struct in_addr host_ip;
    host_ip.s_addr = gethostid();
    if (host_ip.s_addr != 0) {
        snprintf(g_state.local_ip, sizeof(g_state.local_ip), "%s", inet_ntoa(host_ip));
    } else {
        snprintf(g_state.local_ip, sizeof(g_state.local_ip), "127.0.0.1");
    }

    g_state.wifi_bars = osGetWifiStrength();
    set_status(NET_STATUS_WIFI_CONNECTED, "3DS WiFi Ready");
    return start_tcp_connection();
}

void net_cleanup(void) {
    close_socket();
    if (s_soc_buffer) {
        socExit();
        free(s_soc_buffer);
        s_soc_buffer = NULL;
    }
}

void net_reconnect(void) {
    close_socket();
    s_tried_emu_loopback = false;
    s_use_emu_loopback = false;
    start_tcp_connection();
}

bool net_send(const void *data, size_t len) {
    if (g_state.net_status != NET_STATUS_TCP_CONNECTED || s_sock < 0 || !data || len == 0) {
        return false;
    }

    ssize_t sent = send(s_sock, data, len, 0);
    if (sent > 0) {
        g_state.stats.bytes_tx += (uint32_t)sent;
    }
    // A partial send or EAGAIN counts as a failure; the OBD core logs it and retries.
    return sent == (ssize_t)len;
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
    g_state.wifi_bars = osGetWifiStrength();

    if (g_state.ui_mode == UI_MODE_WIFI_SCAN) {
        net_update_scan();
        return;
    }

    switch (g_state.net_status) {
        case NET_STATUS_WIFI_CONNECTING:
        case NET_STATUS_WIFI_CONNECTED: {
            if (s_sock < 0) {
                start_tcp_connection();
            }
            break;
        }

        case NET_STATUS_TCP_CONNECTING: {
            s_reconnect_timer++;
            if (s_reconnect_timer >= 300) {
                s_reconnect_timer = 0;
                close_socket();
                if (!s_tried_emu_loopback && gethostid() == 0 && strcmp(g_config.obd_ip, "192.168.0.10") == 0) {
                    s_tried_emu_loopback = true;
                    s_use_emu_loopback = true;
                    ui_log_system("Trying emulator loopback 127.0.0.1...");
                    start_tcp_connection();
                    return;
                }
                s_use_emu_loopback = false;
                set_status(NET_STATUS_ERROR, "Connect timeout");
                ui_log_system("Press [SELECT] to choose Wi-Fi / IP");
                return;
            }

            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(s_sock, &wfds);

            struct timeval tv = {0, 0};
            int sel = select(s_sock + 1, NULL, &wfds, NULL, &tv);
            if (sel > 0 && FD_ISSET(s_sock, &wfds)) {
                struct sockaddr_in peer;
                socklen_t plen = sizeof(peer);
                int err = 0;
                socklen_t elen = sizeof(err);
                getsockopt(s_sock, SOL_SOCKET, SO_ERROR, &err, &elen);

                if (err == 0 || getpeername(s_sock, (struct sockaddr *)&peer, &plen) == 0) {
                    set_status(NET_STATUS_TCP_CONNECTED, "OBDLink Connected!");
                    obd_on_connected();
                } else {
                    close_socket();
                    if (!s_tried_emu_loopback && gethostid() == 0 && strcmp(g_config.obd_ip, "192.168.0.10") == 0) {
                        s_tried_emu_loopback = true;
                        s_use_emu_loopback = true;
                        ui_log_system("Trying emulator loopback 127.0.0.1...");
                        start_tcp_connection();
                        return;
                    }
                    s_use_emu_loopback = false;
                    set_status(NET_STATUS_ERROR, "TCP Connect Refused");
                    s_reconnect_timer = 120;
                }
            }
            break;
        }

        case NET_STATUS_TCP_CONNECTED: {
            uint8_t rx_buf[512];
            int loops = 0;

            while (loops++ < 8) {
                ssize_t n = recv(s_sock, rx_buf, sizeof(rx_buf), 0);
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
            s_reconnect_timer++;
            if (s_reconnect_timer >= 180) {
                s_reconnect_timer = 0;
                start_tcp_connection();
            }
            break;
        }
    }
}
