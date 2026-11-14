#include <3ds.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <malloc.h>
#include <errno.h>

#include "dtec_common.h"
#include "dtec_net.h"
#include "dtec_obd.h"
#include "ini.h"
#include "platform_3ds.h"

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000

static u32 *s_soc_buffer = NULL;
static int s_sock = -1;
static uint32_t s_reconnect_timer = 0;
static uint32_t s_tcp_connect_ticks = 0;
static bool s_tried_emu_loopback = false;
static char s_cfg_ip[20] = DEFAULT_OBD_IP;
static bool s_ac_ok = false;
static Handle s_join_evt = 0;
static int s_join_slot = -1;
static uint32_t s_join_ticks = 0;

char g_3ds_slot_ssid[DTEC_3DS_WIFI_SLOTS][33];

static void set_status(DTEC_NetStatus st, const char *msg) {
    g_dtec.net_status = st;
    if (msg) {
        snprintf(g_dtec.status_msg, sizeof(g_dtec.status_msg), "%s", msg);
        dtec_log(msg);
    }
}

static void close_socket(void) {
    if (s_sock >= 0) {
        closesocket(s_sock);
        s_sock = -1;
    }
}

static void apply_ini_kv(const char *key, const char *val, void *user) {
    (void)user;
    if (strcasecmp(key, "ssid") == 0 && val[0]) {
        snprintf(g_dtec.wifi_ssid, sizeof(g_dtec.wifi_ssid), "%s", val);
    } else if ((strcasecmp(key, "ip") == 0 || strcasecmp(key, "server_ip") == 0) && val[0]) {
        snprintf(s_cfg_ip, sizeof(s_cfg_ip), "%s", val);
        snprintf(g_dtec.target_ip, sizeof(g_dtec.target_ip), "%s", val);
    } else if (strcasecmp(key, "port") == 0) {
        int port = atoi(val);
        if (port > 0 && port < 65536) g_dtec.target_port = (uint16_t)port;
    } else if (strcasecmp(key, "auto_poll") == 0) {
        g_dtec.auto_poll = ini_parse_bool(val);
    } else if (strcasecmp(key, "profile") == 0 && val[0]) {
        for (int i = 0; i < DTEC_PROFILE_COUNT; i++) {
            if (strcasecmp(val, g_dtec_profiles[i].brand) == 0 ||
                strncasecmp(val, g_dtec_profiles[i].name, strlen(val)) == 0) {
                dtec_obd_apply_profile(i);
                g_dtec.profile_confirmed = true;
                break;
            }
        }
    }
}

static void refresh_local_info(void) {
    if (s_ac_ok) {
        char active_ssid[33];
        memset(active_ssid, 0, sizeof(active_ssid));
        if (R_SUCCEEDED(ACU_GetSSID(active_ssid)) && active_ssid[0] != '\0') {
            snprintf(g_dtec.wifi_ssid, sizeof(g_dtec.wifi_ssid), "%s", active_ssid);
        }
    }

    struct in_addr host_ip;
    host_ip.s_addr = (u32)gethostid();
    if (host_ip.s_addr != 0) {
        snprintf(g_dtec.local_ip, sizeof(g_dtec.local_ip), "%s", inet_ntoa(host_ip));
    } else {
        snprintf(g_dtec.local_ip, sizeof(g_dtec.local_ip), "127.0.0.1");
    }
}

static void start_tcp_connection(void) {
    close_socket();
    refresh_local_info();

    char msg[48];
    snprintf(msg, sizeof(msg), "TCP %s:%u...", g_dtec.target_ip, (unsigned)g_dtec.target_port);
    set_status(NET_TCP_CONNECTING, msg);

    s_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (s_sock < 0) {
        snprintf(msg, sizeof(msg), "Socket fail (%d)", errno);
        set_status(NET_ERROR, msg);
        s_reconnect_timer = 0;
        return;
    }

    int flags = fcntl(s_sock, F_GETFL, 0);
    if (flags < 0) flags = 0;
    fcntl(s_sock, F_SETFL, flags | O_NONBLOCK);

    int nodelay = 1;
    setsockopt(s_sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(g_dtec.target_port);
    srv.sin_addr.s_addr = inet_addr(g_dtec.target_ip);

    s_tcp_connect_ticks = 0;
    int res = connect(s_sock, (struct sockaddr *)&srv, sizeof(srv));
    if (res == 0) {
        set_status(NET_OBD_CONNECTED, "OBD Link Active");
        dtec_obd_on_connected();
    } else if (res < 0 && (errno == EINPROGRESS || errno == EWOULDBLOCK)) {
        set_status(NET_TCP_CONNECTING, "Connecting OBD...");
    } else {
        close_socket();
        snprintf(msg, sizeof(msg), "Connect fail (%d)", errno);
        set_status(NET_ERROR, msg);
        s_reconnect_timer = 0;
    }
}

static bool try_emulator_loopback(void) {
    if (!s_tried_emu_loopback && gethostid() == 0 && strcmp(g_dtec.target_ip, "192.168.0.10") == 0) {
        s_tried_emu_loopback = true;
        snprintf(g_dtec.target_ip, sizeof(g_dtec.target_ip), "127.0.0.1");
        dtec_log("Trying 127.0.0.1 (Emu)");
        start_tcp_connection();
        return true;
    }
    return false;
}

void dtec_net_init(void) {
    close_socket();
    s_reconnect_timer = 0;
    s_tcp_connect_ticks = 0;
    s_tried_emu_loopback = false;

    snprintf(g_dtec.wifi_ssid, sizeof(g_dtec.wifi_ssid), "%s", DEFAULT_OBD_SSID);
    snprintf(s_cfg_ip, sizeof(s_cfg_ip), "%s", DEFAULT_OBD_IP);
    snprintf(g_dtec.target_ip, sizeof(g_dtec.target_ip), "%s", DEFAULT_OBD_IP);
    g_dtec.target_port = DEFAULT_OBD_PORT;

    ini_parse_file("sdmc:/dsobd.ini", apply_ini_kv, NULL);

    /* ac session stays open for the app's lifetime: closing it would drop a slot we joined. */
    if (!s_ac_ok) s_ac_ok = R_SUCCEEDED(acInit());

    set_status(NET_WIFI_CONNECTING, "Init 3DS SOC...");

    if (!s_soc_buffer) {
        s_soc_buffer = (u32 *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
        if (!s_soc_buffer) {
            set_status(NET_ERROR, "SOC alloc failed");
            return;
        }
        if (R_FAILED(socInit(s_soc_buffer, SOC_BUFFERSIZE))) {
            free(s_soc_buffer);
            s_soc_buffer = NULL;
            set_status(NET_ERROR, "socInit failed");
            return;
        }
    }

    refresh_local_info();
    set_status(NET_WIFI_CONNECTED, "3DS WiFi Ready");
    start_tcp_connection();
}

void dtec_net_cleanup(void) {
    close_socket();
    if (s_join_evt) {
        svcCloseHandle(s_join_evt);
        s_join_evt = 0;
    }
    if (s_ac_ok) {
        acExit();
        s_ac_ok = false;
    }
    if (s_soc_buffer) {
        socExit();
        free(s_soc_buffer);
        s_soc_buffer = NULL;
    }
}

void dtec_net_reconnect(void) {
    close_socket();
    s_tried_emu_loopback = false;
    snprintf(g_dtec.target_ip, sizeof(g_dtec.target_ip), "%s", s_cfg_ip);
    if (!s_soc_buffer) {
        dtec_net_init();
        return;
    }
    start_tcp_connection();
}

void dtec_net_send_cmd(const char *cmd_str) {
    if (g_dtec.net_status != NET_OBD_CONNECTED || s_sock < 0 || !cmd_str) return;

    size_t len = strlen(cmd_str);
    char buf[64];
    if (len + 2 >= sizeof(buf)) return;

    memcpy(buf, cmd_str, len);
    buf[len] = '\r';
    buf[len + 1] = '\0';

    ssize_t sent = send(s_sock, buf, len + 1, 0);
    if (sent > 0) {
        g_dtec.bytes_tx += (uint32_t)sent;
        g_dtec.cmds_sent++;
        snprintf(g_dtec.last_cmd, sizeof(g_dtec.last_cmd), "> %s", cmd_str);
    } else if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        set_status(NET_ERROR, "OBD send error");
        close_socket();
        s_reconnect_timer = 0;
    }
}

bool dtec_net_is_connected(void) {
    return g_dtec.net_status == NET_OBD_CONNECTED;
}

void dtec_3ds_net_scan_slots(void) {
    int found = 0;
    for (int i = 0; i < DTEC_3DS_WIFI_SLOTS; i++) {
        /* 0x20-byte SSID followed by its length byte; padded for safety. */
        u8 raw[0x40];
        memset(raw, 0, sizeof(raw));
        g_3ds_slot_ssid[i][0] = '\0';
        if (!s_ac_ok || R_FAILED(ACI_LoadNetworkSetting((u32)i)) ||
            R_FAILED(ACI_GetNetworkWirelessEssidSecuritySsid(raw))) {
            continue;
        }
        size_t len = raw[0x20];
        if (len == 0 || len > 32) len = strnlen((const char *)raw, 32);
        memcpy(g_3ds_slot_ssid[i], raw, len);
        g_3ds_slot_ssid[i][len] = '\0';
        if (len) found++;
    }
    dtec_log(found ? "WiFi slots scanned" : "No saved WiFi slots read");
}

bool dtec_3ds_net_connect_slot(int slot) {
    if (slot < 0 || slot >= DTEC_3DS_WIFI_SLOTS || !g_3ds_slot_ssid[slot][0] || !s_ac_ok) return false;
    if (!s_join_evt && R_FAILED(svcCreateEvent(&s_join_evt, RESET_ONESHOT))) {
        set_status(NET_ERROR, "Join event failed");
        return false;
    }

    close_socket();
    acuConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    /* AllowApType bit N = system connection slot N+1, so only the chosen slot is tried. */
    if (R_FAILED(ACU_CreateDefaultConfig(&cfg)) ||
        R_FAILED(ACU_SetAllowApType(&cfg, (u8)(1u << slot))) ||
        R_FAILED(ACU_SetRequestEulaVersion(&cfg)) ||
        R_FAILED(ACU_ConnectAsync(&cfg, s_join_evt))) {
        set_status(NET_ERROR, "WiFi join request failed");
        return false;
    }

    s_join_slot = slot;
    s_join_ticks = 0;
    snprintf(g_dtec.wifi_ssid, sizeof(g_dtec.wifi_ssid), "%s", g_3ds_slot_ssid[slot]);
    char msg[48];
    snprintf(msg, sizeof(msg), "Joining %.32s...", g_3ds_slot_ssid[slot]);
    set_status(NET_WIFI_CONNECTING, msg);
    return true;
}

static void poll_slot_join(void) {
    /* dtec_net_update runs twice per frame, so 1800 ticks is about 15 s. */
    bool signalled = svcWaitSynchronization(s_join_evt, 0) == 0;
    if (!signalled && ++s_join_ticks < 1800) return;

    int slot = s_join_slot;
    s_join_slot = -1;
    char ssid[33];
    memset(ssid, 0, sizeof(ssid));
    if (R_SUCCEEDED(ACU_GetSSID(ssid)) && strcmp(ssid, g_3ds_slot_ssid[slot]) == 0) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Joined %.32s", ssid);
        set_status(NET_WIFI_CONNECTED, msg);
        dtec_net_reconnect();
        return;
    }
    u32 err = 0;
    ACU_GetLastErrorCode(&err);
    char msg[48];
    snprintf(msg, sizeof(msg), signalled ? "Join failed (%08lX)" : "Join timeout", (unsigned long)err);
    set_status(NET_ERROR, msg);
    s_reconnect_timer = 0;
}

void dtec_net_update(void) {
    if (!s_soc_buffer) return;
    if (s_join_slot >= 0) {
        poll_slot_join();
        return;
    }

    switch (g_dtec.net_status) {
        case NET_WIFI_CONNECTING:
        case NET_WIFI_CONNECTED: {
            if (s_sock < 0) start_tcp_connection();
            break;
        }

        case NET_TCP_CONNECTING: {
            if (s_sock < 0) {
                set_status(NET_DISCONNECTED, "Socket closed");
                break;
            }
            if (++s_tcp_connect_ticks >= 300) {
                close_socket();
                if (!try_emulator_loopback()) {
                    set_status(NET_ERROR, "Connect timeout");
                    s_reconnect_timer = 0;
                }
                break;
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
                    set_status(NET_OBD_CONNECTED, "OBD Link Active");
                    dtec_obd_on_connected();
                } else {
                    close_socket();
                    if (!try_emulator_loopback()) {
                        set_status(NET_ERROR, "OBD link refused");
                        s_reconnect_timer = 0;
                    }
                }
            }
            break;
        }

        case NET_OBD_CONNECTED: {
            if (s_sock < 0) {
                set_status(NET_DISCONNECTED, "Socket lost");
                break;
            }
            uint8_t buf[256];
            int loops = 0;
            while (loops++ < 8) {
                ssize_t n = recv(s_sock, buf, sizeof(buf), 0);
                if (n > 0) {
                    g_dtec.bytes_rx += (uint32_t)n;
                    dtec_obd_feed_rx(buf, (size_t)n);
                } else if (n == 0) {
                    set_status(NET_DISCONNECTED, "Adapter closed link");
                    close_socket();
                    s_reconnect_timer = 0;
                    break;
                } else {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        set_status(NET_ERROR, "Socket recv error");
                        close_socket();
                        s_reconnect_timer = 0;
                    }
                    break;
                }
            }
            break;
        }

        case NET_DISCONNECTED:
        case NET_ERROR: {
            if (++s_reconnect_timer >= 180) {
                s_reconnect_timer = 0;
                snprintf(g_dtec.target_ip, sizeof(g_dtec.target_ip), "%s", s_cfg_ip);
                s_tried_emu_loopback = false;
                start_tcp_connection();
            }
            break;
        }
    }
}
