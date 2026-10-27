#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <fat.h>
#include "config.h"
#include "ini.h"

AppConfig g_config;

void config_init_defaults(void) {
    memset(&g_config, 0, sizeof(g_config));
    strncpy(g_config.wifi_ssid, DEFAULT_OBD_SSID, sizeof(g_config.wifi_ssid) - 1);
    strncpy(g_config.wifi_key, DEFAULT_OBD_KEY, sizeof(g_config.wifi_key) - 1);
    strncpy(g_config.obd_ip, DEFAULT_OBD_IP, sizeof(g_config.obd_ip) - 1);
    g_config.obd_port = DEFAULT_OBD_PORT;
    g_config.auto_poll = true;
    g_config.poll_interval_ms = 400;
}

static void apply_kv(const char *key, const char *val, void *user) {
    (void)user;
    if (strcasecmp(key, "ssid") == 0) {
        strncpy(g_config.wifi_ssid, val, sizeof(g_config.wifi_ssid) - 1);
    } else if (strcasecmp(key, "key") == 0 || strcasecmp(key, "wep_key") == 0 || strcasecmp(key, "password") == 0) {
        strncpy(g_config.wifi_key, val, sizeof(g_config.wifi_key) - 1);
    } else if (strcasecmp(key, "ip") == 0 || strcasecmp(key, "server_ip") == 0) {
        strncpy(g_config.obd_ip, val, sizeof(g_config.obd_ip) - 1);
    } else if (strcasecmp(key, "port") == 0) {
        int port = atoi(val);
        if (port > 0 && port < 65536) {
            g_config.obd_port = (uint16_t)port;
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

bool config_load(void) {
    config_init_defaults();

    if (!fatInitDefault()) {
        return false;
    }

    const char *paths[] = {
        "/dsobd.ini",
        "dsobd.ini",
        "/obd.ini",
        "obd.ini"
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        if (ini_parse_file(paths[i], apply_kv, NULL)) {
            return true;
        }
    }

    // Attempt to write template config file if none existed
    config_save();

    return true;
}

bool config_save(void) {
    FILE *f = fopen("/dsobd.ini", "w");
    if (!f) f = fopen("dsobd.ini", "w");
    if (!f) return false;

    fprintf(f, "# DSOBD Configuration for Veepeak OBDCheck VP01 WiFi\n");
    fprintf(f, "ssid=%s\n", g_config.wifi_ssid);
    fprintf(f, "key=%s\n", g_config.wifi_key);
    fprintf(f, "ip=%s\n", g_config.obd_ip);
    fprintf(f, "port=%u\n", (unsigned)g_config.obd_port);
    fprintf(f, "auto_poll=%d\n", g_config.auto_poll ? 1 : 0);
    fprintf(f, "poll_ms=%lu\n", (unsigned long)g_config.poll_interval_ms);
    fclose(f);
    return true;
}

bool config_set_ssid(const char *ssid) {
    if (!ssid || ssid[0] == '\0') return false;
    snprintf(g_config.wifi_ssid, sizeof(g_config.wifi_ssid), "%s", ssid);
    return config_save();
}
