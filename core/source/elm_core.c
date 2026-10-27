#include "elm_core.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define ELM_GREETING_WAIT_MS 500    // wait for an unsolicited '>' after connect
#define ELM_DEFAULT_TIMEOUT  3000
#define ELM_RESET_TIMEOUT    5000   // ATZ restarts the chip (~1 s)
#define ELM_SEARCH_TIMEOUT   15000  // "SEARCHING..." protocol detection
#define ELM_SEND_RETRY_MS    250

static const char *const s_init_cmds[] = {
    "ATZ",   // Reset ELM327 chip
    "ATE0",  // Echo off
    "ATL0",  // Linefeeds off
    "ATH0",  // Headers off
    "ATSP0", // Automatic protocol detection
    "ATRV",  // Battery voltage
    "0100"   // Supported PIDs 01-20
};
#define ELM_INIT_COUNT ((int)(sizeof(s_init_cmds) / sizeof(s_init_cmds[0])))

static void elm_log(ElmCore *e, const char *msg) {
    if (e->cb.on_log) e->cb.on_log(msg, e->cb.user);
}

void elm_init(ElmCore *e, const ElmCallbacks *cb,
              const char *const *poll_cmds, int poll_count) {
    memset(e, 0, sizeof(*e));
    if (cb) e->cb = *cb;
    e->poll_cmds = poll_cmds;
    e->poll_count = poll_count;
    e->auto_poll = true;
    e->poll_interval_ms = 400;
    e->init_idx = ELM_INIT_COUNT; // nothing to do until connected
}

void elm_on_connected(ElmCore *e, uint32_t now_ms) {
    memset(&e->d, 0, sizeof(e->d));
    e->line_pos = 0;
    e->q_head = 0;
    e->q_count = 0;
    e->poll_idx = 0;
    e->init_idx = 0;
    e->connected = true;
    e->last_poll_ms = now_ms;

    // Some adapters print a '>' as soon as the socket opens. Give it a moment
    // so that prompt is not mistaken for the reply to ATZ.
    e->busy = true;
    e->inflight[0] = '\0';
    e->sent_ms = now_ms;
    e->timeout_ms = ELM_GREETING_WAIT_MS;
}

void elm_on_disconnected(ElmCore *e) {
    e->connected = false;
    e->busy = false;
    e->q_count = 0;
    e->line_pos = 0;
}

static void send_now(ElmCore *e, const char *cmd, uint32_t now_ms) {
    if (!e->cb.send || !e->cb.send(cmd, e->cb.user)) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Send failed: %s", cmd);
        elm_log(e, msg);
        e->d.send_failures++;
        // Back off briefly so a full socket buffer is not hammered every frame.
        e->busy = true;
        e->inflight[0] = '\0';
        e->sent_ms = now_ms;
        e->timeout_ms = ELM_SEND_RETRY_MS;
        return;
    }

    snprintf(e->inflight, sizeof(e->inflight), "%s", cmd);
    e->busy = true;
    e->sent_ms = now_ms;
    e->timeout_ms = (strcmp(cmd, "ATZ") == 0) ? ELM_RESET_TIMEOUT : ELM_DEFAULT_TIMEOUT;
    e->d.cmds_sent++;
    if (e->cb.on_tx) e->cb.on_tx(cmd, e->cb.user);
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Reads n hex bytes (2n characters) from s. Returns false on any non-hex char.
static bool read_hex(const char *s, int n, uint32_t *out) {
    uint32_t v = 0;
    for (int i = 0; i < n * 2; i++) {
        int h = hexval(s[i]);
        if (h < 0) return false;
        v = (v << 4) | (uint32_t)h;
    }
    *out = v;
    return true;
}

bool elm_pid_supported(const ElmCore *e, uint8_t pid) {
    if (pid == 0x00 || pid > 0x20 || !e->d.has_supported) return true;
    return (e->d.supported_01_20 & (1u << (32 - pid))) != 0;
}

static bool poll_cmd_supported(const ElmCore *e, const char *cmd) {
    uint32_t pid;
    if (strlen(cmd) == 4 && cmd[0] == '0' && cmd[1] == '1' && read_hex(cmd + 2, 1, &pid)) {
        return elm_pid_supported(e, (uint8_t)pid);
    }
    return true;
}

static bool looks_like_voltage(const char *line, size_t len) {
    if (len < 4 || len > 8) return false;
    if (line[len - 1] != 'V' && line[len - 1] != 'v') return false;
    if (!isdigit((unsigned char)line[0])) return false;
    return strchr(line, '.') != NULL;
}

static void parse_line(ElmCore *e, const char *line, uint32_t now_ms) {
    ElmData *d = &e->d;

    d->lines_rx++;
    snprintf(d->last_line, sizeof(d->last_line), "%.63s", line);
    if (e->cb.on_line) e->cb.on_line(line, e->cb.user);

    if (strstr(line, "ELM327") || strstr(line, "OBDII to RS232")) {
        snprintf(d->elm_version, sizeof(d->elm_version), "%.31s", line);
        d->elm_ready = true;
        return;
    }
    if (strstr(line, "SEARCHING")) {
        // Protocol detection can take several seconds; extend the deadline.
        e->sent_ms = now_ms;
        e->timeout_ms = ELM_SEARCH_TIMEOUT;
        return;
    }
    if (strcmp(line, "STOPPED") == 0) {
        d->stopped++;
        return;
    }

    size_t len = strlen(line);
    if (looks_like_voltage(line, len)) {
        snprintf(d->voltage, sizeof(d->voltage), "%.15s", line);
        d->has_voltage = true;
        return;
    }

    // Compact hex: "41 0C 1A F8" -> "410C1AF8"
    char c[64];
    size_t j = 0;
    for (size_t i = 0; line[i] != '\0' && j + 1 < sizeof(c); i++) {
        if (!isspace((unsigned char)line[i])) {
            c[j++] = (char)toupper((unsigned char)line[i]);
        }
    }
    c[j] = '\0';

    uint32_t pid, v;
    if (j < 6 || c[0] != '4' || c[1] != '1' || !read_hex(c + 2, 1, &pid)) return;
    const char *data = c + 4;
    size_t dlen = j - 4;

    switch (pid) {
        case 0x00: // Supported PIDs 01-20 (4 bytes). Several ECUs may answer.
            if (dlen >= 8 && read_hex(data, 4, &v)) {
                d->supported_01_20 |= v;
                d->has_supported = true;
            }
            break;
        case 0x0C: // RPM = (A*256 + B) / 4
            if (dlen >= 4 && read_hex(data, 2, &v)) {
                d->rpm = v / 4;
                d->has_rpm = true;
            }
            break;
        case 0x0D: // Speed = A km/h
            if (read_hex(data, 1, &v)) {
                d->speed_kmh = v;
                d->has_speed = true;
            }
            break;
        case 0x05: // Coolant = A - 40 C
            if (read_hex(data, 1, &v)) {
                d->coolant_c = (int)v - 40;
                d->has_coolant = true;
            }
            break;
        case 0x0B: // Intake manifold absolute pressure = A kPa
            if (read_hex(data, 1, &v)) {
                d->map_kpa = (int)v;
                d->has_map = true;
            }
            break;
        case 0x11: // Throttle position
            if (read_hex(data, 1, &v)) {
                d->tps_raw = (uint8_t)v;
                d->has_tps = true;
            }
            break;
        case 0x04: // Calculated engine load
            if (read_hex(data, 1, &v)) {
                d->load_raw = (uint8_t)v;
                d->has_load = true;
            }
            break;
        default:
            break;
    }
}

void elm_feed(ElmCore *e, const uint8_t *data, size_t len, uint32_t now_ms) {
    if (!data) return;

    for (size_t i = 0; i < len; i++) {
        uint8_t ch = data[i];

        if (ch == '\r' || ch == '\n' || ch == '>') {
            if (e->line_pos > 0) {
                e->line[e->line_pos] = '\0';
                e->line_pos = 0;
                parse_line(e, e->line, now_ms);
            }
            if (ch == '>') {
                // Prompt: the adapter has finished and is ready for the next command.
                e->busy = false;
                e->inflight[0] = '\0';
            }
        } else if (ch >= 0x20 && ch < 0x7F) {
            if (e->line_pos + 1 < sizeof(e->line)) {
                e->line[e->line_pos++] = (char)ch;
            }
        }
    }
}

void elm_tick(ElmCore *e, uint32_t now_ms) {
    if (!e->connected) return;

    if (e->busy) {
        if ((uint32_t)(now_ms - e->sent_ms) < e->timeout_ms) return;
        if (e->inflight[0] != '\0') {
            char msg[48];
            snprintf(msg, sizeof(msg), "No reply to %s (timeout)", e->inflight);
            elm_log(e, msg);
            e->d.timeouts++;
        }
        e->busy = false;
        e->inflight[0] = '\0';
    }

    if (e->init_idx < ELM_INIT_COUNT) {
        int idx = e->init_idx++;
        send_now(e, s_init_cmds[idx], now_ms);
        if (e->inflight[0] == '\0') e->init_idx = idx; // send failed, retry it
        return;
    }

    if (e->q_count > 0) {
        char cmd[ELM_CMD_MAX];
        snprintf(cmd, sizeof(cmd), "%s", e->queue[e->q_head]);
        e->q_head = (e->q_head + 1) % ELM_QUEUE_LEN;
        e->q_count--;
        send_now(e, cmd, now_ms);
        return;
    }

    if (e->auto_poll && e->poll_count > 0 &&
        (uint32_t)(now_ms - e->last_poll_ms) >= e->poll_interval_ms) {
        for (int tries = 0; tries < e->poll_count; tries++) {
            const char *cmd = e->poll_cmds[e->poll_idx];
            e->poll_idx = (e->poll_idx + 1) % e->poll_count;
            if (poll_cmd_supported(e, cmd)) {
                e->last_poll_ms = now_ms;
                send_now(e, cmd, now_ms);
                return;
            }
        }
    }
}

bool elm_request(ElmCore *e, const char *cmd) {
    if (!e->connected || !cmd || cmd[0] == '\0' || strlen(cmd) >= ELM_CMD_MAX) return false;

    // A manual reset re-runs the whole init sequence (ATZ turns echo back on).
    if (strcmp(cmd, "ATZ") == 0) {
        e->q_count = 0;
        e->init_idx = 0;
        return true;
    }

    for (int i = 0; i < e->q_count; i++) {
        if (strcmp(e->queue[(e->q_head + i) % ELM_QUEUE_LEN], cmd) == 0) return true;
    }
    if (e->q_count >= ELM_QUEUE_LEN) return false;

    int slot = (e->q_head + e->q_count) % ELM_QUEUE_LEN;
    snprintf(e->queue[slot], ELM_CMD_MAX, "%s", cmd);
    e->q_count++;
    return true;
}
