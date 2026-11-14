#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "dtec_common.h"
#include "dtec_gfx.h"
#include "dtec_net.h"
#include "dtec_obd.h"
#include "dtec_audio.h"
#include "dtec_settings.h"
#include "dtec_logger.h"

DTEC_State g_dtec;

static volatile uint32_t s_millis;

static void clock_tick(void) {
    s_millis += 10;
}

// Forward declarations
void dtec_render_top(uint16_t *buf);
void dtec_render_bottom(uint16_t *buf);
void dtec_handle_touch(int tx, int ty, bool is_down);
void dtec_handle_keys(uint32_t keys);

// Offscreen Backbuffers for 100% flicker-free 60 FPS rendering
static uint16_t s_top_backbuffer[SCREEN_W * SCREEN_H];
static uint16_t s_bottom_backbuffer[SCREEN_W * SCREEN_H];

// DMA reads RAM, not the ARM9 data cache: flush first or real hardware shows stale RAM (white on R4).
static void present(uint16_t *vram_top, uint16_t *vram_bottom) {
    DC_FlushRange(s_top_backbuffer, sizeof(s_top_backbuffer));
    DC_FlushRange(s_bottom_backbuffer, sizeof(s_bottom_backbuffer));
    dmaCopyWords(3, s_top_backbuffer, vram_top, sizeof(s_top_backbuffer));
    dmaCopyWords(3, s_bottom_backbuffer, vram_bottom, sizeof(s_bottom_backbuffer));
}

#ifdef BOOT_TRACE
// Diagnostic build only: one solid colour per boot stage, so a hang on real hardware names its stage.
#ifndef BOOT_TRACE_FRAMES
#define BOOT_TRACE_FRAMES 60
#endif
#ifndef BOOT_TRACE_STAGES
#define BOOT_TRACE_STAGES 4
#endif
static void boot_mark(uint16_t *top, uint16_t *bottom, uint16_t color, int stage) {
    if (stage > BOOT_TRACE_STAGES) return;
    uint32_t fill = (uint32_t)(color | 0x8000) * 0x10001u;
    dmaFillWords(fill, top, 256 * 192 * 2);
    dmaFillWords(fill, bottom, 256 * 192 * 2);
    // ~1 s by polling the scanline counter: must not depend on IRQs, which may be what is broken.
    for (int i = 0; i < BOOT_TRACE_FRAMES; i++) { while (REG_VCOUNT != 192); while (REG_VCOUNT == 192); }
}
// Earliest possible sign of life, before any libnds video setup: magenta backdrop.
static void boot_mark0(void) {
    REG_DISPCNT = MODE_0_2D;            // display on, no layers: the backdrop fills the screen
    BG_PALETTE[0] = RGB15(31, 0, 31);
    for (int i = 0; i < BOOT_TRACE_FRAMES; i++) { while (REG_VCOUNT != 192); while (REG_VCOUNT == 192); }
}
#else
#define boot_mark(top, bottom, color, stage) ((void)0)
#define boot_mark0() ((void)0)
#endif

// Live Wi-Fi/socket processing plus the next OBD request once the adapter is ready.
static void service_link(void) {
    dtec_net_update();
    dtec_audio_update();
    // Use elapsed time even when rendering runs slower than 60 FPS.
    if (g_dtec.screen_mode != DTEC_MODE_CAR_SELECT) {
        dtec_obd_poll_tick(s_millis);
    }
}

extern const volatile unsigned char g_arm9_pad[];

int main(void) {
    (void)g_arm9_pad[0];  // keep the R4 alignment padding linked in
    boot_mark0();         // magenta: main() reached (trace builds only)
    if (isDSiMode()) {
        setCpuClock(true);
    }

    // 1. Configure screens: Top is main 2D engine, Bottom is sub 2D engine
    lcdMainOnTop();

    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);

    // Bank A (128 KB) -> Top Screen Bitmap
    vramSetBankA(VRAM_A_MAIN_BG);
    int bg_top = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    uint16_t *vram_top = bgGetGfxPtr(bg_top);

    // Bank C (128 KB) -> Bottom Touch Screen Bitmap
    vramSetBankC(VRAM_C_SUB_BG);
    int bg_sub = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    uint16_t *vram_bottom = bgGetGfxPtr(bg_sub);

    // Set hardware backdrop colors to cockpit dark blue
    BG_PALETTE[0] = DTEC_COLOR_BG;
    BG_PALETTE_SUB[0] = DTEC_COLOR_BG;

    // Clear hardware VRAM using 32-bit DMA word fills
    dmaFillWords((uint32_t)DTEC_COLOR_BG | ((uint32_t)DTEC_COLOR_BG << 16), vram_top, 256 * 192 * 2);
    dmaFillWords((uint32_t)DTEC_COLOR_BG | ((uint32_t)DTEC_COLOR_BG << 16), vram_bottom, 256 * 192 * 2);

    // 3. Initialize OBD engine and Wi-Fi
    memset(&g_dtec, 0, sizeof(g_dtec));
    g_dtec.screen_mode = DTEC_MODE_MULTI_GAUGE;
    boot_mark(vram_top, vram_bottom, RGB15(31, 0, 0), 1);   // red: main() reached
    timerStart(0, ClockDivider_1024, TIMER_FREQ_1024(100), clock_tick);
    dtec_obd_init();
    dtec_settings_init();
    boot_mark(vram_top, vram_bottom, RGB15(31, 31, 0), 2);  // yellow: SD card init returned
    dtec_audio_init();
    boot_mark(vram_top, vram_bottom, RGB15(0, 31, 0), 3);   // green: audio init returned
    dtec_net_init();
    boot_mark(vram_top, vram_bottom, RGB15(0, 0, 31), 4);   // blue: Wi-Fi init returned

    touchPosition touch;
    bool was_touching = false;
    uint32_t frame = 0;

    // 4. Main Event & Render Loop (60 FPS driven by VBlank)
    while (1) {
        // Yield to dswifi/lwIP cooperative threads until next VBlank IRQ
        cothread_yield_irq(IRQ_VBLANK);

        // Copy completed backbuffers into hardware VRAM during VBlank
        present(vram_top, vram_bottom);

        // Scan keys & touch
        scanKeys();
        uint32_t k_down = keysDown();
        uint32_t k_held = keysHeld();

        // Touch Input
        if (k_held & KEY_TOUCH) {
            touchRead(&touch);
            if (!was_touching) dtec_handle_touch(touch.px, touch.py, true);
            was_touching = true;
        } else if (was_touching) {
            was_touching = false;
        }

        dtec_handle_keys(k_down);

        service_link();
        dtec_logger_tick(s_millis);

        // Update real rolling telemetry history every 10 frames (~6 Hz)
        if ((frame++ % 10) == 0) {
            for (int i = 0; i < GRAPH_HISTORY_LEN - 1; i++) {
                g_dtec.hist_boost[i] = g_dtec.hist_boost[i + 1];
                g_dtec.hist_rpm[i] = g_dtec.hist_rpm[i + 1];
                g_dtec.hist_tps[i] = g_dtec.hist_tps[i + 1];
            }
            g_dtec.hist_boost[GRAPH_HISTORY_LEN - 1] = g_dtec.boost_kpa;
            g_dtec.hist_rpm[GRAPH_HISTORY_LEN - 1] = g_dtec.rpm;
            g_dtec.hist_tps[GRAPH_HISTORY_LEN - 1] = g_dtec.tps_pct;
            if (g_dtec.hist_count < GRAPH_HISTORY_LEN) g_dtec.hist_count++;
        }

        // Service the socket between renders too, so a reply that lands mid-frame
        // triggers the next request right away instead of waiting for VBlank.
        dtec_render_top(s_top_backbuffer);
        service_link();
        // The bottom screen is status/touch UI; 15 FPS frees CPU for the gauges.
        if ((frame & 3) == 1) {
            dtec_render_bottom(s_bottom_backbuffer);
            service_link();
        }
    }

    return 0;
}
