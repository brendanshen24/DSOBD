// Thin adapter between this app and the shared ELM327 core (core/source/elm_core.c).
// The same file is used by the DS, 3DS and PSP terminals.

#include <stdio.h>
#include <string.h>

#include "obd.h"
#include "common.h"
#include "config.h"
#include "net.h"
#include "ui.h"
#include "elm_core.h"

static ElmCore s_elm;
static uint32_t s_frames = 0;

// Polled in this order, one command per poll interval, each only after the
// previous reply has arrived.
static const char *const s_poll_cmds[] = {
    "010C", // Engine RPM
    "010D", // Vehicle speed
    "0105", // Coolant temperature
    "ATRV"  // Battery voltage
};

// obd_poll_tick() runs once per 60 Hz frame on every platform.
static uint32_t now_ms(void) {
    return (s_frames * 50u) / 3u;
}

static bool send_cb(const char *cmd, void *user) {
    (void)user;
    return net_send_cmd(cmd);
}

static void tx_cb(const char *cmd, void *user) {
    (void)user;
    ui_log_tx(cmd);
}

static void log_cb(const char *msg, void *user) {
    (void)user;
    ui_log_system(msg);
}

static void sync_state(void) {
    const ElmData *d = &s_elm.d;

    g_state.elm_inited = d->elm_ready;
    if (d->has_rpm) g_state.rpm = d->rpm;
    if (d->has_speed) g_state.speed_kmh = d->speed_kmh;
    if (d->has_coolant) g_state.coolant_temp_c = d->coolant_c;
    if (d->has_voltage) {
        snprintf(g_state.battery_voltage, sizeof(g_state.battery_voltage), "%s", d->voltage);
    }
    if (d->elm_version[0]) {
        snprintf(g_state.elm_version, sizeof(g_state.elm_version), "%s", d->elm_version);
    }
    snprintf(g_state.last_raw_line, sizeof(g_state.last_raw_line), "%s", d->last_line);
    g_state.stats.lines_rx = d->lines_rx;
    g_state.stats.cmds_sent = d->cmds_sent;
}

void obd_init(void) {
    ElmCallbacks cb = { send_cb, tx_cb, log_cb, NULL, NULL };
    elm_init(&s_elm, &cb, s_poll_cmds, (int)(sizeof(s_poll_cmds) / sizeof(s_poll_cmds[0])));
    s_elm.poll_interval_ms = g_config.poll_interval_ms;

    g_state.elm_inited = false;
    g_state.auto_poll = g_config.auto_poll;
    g_state.rpm = 0;
    g_state.speed_kmh = 0;
    g_state.coolant_temp_c = -999;
    snprintf(g_state.battery_voltage, sizeof(g_state.battery_voltage), "--.-V");
    g_state.elm_version[0] = '\0';
    g_state.last_raw_line[0] = '\0';
}

void obd_on_connected(void) {
    ui_log_system("Sending ELM327 initialization...");
    elm_on_connected(&s_elm, now_ms());
}

void obd_send_cmd(const char *cmd) {
    if (!cmd || cmd[0] == '\0') return;
    if (g_state.net_status != NET_STATUS_TCP_CONNECTED) {
        ui_log_system("Not connected to adapter");
        return;
    }
    if (!elm_request(&s_elm, cmd)) {
        ui_log_system("Command queue full");
    }
}

void obd_feed_rx_data(const uint8_t *data, size_t len) {
    if (!data || len == 0) return;

    // Show every incoming character on the terminal, then parse it.
    ui_log_rx_bytes(data, len);
    elm_feed(&s_elm, data, len, now_ms());
    sync_state();
}

void obd_poll_tick(void) {
    s_frames++;

    if (g_state.net_status != NET_STATUS_TCP_CONNECTED) {
        if (s_elm.connected) elm_on_disconnected(&s_elm);
        return;
    }

    s_elm.auto_poll = g_state.auto_poll;
    elm_tick(&s_elm, now_ms());
    sync_state();
}
