#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psppower.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "config.h"
#include "net.h"
#include "obd.h"
#include "ui.h"

PSP_MODULE_INFO("DSOBD_PSP_Terminal", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

AppState g_state;
static volatile bool s_running = true;

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    s_running = false;
    sceKernelExitGame();
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

static void state_init(void) {
    memset(&g_state, 0, sizeof(g_state));
    g_state.ui_mode = UI_MODE_TERMINAL;
    g_state.net_status = NET_STATUS_DISCONNECTED;
    strncpy(g_state.status_msg, "Initializing...", sizeof(g_state.status_msg) - 1);
    strncpy(g_state.wifi_ssid, DEFAULT_OBD_SSID, sizeof(g_state.wifi_ssid) - 1);
    strncpy(g_state.target_ip, DEFAULT_OBD_IP, sizeof(g_state.target_ip) - 1);
    g_state.target_port = DEFAULT_OBD_PORT;
    g_state.coolant_temp_c = -999;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    setup_callbacks();

    // Boost PSP CPU to 333 MHz
    scePowerSetClockFrequency(333, 333, 166);

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    state_init();
    config_load();

    snprintf(g_state.wifi_ssid, sizeof(g_state.wifi_ssid), "%s", g_config.wifi_ssid);
    snprintf(g_state.target_ip, sizeof(g_state.target_ip), "%s", g_config.obd_ip);
    g_state.target_port = g_config.obd_port;
    g_state.apctl_profile_index = g_config.apctl_profile_index;

    ui_init();
    obd_init();
    net_init();

    SceCtrlData pad;
    uint32_t prev_buttons = 0;

    while (s_running) {
        sceCtrlReadBufferPositive(&pad, 1);
        uint32_t k_down = pad.Buttons & ~prev_buttons;
        prev_buttons = pad.Buttons;

        if (g_state.ui_mode == UI_MODE_WIFI_SCAN) {
            if (k_down & PSP_CTRL_UP) {
                if (g_state.scan_selected_idx > 0) {
                    g_state.scan_selected_idx--;
                }
            }
            if (k_down & PSP_CTRL_DOWN) {
                if (g_state.scan_selected_idx + 1 < g_state.scan_ap_count) {
                    g_state.scan_selected_idx++;
                }
            }
            if (k_down & PSP_CTRL_CROSS) {
                net_connect_to_ap_index(g_state.scan_selected_idx);
            }
            if (k_down & PSP_CTRL_CIRCLE) {
                net_stop_scan();
                ui_log_system("Wi-Fi selection cancelled");
            }
            if (k_down & PSP_CTRL_SQUARE) {
                net_start_scan();
            }
        } else {
            if (k_down & PSP_CTRL_CROSS) {
                obd_send_cmd("ATZ");
            }
            if (k_down & PSP_CTRL_CIRCLE) {
                obd_send_cmd("010C");
            }
            if (k_down & PSP_CTRL_SQUARE) {
                obd_send_cmd("010D");
            }
            if (k_down & PSP_CTRL_TRIANGLE) {
                obd_send_cmd("ATRV");
            }
            if (k_down & PSP_CTRL_LTRIGGER) {
                obd_send_cmd("0105");
            }
            if (k_down & PSP_CTRL_RTRIGGER) {
                obd_send_cmd("0100");
            }
            if (k_down & PSP_CTRL_START) {
                g_state.auto_poll = !g_state.auto_poll;
                ui_log_system(g_state.auto_poll ? "Auto-Polling: ENABLED" : "Auto-Polling: DISABLED");
            }
            if (k_down & PSP_CTRL_SELECT) {
                net_start_scan();
            }
            if (k_down & PSP_CTRL_UP) {
                ui_clear_log();
            }
        }

        net_update();

        if (g_state.ui_mode == UI_MODE_TERMINAL) {
            obd_poll_tick();
        }

        ui_render();
        sceDisplayWaitVblankStart();
    }

    sceKernelExitGame();
    return 0;
}
