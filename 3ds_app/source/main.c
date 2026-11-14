#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>

#include "dtec_common.h"
#include "dtec_net.h"
#include "dtec_obd.h"
#include "dtec_audio.h"
#include "dtec_settings.h"
#include "dtec_logger.h"
#include "gfx3ds.h"
#include "platform_3ds.h"

DTEC_State g_dtec;

static uint64_t s_start_ms;

static uint32_t elapsed_ms(void) {
    return (uint32_t)(osGetTime() - s_start_ms);
}

static void service_link(void) {
    dtec_net_update();
    dtec_audio_update();
    if (g_dtec.screen_mode != DTEC_MODE_CAR_SELECT) {
        dtec_obd_poll_tick(elapsed_ms());
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    osSetSpeedupEnable(true);
    s_start_ms = osGetTime();

    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget *target_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *target_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    if (!gfx3ds_init()) {
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return 1;
    }

    memset(&g_dtec, 0, sizeof(g_dtec));
    g_dtec.screen_mode = DTEC_MODE_MULTI_GAUGE;
    dtec_obd_init();
    dtec_settings_init();
    dtec_audio_init();
    dtec_net_init();

    touchPosition touch;
    bool was_touching = false;
    uint32_t frame = 0;

    while (aptMainLoop()) {
        hidScanInput();
        u32 k_down = hidKeysDown();
        u32 k_held = hidKeysHeld();

        /* START + SELECT exits cleanly to Homebrew Launcher without overriding single START/SELECT. */
        if ((k_held & (KEY_START | KEY_SELECT)) == (KEY_START | KEY_SELECT)) {
            break;
        }

        if (k_held & KEY_TOUCH) {
            hidTouchRead(&touch);
            if (!was_touching) {
                dtec_3ds_handle_touch((int)touch.px, (int)touch.py, true);
            }
            was_touching = true;
        } else if (was_touching) {
            was_touching = false;
        }

        if (k_down) {
            dtec_3ds_handle_keys(k_down);
        }

        service_link();
        dtec_logger_tick(elapsed_ms());

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

        bool hud_black = g_dtec.screen_mode >= DTEC_MODE_DIGITAL_SPEED &&
                         g_dtec.screen_mode <= DTEC_MODE_BEAMNG_STYLE;

        gfx3ds_begin_frame();
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(target_top, hud_black ? CLR_BLACK : CLR_BG);
        C2D_SceneBegin(target_top);
        dtec_3ds_render_top();

        C2D_TargetClear(target_bottom, CLR_BG);
        C2D_SceneBegin(target_bottom);
        dtec_3ds_render_bottom();

        C3D_FrameEnd(0);
        service_link();
    }

    if (g_dtec.logging) {
        dtec_logger_toggle();
    }
    dtec_net_cleanup();
    dtec_audio_shutdown();

    gfx3ds_fini();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
