#include <pspkernel.h>
#include <pspaudio.h>
#include <math.h>
#include <string.h>

#include "dtec_audio.h"
#include "platform_psp.h"

#define AUDIO_RATE       44100
#define CHUNK_SAMPLES    512
#define MAX_SFX_SAMPLES  11264
#define SFX_TOTAL        (DTEC_SFX_COUNT + 1)
#define SFX_SHIFT_IDX    DTEC_SFX_COUNT

static int16_t s_samples[SFX_TOTAL][MAX_SFX_SAMPLES];
static const int s_lengths[SFX_TOTAL] = {1764, 5292, 10584, 8467, 3968};

static int s_channel = -1;
static SceUID s_thread = -1;
static SceUID s_sema = -1;
static volatile bool s_quit = false;
static volatile int s_active_sfx = -1;
static volatile int s_play_pos = 0;
static volatile uint32_t s_play_gen = 0;
static DTEC_NetStatus s_last_status = NET_DISCONNECTED;

static int audio_worker(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    static int16_t __attribute__((aligned(64))) chunk[CHUNK_SAMPLES];

    while (!s_quit) {
        sceKernelWaitSema(s_sema, 1, NULL);
        if (s_quit) break;

        while (!s_quit) {
            int sfx = s_active_sfx;
            uint32_t gen = s_play_gen;
            if (sfx < 0 || sfx >= SFX_TOTAL || !g_dtec.sound_enabled) {
                s_active_sfx = -1;
                break;
            }

            int pos = s_play_pos;
            int len = s_lengths[sfx];
            if (pos >= len) {
                if (s_play_gen == gen) s_active_sfx = -1;
                break;
            }

            int take = len - pos;
            if (take > CHUNK_SAMPLES) take = CHUNK_SAMPLES;
            memcpy(chunk, &s_samples[sfx][pos], (size_t)take * sizeof(int16_t));
            if (take < CHUNK_SAMPLES) {
                memset(chunk + take, 0, (size_t)(CHUNK_SAMPLES - take) * sizeof(int16_t));
            }
            if (s_play_gen == gen) {
                s_play_pos = pos + take;
            }

            sceAudioOutputBlocking(s_channel, PSP_AUDIO_VOLUME_MAX, chunk);
        }
    }
    return 0;
}

void dtec_audio_init(void) {
    const float notes[SFX_TOTAL][3] = {
        {960.0f, 960.0f, 960.0f},
        {660.0f, 880.0f, 1100.0f},
        {523.25f, 659.25f, 783.99f},
        {330.0f, 277.0f, 220.0f},
        {1480.0f, 0.0f, 1760.0f}
    };

    memset(s_samples, 0, sizeof(s_samples));
    for (int sfx = 0; sfx < SFX_TOTAL; sfx++) {
        int length = s_lengths[sfx];
        int steps = (sfx == DTEC_SFX_MOVE) ? 1 : 3;
        int step_len = length / steps;
        for (int i = 0; i < length; i++) {
            int note = i / step_len;
            if (note >= steps) note = steps - 1;
            int pos = i % step_len;
            float freq = notes[sfx][note];
            if (freq <= 1.0f) continue;
            float phase = 6.283185307f * freq * (float)pos / (float)AUDIO_RATE;
            float env = sinf(3.141592654f * (float)pos / (float)(step_len - 1));
            s_samples[sfx][i] = (int16_t)(9500.0f * env * env *
                (sinf(phase) + 0.15f * sinf(phase * 2.0f)));
        }
    }

    s_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, CHUNK_SAMPLES, PSP_AUDIO_FORMAT_MONO);
    if (s_channel < 0) return;

    s_sema = sceKernelCreateSema("dsobd_aud_sema", 0, 0, 1, NULL);
    if (s_sema < 0) {
        sceAudioChRelease(s_channel);
        s_channel = -1;
        return;
    }

    s_quit = false;
    s_active_sfx = -1;
    s_last_status = g_dtec.net_status;

    s_thread = sceKernelCreateThread("dsobd_audio", audio_worker, 0x18, 0x4000, PSP_THREAD_ATTR_USER, NULL);
    if (s_thread >= 0) {
        sceKernelStartThread(s_thread, 0, NULL);
    }
}

static void trigger_sfx(int idx) {
    if (s_channel < 0 || !g_dtec.sound_enabled || idx < 0 || idx >= SFX_TOTAL) return;
    s_play_gen++;
    s_active_sfx = idx;
    s_play_pos = 0;
    sceKernelSignalSema(s_sema, 1);
}

void dtec_audio_play(DTEC_Sound sound) {
    if ((unsigned)sound >= DTEC_SFX_COUNT) return;
    trigger_sfx((int)sound);
}

void psp_audio_play_shift(void) {
    if (!g_psp.shift_beep) return;
    trigger_sfx(SFX_SHIFT_IDX);
}

void dtec_audio_set_enabled(bool enabled) {
    g_dtec.sound_enabled = enabled;
    if (!enabled) {
        s_play_gen++;
        s_active_sfx = -1;
        s_play_pos = 0;
    }
}

void dtec_audio_update(void) {
    DTEC_NetStatus st = g_dtec.net_status;
    if (st == s_last_status) return;
    if (st == NET_OBD_CONNECTED) dtec_audio_play(DTEC_SFX_CONNECTED);
    else if (s_last_status == NET_OBD_CONNECTED) dtec_audio_play(DTEC_SFX_ERROR);
    s_last_status = st;
}

void dtec_audio_shutdown(void) {
    s_quit = true;
    s_active_sfx = -1;
    if (s_sema >= 0) sceKernelSignalSema(s_sema, 1);
    if (s_thread >= 0) {
        SceUInt timeout = 150000;
        sceKernelWaitThreadEnd(s_thread, &timeout);
        sceKernelDeleteThread(s_thread);
        s_thread = -1;
    }
    if (s_sema >= 0) {
        sceKernelDeleteSema(s_sema);
        s_sema = -1;
    }
    if (s_channel >= 0) {
        sceAudioChRelease(s_channel);
        s_channel = -1;
    }
}
