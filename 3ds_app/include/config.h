#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define DEFAULT_OBD_SSID "WiFi_OBDII"
#define DEFAULT_OBD_KEY  ""
#define DEFAULT_OBD_IP   "192.168.0.10"
#define DEFAULT_OBD_PORT 35000

typedef struct {
    char wifi_ssid[33];
    char wifi_key[65];
    char obd_ip[20];
    uint16_t obd_port;
    bool auto_poll;
    uint32_t poll_interval_ms;
} AppConfig;

extern AppConfig g_config;

void config_init_defaults(void);
bool config_load(void);
bool config_save(void);
bool config_set_ssid_and_ip(const char *ssid, const char *ip, uint16_t port);

#endif // CONFIG_H
