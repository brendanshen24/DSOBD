#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "dtec_common.h"
#include "dtec_obd.h"
#include "dtec_net.h"

static char s_rx_line[256];
static size_t s_rx_line_pos = 0;

const DTEC_CarProfile g_dtec_profiles[DTEC_PROFILE_COUNT] = {
    { "Subaru",     "Turbo WRX/STI (CAN)",  "SSM2 CAN 11b/500k (TurboXS)", "ATSP0", NULL },
    { "Standard",   "OBD-II CAN (2008+)",   "ISO 15765-4 CAN 11b 500k",    "ATSP0", NULL },
    { "Toyota",     "Toyota / Lexus / Scion","ISO 15765-4 CAN 11-bit",      "ATSP0", NULL },
    { "VAG",        "VW / Audi / Skoda",    "CAN 11-bit 500k (UDS/TP2.0)",  "ATSP0", NULL },
    { "Ford",       "Ford / Lincoln / Mazda","CAN 11-bit 500k / MS-CAN",    "ATSP0", NULL },
    { "GM",         "Chevy / GMC / Opel",   "GMLAN CAN 11-bit 500k",        "ATSP0", NULL },
    { "Nissan",     "Nissan / Infiniti",    "CAN 11-bit 500k",              "ATSP0", NULL },
    { "Honda",      "Honda / Acura",        "ISO 15765-4 CAN 11-bit",       "ATSP0", NULL },
    { "Hyundai",    "Hyundai / Kia",        "ISO 15765-4 CAN 11-bit",       "ATSP0", NULL },
    { "BMW",        "BMW / Mini",           "D-CAN 11-bit 500k",            "ATSP0", NULL },
    { "Legacy",     "ISO 9141-2 (1996-2007)","K-Line ISO 9141-2 / KWP",    "ATSP3", NULL },
    { "Ford Leg",   "SAE J1850 PWM",        "Ford EEC-V PWM 41.6kbps",      "ATSP1", NULL },
    { "GM Leg",     "SAE J1850 VPW",        "GM Class 2 VPW 10.4kbps",      "ATSP2", NULL },
    { "Generic",    "Auto-Detect Protocol", "ELM327 Automatic Search",      "ATSP0", NULL }
};

const DTEC_Pid g_dtec_pids[DTEC_PID_COUNT] = {
    { 0x04, 1, 0, "ENGINE LOAD",   "%",      100.0f / 255, 0, 0 },
    { 0x05, 1, 0, "COOLANT",       "C",      1, -40, 0 },
    { 0x06, 1, 1, "SHORT TRIM B1", "%",      100.0f / 128, -100, 0 },
    { 0x07, 1, 1, "LONG TRIM B1",  "%",      100.0f / 128, -100, 0 },
    { 0x08, 1, 1, "SHORT TRIM B2", "%",      100.0f / 128, -100, 0 },
    { 0x09, 1, 1, "LONG TRIM B2",  "%",      100.0f / 128, -100, 0 },
    { 0x0A, 1, 0, "FUEL PRESSURE", "kPa",    3, 0, 0 },
    { 0x0B, 1, 0, "INTAKE MAP",    "kPa",    1, 0, 0 },
    { 0x0C, 2, 0, "ENGINE RPM",    "rpm",    0.25f, 0, 0 },
    { 0x0D, 1, 0, "SPEED",         "km/h",   1, 0, 0 },
    { 0x0E, 1, 1, "TIMING ADV",    "deg",    0.5f, -64, 0 },
    { 0x0F, 1, 0, "INTAKE AIR",    "C",      1, -40, 0 },
    { 0x10, 2, 2, "MAF AIRFLOW",   "g/s",    0.01f, 0, 0 },
    { 0x11, 1, 0, "THROTTLE",      "%",      100.0f / 255, 0, 0 },
    { 0x14, 1, 3, "O2 B1 S1",      "V",      0.005f, 0, 0 },
    { 0x15, 1, 3, "O2 B1 S2",      "V",      0.005f, 0, 0 },
    { 0x1C, 1, 0, "OBD STANDARD",  "",       1, 0, 0 },
    { 0x1F, 2, 0, "RUN TIME",      "s",      1, 0, 0 },
    { 0x21, 2, 0, "DIST MIL ON",   "km",     1, 0, 0 },
    { 0x23, 2, 0, "RAIL PRESSURE", "kPa",    10, 0, 0 },
    { 0x2C, 1, 0, "COMMANDED EGR", "%",      100.0f / 255, 0, 0 },
    { 0x2F, 1, 0, "FUEL LEVEL",    "%",      100.0f / 255, 0, 0 },
    { 0x30, 1, 0, "WARM-UPS CLR",  "",       1, 0, 0 },
    { 0x31, 2, 0, "DIST CLEARED",  "km",     1, 0, 0 },
    { 0x33, 1, 0, "BAROMETRIC",    "kPa",    1, 0, 0 },
    { 0x3C, 2, 0, "CAT TEMP B1",   "C",      0.1f, -40, 0 },
    { 0x42, 2, 2, "ECU VOLTAGE",   "V",      0.001f, 0, 0 },
    { 0x43, 2, 0, "ABSOLUTE LOAD", "%",      100.0f / 255, 0, 0 },
    { 0x44, 2, 2, "COMMANDED AFR", "lambda", 2.0f / 65536, 0, 0 },
    { 0x45, 1, 0, "REL THROTTLE",  "%",      100.0f / 255, 0, 0 },
    { 0x46, 1, 0, "AMBIENT AIR",   "C",      1, -40, 0 },
    { 0x49, 1, 0, "ACCEL PEDAL",   "%",      100.0f / 255, 0, 0 },
    { 0x4E, 2, 0, "TIME CLEARED",  "min",    1, 0, 0 },
    { 0x51, 1, 0, "FUEL TYPE",     "",       1, 0, 0 },
    { 0x5C, 1, 0, "OIL TEMP",      "C",      1, -40, 0 },
    { 0x5E, 2, 1, "FUEL RATE",     "L/h",    0.05f, 0, 0 },
    { 0x61, 1, 0, "DEMAND TORQUE", "%",      1, -125, 0 },
    { 0x62, 1, 0, "ACTUAL TORQUE", "%",      1, -125, 0 },
    { 0x63, 2, 0, "REF TORQUE",    "Nm",     1, 0, 0 },
    { 0x77, 1, 0, "INTERCOOLER",   "C",      1, -40, 1 },
    { 0x78, 2, 0, "EGT B1 S1",     "C",      0.1f, -40, 1 },
    { 0x7C, 2, 0, "DPF INLET B1",  "C",      0.1f, -40, 1 },
    { 0xA6, 4, 0, "ODOMETER",      "km",     0.1f, 0, 0 },
};

static char s_init_cmds[20][16];
static int s_init_count = 0;
static int s_init_index = 0;
static uint32_t s_supported[6];  // 0100 ... 01A0 bitmaps; MSB = first PID of the range
static bool s_has_supported = false;
static uint32_t s_mids_a0;       // 06A0 bitmap of mode 06 monitor IDs A1-C0

// Code scan: one command per prompt, after init and before manual/poll requests.
// 06A2-06AD are misfire counters for cylinders 1-12.
static const char *const k_scan_jobs[] = {
    "ATDP", "0101", "03", "07", "0A", "0902", "0904", "090A", "011C", "0151", "01A6",
    "0121", "0131", "0130", "020200", "020400", "020500", "020C00", "020D00",
    "06A0", "06A2", "06A3", "06A4", "06A5", "06A6", "06A7", "06A8", "06A9", "06AA", "06AB", "06AC", "06AD"
};
static const char *s_jobs[1 + sizeof(k_scan_jobs) / sizeof(k_scan_jobs[0])];
static int s_job_count, s_job_index;
static int s_sensor_slot;

// Multi-line replies (codes, VIN) are collected per ECU message and decoded at the prompt.
static uint8_t s_reply[128];
static int s_reply_len;
static uint8_t s_msg_at[12];
static bool s_msg_framed[12];
static int s_msg_count;

void dtec_obd_apply_profile(int idx) {
    if (idx < 0 || idx >= DTEC_PROFILE_COUNT) idx = 0;
    g_dtec.selected_profile = idx;
    strncpy(g_dtec.car_brand, g_dtec_profiles[idx].brand, sizeof(g_dtec.car_brand) - 1);
    g_dtec.car_brand[sizeof(g_dtec.car_brand) - 1] = '\0';
    strncpy(g_dtec.car_model, g_dtec_profiles[idx].name, sizeof(g_dtec.car_model) - 1);
    g_dtec.car_model[sizeof(g_dtec.car_model) - 1] = '\0';

    s_init_count = 0;
    strncpy(s_init_cmds[s_init_count++], "ATZ", 15);
    strncpy(s_init_cmds[s_init_count++], "ATE0", 15);
    strncpy(s_init_cmds[s_init_count++], "ATL0", 15);
    strncpy(s_init_cmds[s_init_count++], "ATH0", 15);
    strncpy(s_init_cmds[s_init_count++], "ATAT1", 15);
    if (g_dtec_profiles[idx].protocol_cmd) {
        strncpy(s_init_cmds[s_init_count++], g_dtec_profiles[idx].protocol_cmd, 15);
    }
    if (g_dtec_profiles[idx].header_cmd) {
        strncpy(s_init_cmds[s_init_count++], g_dtec_profiles[idx].header_cmd, 15);
    }
    strncpy(s_init_cmds[s_init_count++], "ATRV", 15);
    strncpy(s_init_cmds[s_init_count++], "0100", 15);
    strncpy(s_init_cmds[s_init_count++], "0120", 15);
    strncpy(s_init_cmds[s_init_count++], "0140", 15);
    strncpy(s_init_cmds[s_init_count++], "0160", 15);
    strncpy(s_init_cmds[s_init_count++], "0180", 15);
    strncpy(s_init_cmds[s_init_count++], "01A0", 15);
    strncpy(s_init_cmds[s_init_count++], "0133", 15);  // barometric reference for boost

    s_init_index = 0;
}

// Every send waits for the previous prompt. Timeouts use real milliseconds,
// so a slow render cannot stretch a three-second timeout into ten seconds.
#define MS_GREETING 500
#define MS_DEFAULT  3000
#define MS_RESET    5000
#define MS_SEARCH   15000
#define INIT_MAX_RETRY 2

static uint32_t s_now_ms;
static bool s_busy;
static bool s_busy_is_init;
static uint32_t s_sent_ms;
static uint32_t s_timeout_ms;
static char s_inflight[16];
static char s_pending[16];
static int s_retries;
static uint8_t s_slow_phase;
static uint8_t s_boost_samples;
// ELM327 v1.3+ returns as soon as one ECU answers when a mode 01 request ends
// in a response count ("010C1"), instead of waiting out its listen timeout.
static bool s_single_reply = true;

void dtec_log(const char *msg) {
    if (!msg || msg[0] == '\0') return;

    if (g_dtec.log_count < OBD_LOG_LINES) {
        snprintf(g_dtec.log_lines[g_dtec.log_count++], sizeof(g_dtec.log_lines[0]), "%.39s", msg);
    } else {
        for (int i = 0; i < OBD_LOG_LINES - 1; i++) {
            strncpy(g_dtec.log_lines[i], g_dtec.log_lines[i + 1], sizeof(g_dtec.log_lines[0]));
        }
        snprintf(g_dtec.log_lines[OBD_LOG_LINES - 1], sizeof(g_dtec.log_lines[0]), "%.39s", msg);
    }
}

static void reset_codes(void) {
    g_dtec.codes_read = false;
    g_dtec.clear_armed = false;
    g_dtec.has_monitors = false;
    g_dtec.mil_on = false;
    g_dtec.ecu_dtc_count = 0;
    g_dtec.mon_supported = 0;
    g_dtec.mon_incomplete = 0;
    g_dtec.dtc_count = 0;
    g_dtec.ff_dtc = 0;
    memset(g_dtec.ff_has, 0, sizeof(g_dtec.ff_has));
    s_mids_a0 = 0;
    g_dtec.misfire_cyls = 0;
    memset(g_dtec.misfire_drive, 0, sizeof(g_dtec.misfire_drive));
    memset(g_dtec.misfire_avg, 0, sizeof(g_dtec.misfire_avg));
}

// A new connection may be a different car: forget everything it reported.
static void reset_vehicle(void) {
    memset(s_supported, 0, sizeof(s_supported));
    s_has_supported = false;
    s_job_count = s_job_index = 0;
    g_dtec.scanning = false;
    memset(g_dtec.pid_has, 0, sizeof(g_dtec.pid_has));
    memset(g_dtec.pid_fresh, 0, sizeof(g_dtec.pid_fresh));
    g_dtec.vin[0] = '\0';
    g_dtec.protocol[0] = '\0';
    g_dtec.cal_id[0] = '\0';
    g_dtec.ecu_name[0] = '\0';
    reset_codes();
}

void dtec_obd_init(void) {
    s_rx_line_pos = 0;
    dtec_obd_apply_profile(g_dtec.selected_profile);
    s_busy = false;
    s_busy_is_init = false;
    s_inflight[0] = '\0';
    s_retries = 0;
    s_pending[0] = '\0';
    s_slow_phase = 0;
    s_boost_samples = 0;
    s_single_reply = true;
    reset_vehicle();

    g_dtec.elm_inited = false;
    g_dtec.auto_poll = true;
    g_dtec.poll_phase = 0;

    // Reset all telemetry to UNSET / NO FAKE DATA
    g_dtec.rpm = 0.0f;
    g_dtec.has_rpm = false;
    g_dtec.peak_rpm = 0.0f;

    g_dtec.boost_kpa = 0.0f;
    g_dtec.map_kpa = 0;
    g_dtec.has_map = false;
    g_dtec.has_boost = false;
    g_dtec.peak_boost = 0.0f;

    g_dtec.tps_pct = 0.0f;
    g_dtec.has_tps = false;

    g_dtec.speed_kmh = 0;
    g_dtec.has_speed = false;

    g_dtec.coolant_c = -999;
    g_dtec.has_coolant = false;

    g_dtec.engine_load_pct = 0.0f;
    g_dtec.has_load = false;

    strncpy(g_dtec.battery_voltage, "--.-V", sizeof(g_dtec.battery_voltage) - 1);
    g_dtec.has_voltage = false;

    g_dtec.elm_version[0] = '\0';
    g_dtec.last_raw_line[0] = '\0';
    g_dtec.log_count = 0;

#ifndef DTEC_BRAND_NAME
#define DTEC_BRAND_NAME "DSOBD"
#endif

    dtec_log(DTEC_BRAND_NAME " OBD Link Ready");
}

// Send a command and start the prompt-wait clock for it.
static void send_tracked(const char *cmd, bool is_init) {
    // The retry path passes s_inflight itself.
    char next[sizeof(s_inflight)] = {0};
    snprintf(next, sizeof(next), "%s", cmd);
    dtec_net_send_cmd(next);
    memcpy(s_inflight, next, sizeof(s_inflight));
    s_reply_len = s_msg_count = 0;
    s_busy = true;
    s_busy_is_init = is_init;
    s_sent_ms = s_now_ms;
    s_timeout_ms = (strcmp(cmd, "ATZ") == 0) ? MS_RESET : MS_DEFAULT;
}

void dtec_obd_send_cmd(const char *cmd) {
    if (!cmd || cmd[0] == '\0' || strlen(cmd) >= sizeof(s_pending)) return;
    // Queue behind the in-flight reply; sending now would abort it with STOPPED.
    snprintf(s_pending, sizeof(s_pending), "%s", cmd);
}

void dtec_obd_scan_codes(bool clear_first) {
    if (!dtec_net_is_connected()) { dtec_log("Scan: adapter offline"); return; }
    s_job_count = s_job_index = 0;
    if (clear_first) s_jobs[s_job_count++] = "04";
    for (size_t i = 0; i < sizeof(k_scan_jobs) / sizeof(k_scan_jobs[0]); i++) s_jobs[s_job_count++] = k_scan_jobs[i];
    reset_codes();
    g_dtec.scanning = true;
    dtec_log(clear_first ? "Clearing codes..." : "Reading codes...");
}

void dtec_obd_format_dtc(uint16_t code, char out[8]) {
    snprintf(out, 8, "%c%04X", "PCBU"[code >> 14], code & 0x3FFF);
}

int dtec_obd_pid_index(uint8_t pid) {
    for (int i = 0; i < DTEC_PID_COUNT; i++) if (g_dtec_pids[i].pid == pid) return i;
    return -1;
}

void dtec_obd_on_connected(void) {
    s_init_index = 0;
    s_retries = 0;
    s_pending[0] = '\0';
    s_rx_line_pos = 0;
    s_slow_phase = 0;
    s_boost_samples = 0;
    g_dtec.poll_phase = 0;
    g_dtec.has_map = false;
    g_dtec.has_boost = false;
    g_dtec.peak_boost = 0.0f;
    g_dtec.hist_count = 0;
    memset(g_dtec.hist_boost, 0, sizeof(g_dtec.hist_boost));
    s_single_reply = true;
    reset_vehicle();
    g_dtec.elm_inited = false;

    // Greeting wait: many WiFi ELM327 clones emit a '>' prompt (or a reset
    // banner) the instant the socket opens. Hold ~0.5 s before the first ATZ so
    // that unsolicited prompt is not mistaken for ATZ's reply. An early '>'
    // clears the wait immediately (see dtec_obd_feed_rx).
    dtec_log("OBD link up; waiting for adapter...");
    s_busy = true;
    s_busy_is_init = false;
    s_inflight[0] = '\0';
    s_sent_ms = s_now_ms;
    s_timeout_ms = MS_GREETING;
}

static void strip_spaces(const char *src, char *dst, size_t dst_max) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_max; i++) {
        if (!isspace((unsigned char)src[i])) {
            dst[j++] = toupper((unsigned char)src[i]);
        }
    }
    dst[j] = '\0';
}

static unsigned hex_at(const char *s, int digits) {
    char b[9] = {0};
    memcpy(b, s, digits);
    return (unsigned)strtoul(b, NULL, 16);
}

// "41 PP A [B]" live values or "42 PP FF A [B]" freeze frame values.
static void store_pid(const char *compact, bool freeze) {
    int i = dtec_obd_pid_index((uint8_t)hex_at(compact + 2, 2));
    if (i < 0) return;
    const DTEC_Pid *p = &g_dtec_pids[i];
    size_t end = (freeze ? 6 : 4) + (p->skip + p->bytes) * 2u;
    if (strlen(compact) < end) return;
    float v = (float)hex_at(compact + end - p->bytes * 2, p->bytes * 2) * p->scale + p->offset;
    if (freeze) { g_dtec.ff_val[i] = v; g_dtec.ff_has[i] = true; }
    else { g_dtec.pid_val[i] = v; g_dtec.pid_has[i] = g_dtec.pid_fresh[i] = true; }
}

// Barometric PID 33 (read at connect) when the car reports it, else sea level.
static float ambient_kpa(void) {
    int i = dtec_obd_pid_index(0x33);
    return (i >= 0 && g_dtec.pid_has[i]) ? g_dtec.pid_val[i] : 101.0f;
}

static bool collects(const char *cmd) {
    return !strcmp(cmd, "03") || !strcmp(cmd, "07") || !strcmp(cmd, "0A") ||
           !strncmp(cmd, "09", 2) || !strncmp(cmd, "06", 2);
}

static void collect_line(const char *compact) {
    // CAN multi-frame: a byte-count line ("014"), then "0:", "1:", ... frames.
    const char *colon = strchr(compact, ':');
    const char *hex = colon ? colon + 1 : compact;
    size_t n = strlen(hex);
    if (n == 0 || (n & 1) || strspn(hex, "0123456789ABCDEF") != n) return;
    if (!colon || (colon == compact + 1 && compact[0] == '0')) {
        if (s_msg_count == (int)sizeof(s_msg_at)) return;
        s_msg_framed[s_msg_count] = colon != NULL;
        s_msg_at[s_msg_count++] = (uint8_t)s_reply_len;
    } else if (s_msg_count == 0) {
        return;
    }
    for (size_t i = 0; i < n && s_reply_len < (int)sizeof(s_reply); i += 2) {
        s_reply[s_reply_len++] = (uint8_t)hex_at(hex + i, 2);
    }
}

static void add_dtc(uint16_t code, uint8_t kind) {
    if (code == 0 || g_dtec.dtc_count == DTEC_MAX_DTCS) return;  // 0000 is padding
    for (int i = 0; i < g_dtec.dtc_count; i++) {
        if (g_dtec.dtc[i] == code && g_dtec.dtc_kind[i] == kind) return;
    }
    g_dtec.dtc[g_dtec.dtc_count] = code;
    g_dtec.dtc_kind[g_dtec.dtc_count++] = kind;
}

static int msg_len(int m) {
    return (m + 1 < s_msg_count ? s_msg_at[m + 1] : s_reply_len) - s_msg_at[m];
}

// Mode 09 text: VIN (02), first calibration ID (04), ECU name (0A). Count,
// sequence and padding bytes are never printable.
static void finish_info(unsigned pid) {
    char text[48];
    size_t len = 0;
    bool done = false;
    for (int m = 0; m < s_msg_count && !done; m++) {
        const uint8_t *b = s_reply + s_msg_at[m];
        int n = msg_len(m);
        if (n < 2 || b[0] != 0x49 || b[1] != pid) continue;
        if (len && s_msg_framed[m]) break;  // a second ECU's CAN reply
        for (int i = 2; i < n && !done; i++) {
            if (pid == 0x04 && b[i] == 0 && len) done = true;  // padding ends the first ID
            else if ((pid == 0x02 ? isalnum(b[i]) : b[i] >= 0x20 && b[i] < 0x7F) && len + 1 < sizeof(text)) {
                text[len++] = (char)b[i];
            }
        }
    }
    text[len] = '\0';
    if (pid == 0x02 && len >= 17) snprintf(g_dtec.vin, sizeof(g_dtec.vin), "%s", text + len - 17);
    if (pid == 0x04) snprintf(g_dtec.cal_id, sizeof(g_dtec.cal_id), "%s", text);
    if (pid == 0x0A) snprintf(g_dtec.ecu_name, sizeof(g_dtec.ecu_name), "%s", text);
}

// Mode 06, CAN only: 06A0 is "46 A0" + a 4-byte bitmap of monitor IDs A1-C0;
// each cylinder ID (A2-AD) answers 9-byte records MID TID UASID value min max.
// TID 0B is the 10-drive average misfire count, 0C this drive's count.
static void finish_tests(unsigned mid) {
    for (int m = 0; m < s_msg_count; m++) {
        const uint8_t *b = s_reply + s_msg_at[m];
        int n = msg_len(m);
        if (n < 1 || b[0] != 0x46) continue;
        if (mid == 0xA0) {
            // ISO 9141/KWP mode 06 lines are 7 bytes with another layout.
            if (n == 6 && b[1] == 0xA0) s_mids_a0 |= (uint32_t)b[2] << 24 | b[3] << 16 | b[4] << 8 | b[5];
            continue;
        }
        int cyl = (int)mid - 0xA2;
        for (int i = 1; i + 9 <= n; i += 9) {
            uint16_t v = (uint16_t)(b[i + 3] << 8 | b[i + 4]);
            if (b[i] != mid || (b[i + 1] != 0x0B && b[i + 1] != 0x0C)) continue;
            if (b[i + 1] == 0x0B) g_dtec.misfire_avg[cyl] = v;
            else g_dtec.misfire_drive[cyl] = v;
            g_dtec.misfire_cyls |= (uint16_t)(1u << cyl);
        }
    }
}

static void finish_reply(void) {
    if (!collects(s_inflight)) return;
    unsigned mode = hex_at(s_inflight, 2);
    if (mode == 0x09) { finish_info(hex_at(s_inflight + 2, 2)); return; }
    if (mode == 0x06) { finish_tests(hex_at(s_inflight + 2, 2)); return; }
    uint8_t kind = mode == 0x03 ? DTEC_DTC_STORED : mode == 0x07 ? DTEC_DTC_PENDING : DTEC_DTC_PERMANENT;
    for (int m = 0; m < s_msg_count; m++) {
        const uint8_t *b = s_reply + s_msg_at[m];
        int n = msg_len(m);
        if (n < 1 || b[0] != 0x40 + mode) continue;
        // CAN replies carry a code count (and are even length unless multi-frame);
        // ISO 9141/KWP/J1850 lines are always 43 plus three codes, zero padded.
        int i = 1, left = n;
        if (s_msg_framed[m] || n % 2 == 0) left = n > 1 ? b[i++] : 0;
        for (; i + 1 < n && left > 0; i += 2, left--) add_dtc((uint16_t)(b[i] << 8 | b[i + 1]), kind);
    }
}

static void parse_line(const char *line) {
    if (!line || line[0] == '\0') return;

    snprintf(g_dtec.last_raw_line, sizeof(g_dtec.last_raw_line), "%.63s", line);
    dtec_log(line);

    // Older clones reject the response-count suffix with "?": stop using it.
    if (strcmp(line, "?") == 0 && s_single_reply && strncmp(s_inflight, "01", 2) == 0 && strlen(s_inflight) == 5) {
        s_single_reply = false;
        dtec_log("Adapter: single-reply off");
        return;
    }

    // 1. ELM Identification
    if (strstr(line, "ELM327") || strstr(line, "OBDII to RS232")) {
        snprintf(g_dtec.elm_version, sizeof(g_dtec.elm_version), "%.31s", line);
        g_dtec.elm_inited = true;
        return;
    }

    // Protocol auto-detect ("SEARCHING...") can take several seconds; extend the
    // deadline for the command currently in flight so it is not timed out early.
    if (strstr(line, "SEARCHING")) {
        s_sent_ms = s_now_ms;
        s_timeout_ms = MS_SEARCH;
        return;
    }

    if (strcmp(s_inflight, "ATDP") == 0) {
        snprintf(g_dtec.protocol, sizeof(g_dtec.protocol), "%.39s", line);
        return;
    }
    if (collects(s_inflight)) {
        char hex[128];
        strip_spaces(line, hex, sizeof(hex));
        collect_line(hex);
        return;
    }

    // 2. Voltage String (e.g. "13.8V" or "12.6V")
    size_t len = strlen(line);
    if ((len >= 4 && len <= 8) && (line[len - 1] == 'V' || line[len - 1] == 'v')) {
        strncpy(g_dtec.battery_voltage, line, sizeof(g_dtec.battery_voltage) - 1);
        g_dtec.has_voltage = true;
        return;
    }

    // 3. Compact Hex Parsing
    char compact[128];
    strip_spaces(line, compact, sizeof(compact));

    // A long PID (EGT, DPF) arrives multi-frame on CAN; frame 0 holds the PID and first values.
    if (strncmp(compact, "0:", 2) == 0) memmove(compact, compact + 2, strlen(compact) - 1);
    size_t clen = strlen(compact);
    if (strncmp(compact, "41", 2) == 0 && clen >= 6) store_pid(compact, false);
    // Mode 02 freeze frame: PID 02 names the code that stored it.
    if (strncmp(compact, "42", 2) == 0 && clen >= 8) {
        if (strncmp(compact + 2, "02", 2) != 0) store_pid(compact, true);
        else if (clen >= 10) g_dtec.ff_dtc = (uint16_t)hex_at(compact + 6, 4);
        return;
    }

    // OBD Mode 1 PIDs 00/20/.../A0: supported PID bitmaps (41 00 A B C D)
    unsigned range = clen >= 12 ? hex_at(compact + 2, 2) : 1;
    if (strncmp(compact, "41", 2) == 0 && range % 0x20 == 0 && range <= 0xA0) {
        s_supported[range / 0x20] |= hex_at(compact + 4, 8);
        s_has_supported = true;
    }
    // OBD Mode 1 PID 01: MIL, code count and readiness. One line per ECU, so
    // the lamp and monitor bits are OR'd and the counts summed.
    else if (strncmp(compact, "4101", 4) == 0 && clen >= 12) {
        unsigned a = hex_at(compact + 4, 2), b = hex_at(compact + 6, 2);
        unsigned c = hex_at(compact + 8, 2), d = hex_at(compact + 10, 2);
        g_dtec.mil_on |= (a & 0x80) != 0;
        g_dtec.ecu_dtc_count += a & 0x7F;
        g_dtec.mon_supported |= (uint16_t)((b & 7) | (c << 3));
        g_dtec.mon_incomplete |= (uint16_t)(((b >> 4) & 7) | (d << 3));
        g_dtec.has_monitors = true;
    }
    // OBD Mode 1 PID 0C: Engine RPM (41 0C A B -> ((A * 256) + B) / 4)
    else if (strncmp(compact, "410C", 4) == 0 && strlen(compact) >= 8) {
        char hex_b[5] = { compact[4], compact[5], compact[6], compact[7], '\0' };
        unsigned long val = strtoul(hex_b, NULL, 16);
        g_dtec.rpm = (float)(val / 4);
        g_dtec.has_rpm = true;
        if (g_dtec.rpm > g_dtec.peak_rpm) g_dtec.peak_rpm = g_dtec.rpm;
    }
    // OBD Mode 1 PID 0B: Intake Manifold Absolute Pressure (MAP) (41 0B A -> A kPa)
    else if (strncmp(compact, "410B", 4) == 0 && strlen(compact) >= 6) {
        char hex_b[3] = { compact[4], compact[5], '\0' };
        unsigned long val = strtoul(hex_b, NULL, 16);
        g_dtec.map_kpa = (int)val;
        g_dtec.has_map = true;
        g_dtec.boost_kpa = (float)g_dtec.map_kpa - ambient_kpa();

        // Require three consecutive readings above ambient + 5 kPa to ignore noise.
        if (g_dtec.boost_kpa > 5.0f) {
            if (s_boost_samples < 3) s_boost_samples++;
            if (s_boost_samples == 3) g_dtec.has_boost = true;
        } else {
            s_boost_samples = 0;
        }
        if (g_dtec.has_boost && g_dtec.boost_kpa > g_dtec.peak_boost) {
            g_dtec.peak_boost = g_dtec.boost_kpa;
        }
    }
    // OBD Mode 1 PID 11: Throttle Position Sensor (41 11 A -> (A * 100) / 255 %)
    else if (strncmp(compact, "4111", 4) == 0 && strlen(compact) >= 6) {
        char hex_b[3] = { compact[4], compact[5], '\0' };
        unsigned long val = strtoul(hex_b, NULL, 16);
        g_dtec.tps_pct = ((float)val * 100.0f) / 255.0f;
        g_dtec.has_tps = true;
    }
    // OBD Mode 1 PID 0D: Vehicle Speed (41 0D A -> A km/h)
    else if (strncmp(compact, "410D", 4) == 0 && strlen(compact) >= 6) {
        char hex_b[3] = { compact[4], compact[5], '\0' };
        unsigned long val = strtoul(hex_b, NULL, 16);
        g_dtec.speed_kmh = (uint32_t)val;
        g_dtec.has_speed = true;
    }
    // OBD Mode 1 PID 05: Engine Coolant Temp (41 05 A -> A - 40 deg C)
    else if (strncmp(compact, "4105", 4) == 0 && strlen(compact) >= 6) {
        char hex_b[3] = { compact[4], compact[5], '\0' };
        long val = strtol(hex_b, NULL, 16);
        g_dtec.coolant_c = (int)(val - 40);
        g_dtec.has_coolant = true;
    }
    // OBD Mode 1 PID 04: Calculated Engine Load (41 04 A -> (A * 100) / 255 %)
    else if (strncmp(compact, "4104", 4) == 0 && strlen(compact) >= 6) {
        char hex_b[3] = { compact[4], compact[5], '\0' };
        unsigned long val = strtoul(hex_b, NULL, 16);
        g_dtec.engine_load_pct = ((float)val * 100.0f) / 255.0f;
        g_dtec.has_load = true;
    }
}

void dtec_obd_feed_rx(const uint8_t *data, size_t len) {
    if (!data || len == 0) return;

    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];

        if (c == '\r' || c == '\n' || c == '>') {
            if (s_rx_line_pos > 0) {
                s_rx_line[s_rx_line_pos] = '\0';
                g_dtec.lines_rx++;
                parse_line(s_rx_line);
                s_rx_line_pos = 0;
            }

            // '>' is the ELM327 prompt: the adapter has finished the current
            // command and is ready for the next. The scheduler sends the next
            // request on its following tick.
            if (c == '>') {
                finish_reply();
                s_busy = false;
                s_inflight[0] = '\0';
            }
        } else if (c >= 0x20 && c < 0x7F && s_rx_line_pos + 1 < sizeof(s_rx_line)) {
            s_rx_line[s_rx_line_pos++] = (char)c;
        }
    }
}

static bool pid_supported(uint8_t pid) {
    if (pid == 0x00 || pid > 0xC0 || !s_has_supported) return true;
    return (s_supported[(pid - 1) / 32] & (1u << (31 - (pid - 1) % 32))) != 0;
}

// Skip requests the car has already said it cannot answer. A range query
// such as 0160 runs only if the previous bitmap flagged PID 60.
static bool skip_job(const char *cmd) {
    if (strlen(cmd) < 4) return false;
    unsigned pid = hex_at(cmd + 2, 2);
    if (!strncmp(cmd, "01", 2) && strlen(cmd) == 4) return !pid_supported((uint8_t)pid);
    // Freeze frame values exist only when 020200 named the code that stored them.
    if (!strncmp(cmd, "02", 2) && pid != 0x02) return g_dtec.ff_dtc == 0;
    if (!strncmp(cmd, "06", 2) && pid > 0xA0) return !(s_mids_a0 & (1u << (0xC0 - pid)));
    return false;
}

int dtec_obd_sensor_list(int out[DTEC_PID_COUNT]) {
    int n = 0;
    for (int i = 0; i < DTEC_PID_COUNT; i++) if (pid_supported(g_dtec_pids[i].pid)) out[n++] = i;
    return n;
}

void dtec_obd_poll_tick(uint32_t now_ms) {
    s_now_ms = now_ms;

    if (!dtec_net_is_connected()) return;

    // Waiting on a '>' prompt for the in-flight command?
    if (s_busy) {
        if ((uint32_t)(s_now_ms - s_sent_ms) < s_timeout_ms) return;

        // Timed out. For init commands, resend a couple of times before moving
        // on (covers a dropped send or a lost prompt). A greeting wait has an
        // empty inflight string and simply expires.
        if (s_inflight[0] != '\0') {
            if (s_busy_is_init && s_retries < INIT_MAX_RETRY) {
                char m[48];
                snprintf(m, sizeof(m), "Retry %s", s_inflight);
                dtec_log(m);
                s_retries++;
                send_tracked(s_inflight, true);
                return;
            }
            char m[48];
            snprintf(m, sizeof(m), "No reply: %s", s_inflight);
            dtec_log(m);
        }
        s_busy = false;
        s_inflight[0] = '\0';
    }

    // Init phase: one command at a time, each gated on the previous prompt.
    while (s_init_index < s_init_count) {
        const char *cmd = s_init_cmds[s_init_index++];
        if (skip_job(cmd)) continue;
        s_retries = 0;
        send_tracked(cmd, true);
        return;
    }

    while (s_job_index < s_job_count) {
        const char *job = s_jobs[s_job_index++];
        if (skip_job(job)) continue;
        send_tracked(job, false);
        return;
    }
    if (g_dtec.scanning) {
        g_dtec.scanning = false;
        g_dtec.codes_read = true;
        dtec_log("Code scan complete");
    }

    if (s_pending[0] != '\0') {
        send_tracked(s_pending, false);
        s_pending[0] = '\0';
        return;
    }
    if (!g_dtec.auto_poll) return;

    char req[8];
    if (g_dtec.screen_mode == DTEC_MODE_SENSORS) {
        // Poll only the visible page so its rows refresh several times a second.
        int list[DTEC_PID_COUNT], n = dtec_obd_sensor_list(list);
        int first = g_dtec.sensor_page * DTEC_SENSOR_ROWS;
        if (first >= n) return;
        int rows = n - first < DTEC_SENSOR_ROWS ? n - first : DTEC_SENSOR_ROWS;
        s_sensor_slot = (s_sensor_slot + 1) % rows;
        snprintf(req, sizeof(req), "01%02X%s", g_dtec_pids[list[first + s_sensor_slot]].pid, s_single_reply ? "1" : "");
        send_tracked(req, false);
        return;
    }

    // RPM/speed get four of every five slots; secondary values share the rest.
    // Send as soon as the adapter is ready rather than adding a fixed delay.
    static const struct { const char *cmd; uint8_t pid; } fast[] = {
        { "010C", 0x0C }, { "010D", 0x0D }
    };
    static const struct { const char *cmd; uint8_t pid; } slow[] = {
        { "010B", 0x0B }, { "0111", 0x11 }, { "0105", 0x05 },
        { "0104", 0x04 }, { "ATRV", 0x00 }
    };
    const char *cmd = NULL;
    for (int tries = 0; tries < 5 && !cmd; tries++) {
        uint8_t phase = g_dtec.poll_phase;
        g_dtec.poll_phase = (phase + 1) % 5;
        if (phase < 4) {
            if (pid_supported(fast[phase & 1].pid)) cmd = fast[phase & 1].cmd;
            continue;
        }
        for (int i = 0; i < 5 && !cmd; i++) {
            uint8_t secondary = s_slow_phase;
            s_slow_phase = (secondary + 1) % 5;
            if (pid_supported(slow[secondary].pid)) cmd = slow[secondary].cmd;
        }
    }
    if (!cmd) return;
    snprintf(req, sizeof(req), "%s%s", cmd, (s_single_reply && cmd[0] == '0') ? "1" : "");
    send_tracked(req, false);
}
