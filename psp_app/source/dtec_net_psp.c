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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include "dtec_common.h"
#include "dtec_net.h"
#include "dtec_obd.h"
#include "ini.h"
#include "platform_psp.h"

#define WIFI_ASSOC_TIMEOUT_MS 15000u
#define PPSSPP_PROBE_MS        1200u
#define TCP_TIMEOUT_REAL_MS    4500u
#define TCP_TIMEOUT_EMU_MS      700u
#define RECONNECT_WAIT_MS      2500u

static int s_sock = -1;
static bool s_net_hw_inited = false;
static uint32_t s_state_start_ms = 0;
static uint32_t s_tcp_timeout_ms = TCP_TIMEOUT_REAL_MS;
static bool s_tried_emu_loopback = false;
static bool s_apctl_got_ip = false;
static char s_cfg_ip[20] = DEFAULT_OBD_IP;

static void set_status(DTEC_NetStatus st, const char *msg) {
    g_dtec.net_status = st;
    s_state_start_ms = psp_elapsed_ms();
    if (msg) {
        snprintf(g_dtec.status_msg, sizeof(g_dtec.status_msg), "%s", msg);
        dtec_log(msg);
    }
}

static void close_socket(void) {
    if (s_sock >= 0) {
        sceNetInetClose(s_sock);
        s_sock = -1;
    }
}

static bool is_on_vp01_subnet(void) {
    return strncmp(g_psp.local_ip, "192.168.0.", 10) == 0;
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
    } else if (strcasecmp(key, "apctl_profile") == 0) {
        int prof = atoi(val);
        if (prof >= 0 && prof <= 10) g_psp.apctl_profile_index = prof;
    } else if (strcasecmp(key, "profile") == 0 && val[0]) {
        char *end = NULL;
        long prof = strtol(val, &end, 10);
        if (end && *end == '\0' && prof >= 0 && prof <= 10) {
            g_psp.apctl_profile_index = (int)prof;
        } else {
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
}

void psp_net_save_ini(void) {
    FILE *f = fopen("ms0:/PSP/GAME/DSOBD/dsobd.ini", "w");
    if (!f) f = fopen("dsobd.ini", "w");
    if (!f) f = fopen("ms0:/dsobd.ini", "w");
    if (!f) return;

    fprintf(f, "# PSPOBD Configuration\n");
    fprintf(f, "ssid=%s\n", g_dtec.wifi_ssid);
    fprintf(f, "ip=%s\n", s_cfg_ip);
    fprintf(f, "port=%u\n", (unsigned)g_dtec.target_port);
    fprintf(f, "apctl_profile=%d\n", g_psp.apctl_profile_index);
    if (g_dtec.profile_confirmed && g_dtec.selected_profile >= 0 &&
        g_dtec.selected_profile < DTEC_PROFILE_COUNT) {
        fprintf(f, "profile=%s\n", g_dtec_profiles[g_dtec.selected_profile].brand);
    }
    fprintf(f, "auto_poll=%d\n", g_dtec.auto_poll ? 1 : 0);
    fclose(f);
}

static void add_wifi_item(const char *ssid, const char *profile_name, const char *ip,
                          uint16_t port, int netparam_id, uint8_t sec, int16_t signal, bool is_preset) {
    if (!ssid || !ssid[0] || g_psp.wifi_count >= MAX_SCAN_APS) return;
    for (int i = 0; i < g_psp.wifi_count; i++) {
        if (strcmp(g_psp.wifi_items[i].ssid, ssid) == 0 &&
            strcmp(g_psp.wifi_items[i].target_ip, ip) == 0 &&
            g_psp.wifi_items[i].netparam_id == netparam_id) {
            return;
        }
    }
    PspWifiItem *it = &g_psp.wifi_items[g_psp.wifi_count++];
    memset(it, 0, sizeof(*it));
    snprintf(it->ssid, sizeof(it->ssid), "%s", ssid);
    snprintf(it->profile_name, sizeof(it->profile_name), "%s",
             (profile_name && profile_name[0]) ? profile_name : ssid);
    snprintf(it->target_ip, sizeof(it->target_ip), "%s", ip);
    it->target_port = port;
    it->netparam_id = netparam_id;
    it->security_type = sec;
    it->signal = signal;
    it->is_preset = is_preset;
}

void psp_net_refresh_list(void) {
    g_psp.wlan_switch_on = (sceWlanGetSwitchState() != 0);
    g_psp.wifi_count = 0;
    g_psp.wifi_selected = 0;

    for (int id = 1; id <= 10; id++) {
        if (sceUtilityCheckNetParam(id) == 0) {
            netData name_data, ssid_data, sec_data, route_data;
            memset(&name_data, 0, sizeof(name_data));
            memset(&ssid_data, 0, sizeof(ssid_data));
            memset(&sec_data, 0, sizeof(sec_data));
            memset(&route_data, 0, sizeof(route_data));

            sceUtilityGetNetParam(id, PSP_NETPARAM_NAME, &name_data);
            sceUtilityGetNetParam(id, PSP_NETPARAM_SSID, &ssid_data);
            sceUtilityGetNetParam(id, PSP_NETPARAM_SECURE, &sec_data);
            sceUtilityGetNetParam(id, PSP_NETPARAM_ROUTE, &route_data);

            const char *gw = (route_data.asString[0] && strcmp(route_data.asString, "0.0.0.0") != 0)
                             ? route_data.asString : s_cfg_ip;
            if (ssid_data.asString[0]) {
                add_wifi_item(ssid_data.asString, name_data.asString, gw, g_dtec.target_port,
                              id, (uint8_t)sec_data.asUint, 95, false);
            }
        }
    }

    int ap_state = 0;
    if (s_net_hw_inited && sceNetApctlGetState(&ap_state) == 0 && ap_state == PSP_NET_APCTL_STATE_GOT_IP) {
        SceNetApctlInfo ssid_info, str_info;
        memset(&ssid_info, 0, sizeof(ssid_info));
        memset(&str_info, 0, sizeof(str_info));
        if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_SSID, &ssid_info) == 0 && ssid_info.ssid[0]) {
            sceNetApctlGetInfo(PSP_NET_APCTL_INFO_STRENGTH, &str_info);
            add_wifi_item((const char *)ssid_info.ssid, "Active WLAN", s_cfg_ip, g_dtec.target_port,
                          g_psp.apctl_profile_index, 0,
                          str_info.strength ? (int16_t)str_info.strength : 100, false);
        }
    }

    if (g_dtec.wifi_ssid[0]) {
        add_wifi_item(g_dtec.wifi_ssid, "Saved INI", s_cfg_ip, g_dtec.target_port,
                      g_psp.apctl_profile_index, 0, 90, true);
    }
    add_wifi_item("WiFi_OBDII",      "Veepeak VP01",   "192.168.0.10", 35000, 1, 0, 95, true);
    add_wifi_item("OBDII",           "ELM327 Generic", "192.168.0.10", 35000, 1, 0, 80, true);
    add_wifi_item("V-LINK",          "vGate V-Link",   "192.168.0.10", 35000, 1, 0, 75, true);
    add_wifi_item("CLKDevices",      "CLK OBDWiFi",    "192.168.0.10", 35000, 1, 0, 70, true);
    add_wifi_item("PPSSPP Loopback", "Emulator Mock",  "127.0.0.1",    35000, 0, 0, 100, true);
}

static bool start_tcp_connection(void) {
    close_socket();

    s_sock = sceNetInetSocket(AF_INET, SOCK_STREAM, 0);
    if (s_sock < 0) {
        set_status(NET_ERROR, "Socket create failed");
        return false;
    }

    int nonblock = 1;
    sceNetInetSetsockopt(s_sock, SOL_SOCKET, SO_NONBLOCK, &nonblock, sizeof(nonblock));

    int nodelay = 1;
    sceNetInetSetsockopt(s_sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(g_dtec.target_port);
    srv.sin_addr.s_addr = inet_addr(g_dtec.target_ip);

    s_tcp_timeout_ms = (!is_on_vp01_subnet() && !s_tried_emu_loopback &&
                        strcmp(g_dtec.target_ip, "192.168.0.10") == 0)
                       ? TCP_TIMEOUT_EMU_MS : TCP_TIMEOUT_REAL_MS;

    char msg[48];
    snprintf(msg, sizeof(msg), "TCP %s:%u...", g_dtec.target_ip, (unsigned)g_dtec.target_port);
    set_status(NET_TCP_CONNECTING, msg);

    int res = sceNetInetConnect(s_sock, (struct sockaddr *)&srv, sizeof(srv));
    int err = sceNetInetGetErrno();
    if (res == 0) {
        set_status(NET_OBD_CONNECTED, "OBD Link Active");
        dtec_obd_on_connected();
        return true;
    } else if (err == EINPROGRESS || err == EALREADY || err == EWOULDBLOCK) {
        return true;
    }

    close_socket();
    set_status(NET_ERROR, "Socket connect failed");
    return false;
}

static bool try_emulator_loopback(void) {
    if (!s_tried_emu_loopback && !is_on_vp01_subnet() &&
        strcmp(g_dtec.target_ip, "192.168.0.10") == 0) {
        s_tried_emu_loopback = true;
        snprintf(g_dtec.target_ip, sizeof(g_dtec.target_ip), "127.0.0.1");
        dtec_log("Trying 127.0.0.1 (PPSSPP)");
        return start_tcp_connection();
    }
    return false;
}

bool psp_net_connect_item(int index) {
    if (index < 0 || index >= g_psp.wifi_count) return false;
    PspWifiItem *it = &g_psp.wifi_items[index];

    snprintf(g_dtec.wifi_ssid, sizeof(g_dtec.wifi_ssid), "%s", it->ssid);
    snprintf(s_cfg_ip, sizeof(s_cfg_ip), "%s", it->target_ip);
    snprintf(g_dtec.target_ip, sizeof(g_dtec.target_ip), "%s", it->target_ip);
    g_dtec.target_port = it->target_port;
    g_psp.apctl_profile_index = it->netparam_id;
    psp_net_save_ini();

    s_tried_emu_loopback = false;
    s_apctl_got_ip = false;
    close_socket();

    if (strcmp(g_dtec.target_ip, "127.0.0.1") == 0) {
        snprintf(g_psp.local_ip, sizeof(g_psp.local_ip), "127.0.0.1");
        set_status(NET_WIFI_CONNECTED, "Loopback Target");
        return start_tcp_connection();
    }

    if (s_net_hw_inited) {
        sceNetApctlDisconnect();
        int prof = it->netparam_id > 0 ? it->netparam_id : 0;
        if (sceNetApctlConnect(prof) < 0) {
            sceNetApctlConnect(0);
        }
    }
    set_status(NET_WIFI_CONNECTING, "Joining WLAN...");
    return true;
}

void psp_net_set_target(const char *ip, uint16_t port) {
    if (ip && ip[0]) {
        snprintf(s_cfg_ip, sizeof(s_cfg_ip), "%s", ip);
        snprintf(g_dtec.target_ip, sizeof(g_dtec.target_ip), "%s", ip);
    }
    if (port > 0) g_dtec.target_port = port;
    psp_net_save_ini();
    dtec_net_reconnect();
}

void dtec_net_init(void) {
    close_socket();
    s_tried_emu_loopback = false;
    s_apctl_got_ip = false;

    snprintf(g_dtec.wifi_ssid, sizeof(g_dtec.wifi_ssid), "%s", DEFAULT_OBD_SSID);
    snprintf(s_cfg_ip, sizeof(s_cfg_ip), "%s", DEFAULT_OBD_IP);
    snprintf(g_dtec.target_ip, sizeof(g_dtec.target_ip), "%s", DEFAULT_OBD_IP);
    g_dtec.target_port = DEFAULT_OBD_PORT;
    g_psp.apctl_profile_index = 1;

    const char *ini_paths[] = {"ms0:/PSP/GAME/DSOBD/dsobd.ini", "dsobd.ini", "ms0:/dsobd.ini"};
    for (size_t i = 0; i < sizeof(ini_paths) / sizeof(ini_paths[0]); i++) {
        if (ini_parse_file(ini_paths[i], apply_ini_kv, NULL)) break;
    }

    g_psp.wlan_switch_on = (sceWlanGetSwitchState() != 0);

    if (!s_net_hw_inited) {
        sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
        sceUtilityLoadNetModule(PSP_NET_MODULE_INET);

        if (sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024) < 0) {
            set_status(NET_ERROR, "sceNetInit failed");
            return;
        }
        if (sceNetInetInit() < 0) {
            set_status(NET_ERROR, "sceNetInetInit failed");
            return;
        }
        if (sceNetApctlInit(0x8000, 48) < 0) {
            set_status(NET_ERROR, "sceNetApctlInit failed");
            return;
        }
        s_net_hw_inited = true;
    }

    psp_net_refresh_list();

    int state = 0;
    if (sceNetApctlGetState(&state) == 0 && state == PSP_NET_APCTL_STATE_GOT_IP) {
        s_apctl_got_ip = true;
        set_status(NET_WIFI_CONNECTED, "WLAN Ready");
        start_tcp_connection();
        return;
    }

    if (strcmp(g_dtec.target_ip, "127.0.0.1") == 0) {
        snprintf(g_psp.local_ip, sizeof(g_psp.local_ip), "127.0.0.1");
        set_status(NET_WIFI_CONNECTED, "Loopback Ready");
        start_tcp_connection();
        return;
    }

    if (sceNetApctlConnect(g_psp.apctl_profile_index) < 0) {
        sceNetApctlConnect(0);
    }
    set_status(NET_WIFI_CONNECTING, g_psp.wlan_switch_on ? "Joining WLAN..." : "WLAN Switch OFF");
}

void dtec_net_cleanup(void) {
    close_socket();
    if (s_net_hw_inited) {
        sceNetApctlTerm();
        sceNetInetTerm();
        sceNetTerm();
        s_net_hw_inited = false;
    }
}

void dtec_net_reconnect(void) {
    close_socket();
    s_tried_emu_loopback = false;
    snprintf(g_dtec.target_ip, sizeof(g_dtec.target_ip), "%s", s_cfg_ip);
    if (!s_net_hw_inited) {
        dtec_net_init();
        return;
    }
    if (s_apctl_got_ip || g_dtec.net_status >= NET_WIFI_CONNECTED ||
        strcmp(g_dtec.target_ip, "127.0.0.1") == 0) {
        start_tcp_connection();
    } else {
        if (sceNetApctlConnect(g_psp.apctl_profile_index) < 0) {
            sceNetApctlConnect(0);
        }
        set_status(NET_WIFI_CONNECTING, "Joining WLAN...");
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

    int sent = (int)sceNetInetSend(s_sock, buf, len + 1, MSG_DONTWAIT);
    if (sent > 0) {
        g_dtec.bytes_tx += (uint32_t)sent;
        g_dtec.cmds_sent++;
        snprintf(g_dtec.last_cmd, sizeof(g_dtec.last_cmd), "> %s", cmd_str);
    } else if (sent < 0) {
        int err = sceNetInetGetErrno();
        if (err != EAGAIN && err != EWOULDBLOCK) {
            set_status(NET_ERROR, "OBD send error");
            close_socket();
        }
    }
}

bool dtec_net_is_connected(void) {
    return g_dtec.net_status == NET_OBD_CONNECTED;
}

static void read_apctl_details(void) {
    SceNetApctlInfo ip_info, gw_info, ssid_info;
    memset(&ip_info, 0, sizeof(ip_info));
    memset(&gw_info, 0, sizeof(gw_info));
    memset(&ssid_info, 0, sizeof(ssid_info));
    if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &ip_info) == 0) {
        snprintf(g_psp.local_ip, sizeof(g_psp.local_ip), "%s", ip_info.ip);
    }
    if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_GATEWAY, &gw_info) == 0) {
        snprintf(g_psp.gateway_ip, sizeof(g_psp.gateway_ip), "%s", gw_info.gateway);
    }
    if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_SSID, &ssid_info) == 0 && ssid_info.ssid[0]) {
        snprintf(g_dtec.wifi_ssid, sizeof(g_dtec.wifi_ssid), "%s", (const char *)ssid_info.ssid);
    }
}

void dtec_net_update(void) {
    g_psp.wlan_switch_on = (sceWlanGetSwitchState() != 0);
    if (!s_net_hw_inited) return;

    uint32_t now_ms = psp_elapsed_ms();
    uint32_t elapsed = now_ms - s_state_start_ms;

    int ap_state = 0;
    if (sceNetApctlGetState(&ap_state) == 0) {
        g_psp.apctl_state = ap_state;
        if (ap_state == PSP_NET_APCTL_STATE_GOT_IP) {
            s_apctl_got_ip = true;
            SceNetApctlInfo str_info;
            memset(&str_info, 0, sizeof(str_info));
            if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_STRENGTH, &str_info) == 0) {
                g_psp.signal_strength = (int)str_info.strength;
            }
        }
    }

    switch (g_dtec.net_status) {
        case NET_WIFI_CONNECTING: {
            if (ap_state == PSP_NET_APCTL_STATE_GOT_IP) {
                read_apctl_details();
                set_status(NET_WIFI_CONNECTED, "WLAN Connected");
                start_tcp_connection();
                break;
            }

            if (ap_state == PSP_NET_APCTL_STATE_SCANNING ||
                ap_state == PSP_NET_APCTL_STATE_JOINING ||
                ap_state == PSP_NET_APCTL_STATE_GETTING_IP) {
                if (elapsed >= WIFI_ASSOC_TIMEOUT_MS) {
                    set_status(NET_ERROR, "WLAN assoc timeout");
                }
            } else if (elapsed >= PPSSPP_PROBE_MS) {
                if (!g_psp.wlan_switch_on && strcmp(g_dtec.target_ip, "127.0.0.1") != 0) {
                    set_status(NET_ERROR, "WLAN Switch OFF");
                } else {
                    snprintf(g_psp.local_ip, sizeof(g_psp.local_ip), "127.0.0.1");
                    set_status(NET_WIFI_CONNECTED, "WLAN / Emu Ready");
                    start_tcp_connection();
                }
            }
            break;
        }

        case NET_WIFI_CONNECTED: {
            if (s_sock < 0) start_tcp_connection();
            break;
        }

        case NET_TCP_CONNECTING: {
            if (s_sock < 0) {
                set_status(NET_DISCONNECTED, "Socket closed");
                break;
            }
            if (elapsed >= s_tcp_timeout_ms) {
                close_socket();
                if (!try_emulator_loopback()) {
                    set_status(NET_ERROR, "TCP timeout");
                }
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
                    set_status(NET_OBD_CONNECTED, "OBD Link Active");
                    dtec_obd_on_connected();
                } else {
                    close_socket();
                    if (!try_emulator_loopback()) {
                        set_status(NET_ERROR, "OBD link refused");
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
            uint8_t rx_buf[512];
            int loops = 0;
            while (loops++ < 8) {
                int n = (int)sceNetInetRecv(s_sock, rx_buf, sizeof(rx_buf), MSG_DONTWAIT);
                if (n > 0) {
                    g_dtec.bytes_rx += (uint32_t)n;
                    g_psp.rx_packets++;
                    dtec_obd_feed_rx(rx_buf, (size_t)n);
                } else if (n == 0) {
                    close_socket();
                    set_status(NET_DISCONNECTED, "Adapter closed link");
                    break;
                } else {
                    int err = sceNetInetGetErrno();
                    if (err != EAGAIN && err != EWOULDBLOCK && err != EINPROGRESS) {
                        close_socket();
                        set_status(NET_ERROR, "Socket read error");
                    }
                    break;
                }
            }
            break;
        }

        case NET_DISCONNECTED:
        case NET_ERROR: {
            if (elapsed >= RECONNECT_WAIT_MS) {
                s_tried_emu_loopback = false;
                snprintf(g_dtec.target_ip, sizeof(g_dtec.target_ip), "%s", s_cfg_ip);
                if (s_apctl_got_ip || strcmp(g_dtec.target_ip, "127.0.0.1") == 0) {
                    if (s_apctl_got_ip) read_apctl_details();
                    start_tcp_connection();
                } else {
                    set_status(NET_WIFI_CONNECTING, "Retrying WLAN...");
                }
            }
            break;
        }
    }
}
