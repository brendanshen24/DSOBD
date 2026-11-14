#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psppower.h>
#include <stdio.h>
#include <string.h>

#include "dtec_common.h"
#include "dtec_net.h"
#include "dtec_obd.h"
#include "dtec_audio.h"
#include "dtec_settings.h"
#include "dtec_logger.h"
#include "gfx_psp.h"
#include "platform_psp.h"

PSP_MODULE_INFO("PSPOBD", 0, 2, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

DTEC_State g_dtec;
PspState g_psp;

static volatile bool s_running = true;
static SceInt64 s_start_us = 0;

uint32_t psp_elapsed_ms(void) {
    SceInt64 now = sceKernelGetSystemTimeWide();
    if (now < s_start_us) return 0;
    return (uint32_t)((now - s_start_us) / 1000);
}

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    s_running = false;
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void) {
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }
}

static void poll_psp_power(void) {
    if (scePowerIsBatteryExist()) {
        int pct = scePowerGetBatteryLifePercent();
        g_psp.battery_pct = (pct >= 0 && pct <= 100) ? pct : -1;
        g_psp.battery_charging = (scePowerIsBatteryCharging() != 0);
    } else {
        g_psp.battery_pct = -1;
        g_psp.battery_charging = false;
    }
    g_psp.ac_online = (scePowerIsPowerOnline() != 0);
}

static void service_link(void) {
    dtec_net_update();
    dtec_audio_update();
    if (g_dtec.screen_mode != DTEC_MODE_CAR_SELECT &&
        g_dtec.screen_mode != PSP_MODE_WIFI) {
        dtec_obd_poll_tick(psp_elapsed_ms());
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    setup_callbacks();
    scePowerSetClockFrequency(333, 333, 166);
    s_start_us = sceKernelGetSystemTimeWide();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    memset(&g_dtec, 0, sizeof(g_dtec));
    memset(&g_psp, 0, sizeof(g_psp));
    g_dtec.screen_mode = DTEC_MODE_MULTI_GAUGE;
    g_psp.battery_pct = -1;
    g_psp.peak_coolant_c = -999;

    if (!gfx_psp_init()) {
        sceKernelExitGame();
        return 1;
    }

    dtec_obd_init();
    dtec_settings_init();
    psp_ui_init();
    dtec_audio_init();
    dtec_net_init();
    poll_psp_power();

    SceCtrlData pad;
    uint32_t prev_buttons = 0;
    uint32_t frame = 0;

    while (s_running) {
        sceCtrlReadBufferPositive(&pad, 1);
        uint32_t k_down = pad.Buttons & ~prev_buttons;
        prev_buttons = pad.Buttons;

        int8_t lx = (int8_t)((int)pad.Lx - 128);
        int8_t ly = (int8_t)((int)pad.Ly - 128);

        psp_ui_handle_input(k_down, pad.Buttons, lx, ly);

        service_link();
        dtec_logger_tick(psp_elapsed_ms());

        if ((frame % 60u) == 0u) {
            poll_psp_power();
        }
        if ((frame % 120u) == 0u && g_psp.keep_awake &&
            (g_dtec.net_status == NET_OBD_CONNECTED || g_dtec.logging)) {
            scePowerTick(PSP_POWER_TICK_DISPLAY);
        }

        if ((frame++ % 10u) == 0u) {
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

        psp_ui_render();
        service_link();
    }

    if (g_dtec.logging) {
        dtec_logger_toggle();
    }
    psp_ui_fini();
    dtec_net_cleanup();
    dtec_audio_shutdown();
    gfx_psp_fini();
    sceKernelExitGame();
    return 0;
}
