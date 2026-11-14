#include <fat.h>
#include <stdio.h>
#include <string.h>

#include "dtec_common.h"
#include "dtec_logger.h"

#define ROW_MS   100
#define FLUSH_MS 5000

static FILE *s_file;
static char s_buf[4096];
static bool s_clock_set;
static uint32_t s_t0, s_last_row, s_last_flush;

static void stop(const char *why) {
    bool ok = fclose(s_file) == 0 && !why;
    s_file = NULL;
    g_dtec.logging = false;
    char msg[48];
    if (ok) snprintf(msg, sizeof(msg), "Saved %lu rows %s", (unsigned long)g_dtec.log_rows, g_dtec.log_file);
    else snprintf(msg, sizeof(msg), "Log stopped: %s", why ? why : "write failed");
    dtec_log(msg);
}

static void start(void) {
    const char *drive = fatGetDefaultDrive();
    char path[300];
    for (int i = 1; drive && drive[0] && i <= 999; i++) {
        snprintf(path, sizeof(path), "%sdsobd-log-%03d.csv", drive, i);
        FILE *f = fopen(path, "r");
        if (f) { fclose(f); continue; }
        s_file = fopen(path, "w");
        snprintf(g_dtec.log_file, sizeof(g_dtec.log_file), "dsobd-log-%03d.csv", i);
        break;
    }
    if (!s_file) { dtec_log("Log: SD card unavailable"); return; }
    setvbuf(s_file, s_buf, _IOFBF, sizeof(s_buf));
    fputs("time_s", s_file);
    for (int i = 0; i < DTEC_PID_COUNT; i++) {
        const DTEC_Pid *p = &g_dtec_pids[i];
        fprintf(s_file, p->unit[0] ? ",%s (%s)" : ",%s", p->name, p->unit);
    }
    fputc('\n', s_file);
    memset(g_dtec.pid_fresh, 0, sizeof(g_dtec.pid_fresh));
    g_dtec.log_rows = 0;
    g_dtec.logging = true;
    s_clock_set = false;
    dtec_log("Logging to SD card");
}

void dtec_logger_toggle(void) {
    if (s_file) stop(NULL);
    else start();
}

void dtec_logger_tick(uint32_t now_ms) {
    if (!s_file) return;
    if (!s_clock_set) { s_t0 = s_last_row = s_last_flush = now_ms; s_clock_set = true; }
    if ((uint32_t)(now_ms - s_last_row) < ROW_MS) return;
    bool any = false;
    for (int i = 0; i < DTEC_PID_COUNT; i++) any |= g_dtec.pid_fresh[i];
    if (!any) return;
    s_last_row = now_ms;
    uint32_t t = now_ms - s_t0;
    fprintf(s_file, "%lu.%02lu", (unsigned long)(t / 1000), (unsigned long)(t % 1000 / 10));
    for (int i = 0; i < DTEC_PID_COUNT; i++) {
        if (g_dtec.pid_fresh[i]) fprintf(s_file, ",%.*f", g_dtec_pids[i].decimals, g_dtec.pid_val[i]);
        else fputc(',', s_file);
        g_dtec.pid_fresh[i] = false;
    }
    fputc('\n', s_file);
    g_dtec.log_rows++;
    // Bound what a power-off loses without stalling a frame on every row.
    if ((uint32_t)(now_ms - s_last_flush) >= FLUSH_MS) {
        s_last_flush = now_ms;
        if (fflush(s_file) != 0) { stop("SD write failed"); return; }
    }
    if (ferror(s_file)) stop("SD write failed");
}
