#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "dtec_obd.h"
#include "dtec_net.h"
#include "dtec_gfx.h"
#include "dtec_audio.h"
#include "dtec_settings.h"
#include "dtec_logger.h"

DTEC_State g_dtec;
static char sent[256][16];
static int sent_count;
static uint32_t now;
static bool connected = true;
static char labels[128][80];
static uint16_t label_colors[128];
static int label_x[128], label_y[128], label_count;
void dtec_render_top(uint16_t *);
void dtec_render_bottom(uint16_t *);
void dtec_handle_touch(int, int, bool);
void dtec_handle_keys(uint32_t);

static int sounds_played, sounds_stopped;
static bool have_storage;
static char storage[256];
bool fatInitDefault(void) { return have_storage; }
const char *fatGetDefaultDrive(void) { return storage; }
void DC_FlushRange(const void *p, unsigned size) { (void)p; (void)size; }
void soundEnable(void) {}
void soundKill(int channel) { assert(channel == 8); sounds_stopped++; }
int soundPlaySampleChannel(int channel, const void *data, SoundFormat format, unsigned bytes,
        unsigned freq, unsigned volume, unsigned pan, bool loop, unsigned loop_point) {
    assert(channel == 8 && format == SoundFormat_16Bit && freq == 16000);
    assert(volume <= 127 && pan == 64 && !loop && loop_point == 0);
    assert((bytes % 4) == 0 && ((uintptr_t)data % 32) == 0);
    const int16_t *pcm = data;
    int peak = 0;
    for (unsigned i = 0; i < bytes / 2; i++) if (abs(pcm[i]) > peak) peak = abs(pcm[i]);
    assert(peak > 1000 && peak < 32767);
    assert(pcm[0] == 0 && abs(pcm[bytes / 2 - 1]) < 2);
    sounds_played++;
    return channel;
}

bool dtec_net_is_connected(void) { return connected; }
void dtec_net_reconnect(void) { dtec_obd_on_connected(); }
void dtec_net_send_cmd(const char *cmd) {
    assert(sent_count < 256);
    snprintf(sent[sent_count++], 16, "%s", cmd);
}
void gfx_clear(uint16_t *buf, uint16_t color) { (void)buf; (void)color; label_count = 0; }
void gfx_background(uint16_t *b) { (void)b; }
void gfx_panel(uint16_t *b, int x, int y, int w, int h, uint16_t c, uint16_t border) { (void)b; (void)x; (void)y; (void)w; (void)h; (void)c; (void)border; }
void gfx_dial(uint16_t *b, int x, int y, int r) { (void)b; (void)x; (void)y; (void)r; }
void gfx_gear(uint16_t *b, int x, int y, uint16_t c) { (void)b; (void)x; (void)y; (void)c; }
void gfx_pixel(uint16_t *b, int x, int y, uint16_t c) { (void)b; (void)x; (void)y; (void)c; }
void gfx_rect(uint16_t *b, int x, int y, int w, int h, uint16_t c) { (void)b; (void)x; (void)y; (void)w; (void)h; (void)c; }
void gfx_rect_fill(uint16_t *b, int x, int y, int w, int h, uint16_t c) { gfx_rect(b,x,y,w,h,c); }
void gfx_line(uint16_t *b, int x, int y, int w, int h, uint16_t c) { gfx_rect(b,x,y,w,h,c); }
void gfx_circle(uint16_t *b, int x, int y, int r, uint16_t c) { gfx_rect(b,x,y,r,r,c); }
void gfx_circle_fill(uint16_t *b, int x, int y, int r, uint16_t c) { gfx_circle(b,x,y,r,c); }
void gfx_draw_str(uint16_t *b, int x, int y, const char *s, uint16_t c, int scale) {
    (void)b; (void)scale;
    assert(label_count < 128);
    snprintf(labels[label_count], 80, "%s", s);
    label_x[label_count] = x; label_y[label_count] = y;
    label_colors[label_count++] = c;
}
void gfx_draw_str_clipped(uint16_t *b, int x, int y, int w, const char *s, uint16_t c, int scale) {
    (void)w; gfx_draw_str(b,x,y,s,c,scale);
}
int gfx_text_width(const char *s, int scale) { return (int)strlen(s) * 6 * scale; }
void gfx_draw_str_align(uint16_t *b, int x, int y, int a, const char *s, uint16_t c, int scale) {
    gfx_draw_str(b, x - gfx_text_width(s, scale) * a / 2, y, s, c, scale);
}
void gfx_seg_number(uint16_t *b, int x, int y, int w, int h, int t, int d, int val, uint16_t on, uint16_t off, int skew) {
    (void)w; (void)h; (void)t; (void)d; (void)off; (void)skew;
    char s[16]; if (val < 0) snprintf(s, sizeof(s), "---"); else snprintf(s, sizeof(s), "%d", val);
    gfx_draw_str(b, x, y, s, on, 1);
}
static bool contains(const char *s) {
    for (int i = 0; i < label_count; i++) if (strstr(labels[i], s)) return true;
    return false;
}
static bool has_label(const char *s) {
    for (int i = 0; i < label_count; i++) if (!strcmp(labels[i], s)) return true;
    return false;
}
static void rx(const char *s) { dtec_obd_feed_rx((const uint8_t *)s, strlen(s)); }
static void tick(uint32_t delta) { now += delta; dtec_obd_poll_tick(now); }
static void reset(void) {
    memset(&g_dtec, 0, sizeof(g_dtec)); sent_count = 0;
    dtec_obd_init(); dtec_obd_poll_tick(now);
    dtec_obd_on_connected();
}
// Expect the next request and answer it; NULL replies "NO DATA".
static void scan_reply(const char *cmd, const char *reply) {
    tick(10); assert(strcmp(sent[sent_count-1], cmd) == 0);
    rx(reply ? reply : "NO DATA\r>");
}
static void run_at_init(void) {
    static const char *at[] = {"ATZ","ATE0","ATL0","ATH0","ATAT1","ATSP0","ATRV"};
    for (int i = 0; i < 7; i++) scan_reply(at[i], "OK\r>");
}
// Every supported-PID range reports every PID.
static void init(void) {
    reset(); rx(">"); run_at_init();
    static const char *ranges[] = {"00", "20", "40", "60", "80", "A0"};
    for (int i = 0; i < 6; i++) {
        char cmd[8], reply[32];
        snprintf(cmd, sizeof(cmd), "01%s", ranges[i]);
        snprintf(reply, sizeof(reply), "41 %s FF FF FF FF\r>", ranges[i]);
        scan_reply(cmd, reply);
    }
    scan_reply("0133", NULL); sent_count = 0;
}
static void test_polling(void) {
    init(); tick(10); assert(strcmp(sent[0], "010C1") == 0);
    tick(100); assert(sent_count == 1);
    dtec_obd_send_cmd("ATRV"); assert(sent_count == 1);
    rx("41 0C 1F 40\r"); tick(10); assert(sent_count == 1);
    rx(">"); tick(10); assert(strcmp(sent[1], "ATRV") == 0);
    rx("13.8V\r>"); tick(10); assert(strcmp(sent[2], "010D1") == 0);
    rx("41 0D 64\r>"); assert(g_dtec.speed_kmh == 100); assert(g_dtec.rpm == 2000);
    tick(10); assert(strcmp(sent[3], "010C1") == 0); rx(">");
    tick(10); assert(strcmp(sent[4], "010D1") == 0); rx(">");
    tick(10); assert(strcmp(sent[5], "010B1") == 0);
    rx("41 0B 24\r>");
    sent_count = 0;
    const char *secondary[] = {"01111","01051","01041","ATRV","010B1"};
    for (int round = 0; round < 5; round++) {
        for (int f = 0; f < 4; f++) {
            tick(10); assert(strcmp(sent[sent_count-1], f & 1 ? "010D1" : "010C1") == 0); rx(">");
        }
        tick(10); assert(strcmp(sent[sent_count-1], secondary[round]) == 0); rx(">");
    }
    // Adapter rejects the response-count suffix: fall back to plain PIDs.
    init(); tick(10); assert(strcmp(sent[0], "010C1") == 0);
    rx("?\r>"); tick(10); assert(strcmp(sent[1], "010D") == 0);
    // Only RPM supported: do not wait on unsupported speed or slow PIDs.
    // Unsupported ranges and PID 33 are not queried at all.
    init(); dtec_obd_on_connected(); rx(">");
    run_at_init(); scan_reply("0100", "41 00 00 10 00 00\r>"); sent_count = 0;
    for (int i = 0; i < 6; i++) { tick(10); rx(">"); }
    assert(strcmp(sent[0], "010C1") == 0);
    for (int i = 0; i < sent_count; i++) assert(strcmp(sent[i], "010C1") == 0 || strcmp(sent[i], "ATRV") == 0);
    g_dtec.auto_poll = false; int n = sent_count; tick(10); assert(sent_count == n);
    dtec_obd_send_cmd("010D"); tick(10); assert(sent_count == n + 1);
}
static void test_timeouts(void) {
    init(); tick(10); assert(sent_count == 1);
    tick(2990); assert(sent_count == 1); tick(10); assert(sent_count == 2);
    rx("SEARCHING...\r"); tick(14990); assert(sent_count == 2); tick(10); assert(sent_count == 3);
    reset(); rx(">"); tick(10); assert(strcmp(sent[sent_count-1], "ATZ") == 0);
    tick(5000); assert(strcmp(sent[sent_count-1], "ATZ") == 0);
    tick(5000); assert(strcmp(sent[sent_count-1], "ATZ") == 0);
    tick(5000); assert(strcmp(sent[sent_count-1], "ATE0") == 0);
    now = UINT32_MAX - 100; reset(); rx(">"); tick(10);
    int n = sent_count; tick(4990); assert(sent_count == n); tick(10); assert(sent_count == n + 1);
}
static bool has_dtc(const char *want, int kind) {
    for (int i = 0; i < g_dtec.dtc_count; i++) {
        char code[8]; dtec_obd_format_dtc(g_dtec.dtc[i], code);
        if (!strcmp(code, want) && g_dtec.dtc_kind[i] == kind) return true;
    }
    return false;
}
static void test_codes(void) {
    init(); g_dtec.screen_mode = DTEC_MODE_CODES;
    dtec_handle_keys(KEY_A); assert(g_dtec.scanning);
    scan_reply("ATDP", "AUTO, ISO 15765-4 (CAN 11/500)\r>");
    // Two ECUs: engine with MIL + 2 codes, transmission with 1.
    scan_reply("0101", "41 01 82 07 65 04\r41 01 01 00 00 00\r>");
    // CAN: count byte, single frame from one ECU and multi-frame from another.
    scan_reply("03", "43 02 01 33 04 20\r00A\r0: 43 04 C1 23 81 00\r1: 00 01 01 71 00 00 00\r>");
    scan_reply("07", "43 00\r47 01 01 71\r>");
    scan_reply("0A", "4A 01 04 20\r>");
    scan_reply("0902", "014\r0: 49 02 01 31 48 47\r1: 43 4D 38 32 36 33 33\r2: 41 30 30 34 33 35 32\r>");
    // Calibration ID: zero padding ends it.
    scan_reply("0904", "013\r0: 49 04 01 33 37 38\r1: 30 35 52 42 42 41 35\r2: 33 30 00 00 00 00\r>");
    // Engine and transmission both answer: keep the first ECU's name.
    scan_reply("090A", "017\r0: 49 0A 01 45 43 4D\r1: 00 2D 45 6E 67 69 6E\r2: 65 43 6F 6E 74 72 6F\r3: 6C 00 00\r"
                       "017\r0: 49 0A 01 54 43 4D\r1: 00 2D 54 72 61 6E 73\r2: 6D 69 73 73 69 6F 6E\r3: 00 00 00\r>");
    scan_reply("011C", "41 1C 01\r>");
    scan_reply("0151", "41 51 01\r>");
    scan_reply("01A6", "41 A6 00 12 D6 87\r>");
    scan_reply("0121", "41 21 00 0C\r>");
    scan_reply("0131", "41 31 04 D2\r>");
    scan_reply("0130", "41 30 03\r>");
    scan_reply("020200", "42 02 00 01 33\r>");
    scan_reply("020400", "42 04 00 45\r>");
    scan_reply("020500", "42 05 00 84\r>");
    scan_reply("020C00", "42 0C 00 1F 40\r>");
    scan_reply("020D00", "42 0D 00 64\r>");
    // Misfire monitors A1-A5: cylinders 1-4. Cylinder 1 does not answer.
    scan_reply("06A0", "46 A0 F8 00 00 00\r>");
    scan_reply("06A2", NULL);
    scan_reply("06A3", "013\r0: 46 A3 0B 24 00 00\r1: 00 00 FF FF A3 0C 24\r2: 00 00 00 00 FF FF\r>");
    scan_reply("06A4", "013\r0: 46 A4 0B 24 00 06\r1: 00 00 FF FF A4 0C 24\r2: 00 0E 00 00 FF FF\r>");
    scan_reply("06A5", "013\r0: 46 A5 0B 24 00 00\r1: 00 00 FF FF A5 0C 24\r2: 00 00 00 00 FF FF\r>");
    tick(10); assert(!g_dtec.scanning && g_dtec.codes_read);
    assert(strcmp(sent[sent_count-1], "010C1") == 0); rx(">");  // polling resumes
    assert(g_dtec.mil_on && g_dtec.ecu_dtc_count == 3);
    assert(__builtin_popcount(g_dtec.mon_supported) == 7);
    assert(__builtin_popcount(g_dtec.mon_supported & ~g_dtec.mon_incomplete) == 6);
    assert(has_dtc("P0133", 0) && has_dtc("P0420", 0) && has_dtc("U0123", 0) && has_dtc("B0100", 0));
    assert(has_dtc("P0001", 0) && has_dtc("P0171", 1) && has_dtc("P0420", 2));
    assert(has_dtc("P0171", 0));
    assert(g_dtec.dtc_count == 8);  // 0000 padding is dropped, nothing past the CAN count
    assert(strcmp(g_dtec.vin, "1HGCM82633A004352") == 0);
    assert(strcmp(g_dtec.protocol, "AUTO, ISO 15765-4 (CAN 11/500)") == 0);
    char code[8]; dtec_obd_format_dtc(g_dtec.ff_dtc, code); assert(!strcmp(code, "P0133"));
    int rpm = dtec_obd_pid_index(0x0C);
    assert(g_dtec.ff_has[rpm] && g_dtec.ff_val[rpm] == 2000);
    uint16_t dummy;
    dtec_render_top(&dummy);
    assert(contains("CHECK ENGINE ON")); assert(contains("READINESS 6/7")); assert(contains("VIN 1HGCM82633A004352"));
    assert(contains("FREEZE FRAME  P0133")); assert(contains("2000rpm")); assert(contains("CLEARED 1234km AGO  MIL 12km"));
    dtec_render_bottom(&dummy); assert(contains("P0420 PERM")); assert(contains("P0171 PENDING"));
    assert(!strcmp(g_dtec.cal_id, "37805RBBA530") && !strcmp(g_dtec.ecu_name, "ECM-EngineControl"));
    assert(g_dtec.misfire_cyls == 0xE && g_dtec.misfire_drive[2] == 14 && g_dtec.misfire_avg[2] == 6);
    g_dtec.screen_mode = DTEC_MODE_ECU_INFO; dtec_render_top(&dummy);
    assert(contains("ECU ECM-EngineControl")); assert(contains("CAL 37805RBBA530"));
    assert(contains("OBD OBD-II CARB")); assert(contains("GASOLINE")); assert(contains("ODOMETER 123457km"));
    assert(has_label("C3")); assert(has_label("14")); assert(has_label("6")); assert(!has_label("C1"));
    int n = sent_count; dtec_handle_touch(20, 40, true); assert(g_dtec.scanning);
    scan_reply("ATDP", NULL); assert(sent_count == n + 1);

    // ISO 9141 / KWP: no count byte, three codes per line, zero padded.
    init(); dtec_obd_scan_codes(false);
    scan_reply("ATDP", NULL); scan_reply("0101", NULL);
    scan_reply("03", "43 01 33 02 44 00 00\r43 03 00 00 00 00 00\r>");
    assert(g_dtec.dtc_count == 3 && has_dtc("P0133", 0) && has_dtc("P0244", 0) && has_dtc("P0300", 0));
    scan_reply("07", NULL); scan_reply("0A", NULL); scan_reply("0902", NULL);
    scan_reply("0904", NULL); scan_reply("090A", NULL);
    scan_reply("011C", NULL); scan_reply("0151", NULL); scan_reply("01A6", NULL);
    scan_reply("0121", NULL); scan_reply("0131", NULL); scan_reply("0130", NULL);
    scan_reply("020200", NULL);
    // No freeze frame: its value requests are skipped. A 7-byte K-line mode 06
    // line is not a CAN monitor bitmap, so no cylinder requests follow.
    scan_reply("06A0", "46 A0 01 00 00 00 00\r>");
    tick(10); assert(g_dtec.codes_read && strcmp(sent[sent_count-1], "010C1") == 0);

    // Clear needs two presses, then sends 04 before rescanning.
    init(); g_dtec.screen_mode = DTEC_MODE_CODES;
    dtec_handle_keys(KEY_Y); tick(10); assert(sent_count == 1 && g_dtec.clear_armed);
    rx(">"); dtec_handle_touch(200, 40, true); assert(!g_dtec.clear_armed && g_dtec.scanning);
    tick(10); assert(strcmp(sent[sent_count-1], "04") == 0); rx("44\r>");
    tick(10); assert(strcmp(sent[sent_count-1], "ATDP") == 0);
    dtec_handle_touch(240, 10, true); assert(g_dtec.screen_mode == DTEC_MODE_SENSORS && !g_dtec.clear_armed);
    connected = false; g_dtec.scanning = false; dtec_obd_scan_codes(false); assert(!g_dtec.scanning); connected = true;
}
static void test_sensors(void) {
    reset(); rx(">");
    // 01-20: 04 05 0C 0D 0F 11 + next range; 21-40: 2F 31 33 + next; 41-60: 46 5C.
    static const char *cmds[] = {"ATZ","ATE0","ATL0","ATH0","ATAT1","ATSP0","ATRV","0100","0120","0140","0133"};
    static const char *replies[] = {"OK\r>","OK\r>","OK\r>","OK\r>","OK\r>","OK\r>","12.6V\r>",
        "41 00 18 1A 80 01\r>", "41 20 00 02 A0 01\r>", "41 40 04 00 00 10\r>", "41 33 55\r>"};
    for (int i = 0; i < 11; i++) { tick(10); assert(!strcmp(sent[sent_count-1], cmds[i])); rx(replies[i]); }
    int list[DTEC_PID_COUNT], n = dtec_obd_sensor_list(list);
    assert(n == 11);
    // Altitude: 85 kPa ambient, so 128 kPa MAP is +43 boost.
    rx("41 0B 80\r"); assert(g_dtec.boost_kpa == 43);
    g_dtec.screen_mode = DTEC_MODE_SENSORS; sent_count = 0;
    for (int i = 0; i < 16; i++) { tick(10); rx(">"); }
    // Page 1 polls only its eight rows.
    for (int i = 0; i < sent_count; i++) {
        unsigned pid = (unsigned)strtoul(sent[i] + 2, NULL, 16) >> 4;
        bool on_page = false;
        for (int r = 0; r < DTEC_SENSOR_ROWS; r++) on_page |= g_dtec_pids[list[r]].pid == pid;
        assert(on_page && strlen(sent[i]) == 5);
    }
    rx("41 0F 46\r41 2F 80\r41 5C 7D\r");
    dtec_handle_keys(KEY_DOWN); assert(g_dtec.sensor_page == 1);
    uint16_t dummy; dtec_render_top(&dummy);
    assert(contains("PAGE 2/2  11 PIDS")); assert(contains("OIL TEMP")); assert(contains("85C"));
    dtec_handle_keys(KEY_DOWN); assert(g_dtec.sensor_page == 0);
    dtec_render_top(&dummy); assert(contains("INTAKE AIR")); assert(contains("30C"));
    dtec_handle_touch(20, 40, true); assert(g_dtec.sensor_page == 1);

    // PIDs above 60: a sensor-present byte before the value, 4-byte odometer,
    // and a 9-byte EGT reply that arrives as CAN frames.
    init();
    rx("41 77 01 46 00 00 00\r41 A6 00 12 D6 87\r00B\r0: 41 78 01 17 0C 00 00\r1: 00 00 00 00 00\r");
    assert(g_dtec.pid_val[dtec_obd_pid_index(0x77)] == 30);
    assert(fabsf(g_dtec.pid_val[dtec_obd_pid_index(0x78)] - 550) < 0.01f);
    assert(fabsf(g_dtec.pid_val[dtec_obd_pid_index(0xA6)] - 123456.7f) < 0.05f);
    assert(dtec_obd_sensor_list(list) == DTEC_PID_COUNT);
}
static void test_logger(void) {
    char dir[] = "/tmp/dsobd-log.XXXXXX", path[300], line[1024], want[1024];
    assert(mkdtemp(dir));
    snprintf(storage, sizeof(storage), "%s/", dir);
    snprintf(path, sizeof(path), "%sdsobd-log-001.csv", storage);
    FILE *f = fopen(path, "w"); assert(f); fclose(f);  // an earlier log is never overwritten
    init(); g_dtec.screen_mode = DTEC_MODE_SENSORS;
    rx("41 0C 1F 40\r");  // read before logging started: not logged
    dtec_handle_touch(200, 40, true); assert(g_dtec.logging && !strcmp(g_dtec.log_file, "dsobd-log-002.csv"));
    uint16_t dummy; dtec_render_top(&dummy); assert(contains("REC"));
    dtec_render_bottom(&dummy); assert(contains("R / STOP")); assert(contains("REC dsobd-log-002.csv 0"));
    dtec_logger_tick(1000);
    rx("41 0D 64\r41 05 84\r");
    dtec_logger_tick(1050); assert(g_dtec.log_rows == 0);
    dtec_logger_tick(1100); assert(g_dtec.log_rows == 1);
    dtec_logger_tick(1300); assert(g_dtec.log_rows == 1);  // nothing new read
    rx("41 0C 1F 40\r"); dtec_logger_tick(1400); assert(g_dtec.log_rows == 2);
    dtec_handle_keys(KEY_R); assert(!g_dtec.logging);
    snprintf(path, sizeof(path), "%sdsobd-log-002.csv", storage);
    f = fopen(path, "r"); assert(f);
    assert(fgets(line, sizeof(line), f) && !strncmp(line, "time_s,ENGINE LOAD (%),COOLANT (C),", 35));
    assert(strstr(line, ",FUEL TYPE,") && strstr(line, ",ODOMETER (km)\n"));
    for (int row = 0; row < 2; row++) {
        int len = snprintf(want, sizeof(want), row ? "0.40" : "0.10");
        for (int i = 0; i < DTEC_PID_COUNT; i++) {
            uint8_t pid = g_dtec_pids[i].pid;
            const char *v = row ? (pid == 0x0C ? "2000" : "") : pid == 0x05 ? "92" : pid == 0x0D ? "100" : "";
            len += snprintf(want + len, sizeof(want) - len, ",%s", v);
        }
        snprintf(want + len, sizeof(want) - len, "\n");
        assert(fgets(line, sizeof(line), f) && !strcmp(line, want));
    }
    assert(!fgets(line, sizeof(line), f)); fclose(f);
    assert(unlink(path) == 0);
    snprintf(path, sizeof(path), "%sdsobd-log-001.csv", storage);
    assert(unlink(path) == 0 && rmdir(dir) == 0);
    // No card: logging refuses to start.
    storage[0] = '\0'; dtec_logger_toggle(); assert(!g_dtec.logging);
    assert(!strcmp(g_dtec.log_lines[g_dtec.log_count - 1], "Log: SD card unavailable"));
}
static void test_boost(void) {
    reset();
    rx("41 0B 24\r41 0B 65\r41 0B 6A\r");
    assert(g_dtec.has_map); assert(!g_dtec.has_boost);
    rx("41 0B 80\r41 0B 65\r41 0B 80\r41 0B 80\r"); assert(!g_dtec.has_boost);
    rx("41 0B 80\r"); assert(g_dtec.has_boost); assert(g_dtec.peak_boost == 27);
    rx("41 0B 24\r"); assert(g_dtec.has_boost); assert(g_dtec.boost_kpa == -65);
    dtec_obd_on_connected(); assert(!g_dtec.has_boost); assert(!g_dtec.has_map);
}
static void test_screens(void) {
    uint16_t dummy;
    reset(); rx("41 0C 1F 40\r41 0D 64\r41 05 84\r");
    g_dtec.screen_mode = DTEC_MODE_MULTI_GAUGE; dtec_render_top(&dummy);
    assert(contains("100 km/h")); assert(contains("92 C")); assert(!contains("BOOST"));
    assert(!contains("mph")); assert(!contains("PSI"));
    g_dtec.screen_mode = DTEC_MODE_ANALOG_DIALS; dtec_render_top(&dummy);
    assert(contains("TACH")); assert(contains("SPEED")); assert(contains("km/h")); assert(contains("92C"));
    assert(!contains("BOOST"));
    g_dtec.screen_mode = DTEC_MODE_DIGITAL_SPEED; dtec_render_top(&dummy);
    assert(contains("100")); assert(contains("2000")); assert(contains("km/h"));
    g_dtec.screen_mode = DTEC_MODE_BIG_SPEED; dtec_render_top(&dummy); assert(contains("100"));
    g_dtec.screen_mode = DTEC_MODE_ARC_TACH; dtec_render_top(&dummy);
    assert(contains("100")); assert(contains("2000 RPM")); assert(!contains("BST"));
    g_dtec.screen_mode = DTEC_MODE_SIM_DASH; dtec_render_top(&dummy);
    assert(contains("100")); assert(contains("92C")); assert(contains("LOAD"));
    g_dtec.screen_mode = DTEC_MODE_GT_STYLE; dtec_render_top(&dummy);
    assert(contains("100")); assert(contains("2000 rpm")); assert(contains("km/h"));
    g_dtec.screen_mode = DTEC_MODE_FORZA_STYLE; dtec_render_top(&dummy);
    assert(contains("100")); assert(contains("KM/H")); assert(!contains("kPa"));
    g_dtec.screen_mode = DTEC_MODE_BEAMNG_STYLE; dtec_render_top(&dummy);
    assert(contains("100")); assert(contains("2000 rpm")); assert(contains("Temp 92C"));
    dtec_render_bottom(&dummy); assert(contains("BEAMNG STYLE"));
    dtec_handle_touch(240, 10, true); assert(g_dtec.screen_mode == DTEC_MODE_ANALOG_DIALS);
    dtec_handle_touch(10, 10, true); assert(g_dtec.screen_mode == DTEC_MODE_BEAMNG_STYLE);
    g_dtec.screen_mode = DTEC_MODE_CAR_SELECT;
    dtec_handle_touch(10, 10, true); assert(g_dtec.screen_mode == DTEC_MODE_SETTINGS);
    g_dtec.screen_mode = DTEC_MODE_BOOST_SETUP; dtec_render_top(&dummy); assert(!contains("BOOST"));
    g_dtec.screen_mode = DTEC_MODE_REALTIME_GRAPH; dtec_render_top(&dummy); assert(!contains("BOOST")); assert(!contains("kPa"));
    rx("41 0B 80\r41 0B 80\r41 0B 80\r");
    g_dtec.screen_mode = DTEC_MODE_MULTI_GAUGE; dtec_render_top(&dummy); assert(contains("BOOST")); assert(contains("+27kPa"));
    g_dtec.screen_mode = DTEC_MODE_CAR_SELECT;
    // Same selection path as D-pad: every profile remains on a visible row.
    for (int direction = 0; direction < 2; direction++) for (int i = 0; i < DTEC_PROFILE_COUNT; i++) {
        int idx = direction ? DTEC_PROFILE_COUNT - i - 1 : i;
        dtec_obd_apply_profile(idx); dtec_render_bottom(&dummy);
        bool visible = false;
        for (int j = 0; j < label_count; j++) if (!strcmp(labels[j], g_dtec_profiles[idx].name)) {
            assert(label_colors[j] == DTEC_COLOR_WHITE); assert(label_y[j] >= 28 && label_y[j] < 160); visible = true;
        }
        assert(visible);
    }
    dtec_obd_apply_profile(0); dtec_handle_touch(150, 175, true); assert(g_dtec.selected_profile == 4);
    dtec_handle_touch(150, 175, true); assert(g_dtec.selected_profile == 8);
    dtec_handle_touch(150, 175, true); assert(g_dtec.selected_profile == 12);
    dtec_handle_touch(20, 67, true); assert(g_dtec.selected_profile == 13);
    dtec_handle_touch(150, 175, true); assert(g_dtec.selected_profile == 1);
    dtec_handle_touch(20, 175, true); assert(g_dtec.selected_profile == 13);
    dtec_handle_touch(210, 175, true); assert(g_dtec.profile_confirmed); assert(g_dtec.screen_mode == DTEC_MODE_OBD_TERMINAL);
}
static void test_audio_settings(void) {
    reset();
    have_storage = false;
    dtec_settings_init();
    assert(g_dtec.sound_enabled && !g_dtec.settings_saved);
    dtec_audio_init();
    for (int i = 0; i < DTEC_SFX_COUNT; i++) dtec_audio_play(i);
    assert(sounds_played == DTEC_SFX_COUNT);
    g_dtec.screen_mode = DTEC_MODE_MULTI_GAUGE;
    dtec_handle_touch(190, 10, true); assert(g_dtec.screen_mode == DTEC_MODE_SETTINGS);
    dtec_handle_touch(200, 60, true); assert(!g_dtec.sound_enabled);
    assert(sounds_stopped == 1 && !g_dtec.settings_saved && g_dtec.settings_session_only);
    int count = sounds_played;
    dtec_handle_keys(KEY_X);
    dtec_handle_touch(80, 120, true);
    g_dtec.net_status = NET_OBD_CONNECTED; dtec_audio_update();
    g_dtec.net_status = NET_DISCONNECTED; dtec_audio_update();
    assert(sounds_played == count);
    // Settings must never issue OBD commands or toggle polling.
    int cmds = sent_count; bool poll = g_dtec.auto_poll;
    dtec_handle_keys(KEY_START | KEY_Y | KEY_R | KEY_L);
    assert(sent_count == cmds && g_dtec.auto_poll == poll);
    dtec_handle_keys(KEY_A); assert(g_dtec.sound_enabled && sounds_played == count + 1);
    dtec_handle_keys(KEY_X); assert(sounds_played == count + 2);
    dtec_handle_keys(KEY_B); assert(g_dtec.screen_mode == DTEC_MODE_MULTI_GAUGE);
    count = sounds_played;
    g_dtec.net_status = NET_OBD_CONNECTED; dtec_audio_update(); dtec_audio_update();
    assert(sounds_played == count + 1);
    g_dtec.net_status = NET_ERROR; dtec_audio_update(); dtec_audio_update();
    assert(sounds_played == count + 2);
    // Save, reload and reject invalid values using the real file code.
    char dir[] = "/tmp/dsobd-settings.XXXXXX";
    assert(mkdtemp(dir));
    snprintf(storage, sizeof(storage), "%s/", dir); have_storage = true;
    dtec_settings_init();
    dtec_settings_toggle_sound(); assert(!g_dtec.sound_enabled && g_dtec.settings_saved);
    dtec_settings_init(); assert(!g_dtec.sound_enabled && g_dtec.settings_saved);
    dtec_settings_toggle_sound(); dtec_settings_init(); assert(g_dtec.sound_enabled);
    char path[300]; snprintf(path, sizeof(path), "%sdsobd-settings.ini", storage);
    FILE *f = fopen(path, "w"); assert(f); fputs("sound_effects=9\nunknown=0\n", f); fclose(f);
    dtec_settings_init(); assert(g_dtec.sound_enabled && !g_dtec.settings_saved);
    assert(unlink(path) == 0 && rmdir(dir) == 0);
    assert(!dtec_settings_save());
}

int main(void) {
    test_polling(); test_timeouts(); test_boost(); test_screens(); test_codes(); test_sensors();
    test_audio_settings(); test_logger();
    puts("DS telemetry, codes, misfire, ECU info, sensors, CSV log, screens, navigation, PCM cues and settings passed");
}
