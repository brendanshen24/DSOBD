#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include "config.h"
#include "ini.h"

AppConfig g_config;

void config_init_defaults(void) {
    memset(&g_config, 0, sizeof(g_config));
    strncpy(g_config.wifi_ssid, DEFAULT_OBD_SSID, sizeof(g_config.wifi_ssid) - 1);
    strncpy(g_config.wifi_key, DEFAULT_OBD_KEY, sizeof(g_config.wifi_key) - 1);
    strncpy(g_config.obd_ip, DEFAULT_OBD_IP, sizeof(g_config.obd_ip) - 1);
    g_config.obd_port = DEFAULT_OBD_PORT;
    g_config.apctl_profile_index = 1;
    g_config.auto_poll = true;
    g_config.poll_interval_ms = 400;
}

static void apply_kv(const char *key, const char *val, void *user) {
    (void)user;
    if (strcasecmp(key, "ssid") == 0) {
        snprintf(g_config.wifi_ssid, sizeof(g_config.wifi_ssid), "%s", val);
    } else if (strcasecmp(key, "key") == 0 || strcasecmp(key, "wep_key") == 0 || strcasecmp(key, "password") == 0) {
        snprintf(g_config.wifi_key, sizeof(g_config.wifi_key), "%s", val);
    } else if (strcasecmp(key, "ip") == 0 || strcasecmp(key, "server_ip") == 0) {
        snprintf(g_config.obd_ip, sizeof(g_config.obd_ip), "%s", val);
    } else if (strcasecmp(key, "port") == 0) {
        int port = atoi(val);
        if (port > 0 && port < 65536) {
            g_config.obd_port = (uint16_t)port;
        }
    } else if (strcasecmp(key, "profile") == 0) {
        int prof = atoi(val);
        if (prof >= 0 && prof <= 10) {
            g_config.apctl_profile_index = prof;
        }
    } else if (strcasecmp(key, "auto_poll") == 0) {
        g_config.auto_poll = ini_parse_bool(val);
    } else if (strcasecmp(key, "poll_ms") == 0) {
        int ms = atoi(val);
        if (ms >= 100 && ms <= 10000) {
            g_config.poll_interval_ms = (uint32_t)ms;
        }
    }
}

bool config_save(void) {
    FILE *f = fopen("dsobd.ini", "w");
    if (!f) f = fopen("ms0:/PSP/GAME/DSOBD/dsobd.ini", "w");
    if (!f) f = fopen("ms0:/dsobd.ini", "w");
    if (!f) return false;

    fprintf(f, "# DSOBD PSP Configuration for Veepeak OBDCheck VP01 WiFi\n");
    fprintf(f, "ssid=%s\n", g_config.wifi_ssid);
    fprintf(f, "key=%s\n", g_config.wifi_key);
    fprintf(f, "ip=%s\n", g_config.obd_ip);
    fprintf(f, "port=%u\n", (unsigned)g_config.obd_port);
    fprintf(f, "profile=%d\n", g_config.apctl_profile_index);
    fprintf(f, "auto_poll=%d\n", g_config.auto_poll ? 1 : 0);
    fprintf(f, "poll_ms=%lu\n", (unsigned long)g_config.poll_interval_ms);
    fclose(f);
    return true;
}

bool config_load(void) {
    config_init_defaults();

    const char *paths[] = {
        "dsobd.ini",
        "ms0:/PSP/GAME/DSOBD/dsobd.ini",
        "ms0:/dsobd.ini"
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        if (ini_parse_file(paths[i], apply_kv, NULL)) {
            return true;
        }
    }

    config_save();
    return true;
}

bool config_set_ssid_and_ip(const char *ssid, const char *ip, uint16_t port, int profile_idx) {
    if (ssid && ssid[0] != '\0') {
        snprintf(g_config.wifi_ssid, sizeof(g_config.wifi_ssid), "%s", ssid);
    }
    if (ip && ip[0] != '\0') {
        snprintf(g_config.obd_ip, sizeof(g_config.obd_ip), "%s", ip);
    }
    if (port > 0) {
        g_config.obd_port = port;
    }
    if (profile_idx >= 0) {
        g_config.apctl_profile_index = profile_idx;
    }
    return config_save();
}
