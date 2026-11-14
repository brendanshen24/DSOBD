#include <3ds.h>
#include <math.h>
#include <string.h>

#include "dtec_audio.h"
#include "platform_3ds.h"

#define SAMPLE_RATE  16000
#define SAMPLE_COUNT 4096

static int16_t *s_samples = NULL;
static const int s_lengths[DTEC_SFX_COUNT] = {640, 1920, 3840, 3072};
static ndspWaveBuf s_wavebuf;
static bool s_ready = false;
static DTEC_NetStatus s_last_status = NET_DISCONNECTED;

void dtec_audio_init(void) {
    const float notes[DTEC_SFX_COUNT][3] = {
        {960, 960, 960},
        {660, 880, 1100},
        {523.25f, 659.25f, 783.99f},
        {330, 277, 220}
    };

    s_samples = (int16_t *)linearAlloc((size_t)DTEC_SFX_COUNT * SAMPLE_COUNT * sizeof(int16_t));
    if (!s_samples) {
        s_ready = false;
        return;
    }
    memset(s_samples, 0, (size_t)DTEC_SFX_COUNT * SAMPLE_COUNT * sizeof(int16_t));

    for (int sound = 0; sound < DTEC_SFX_COUNT; sound++) {
        int16_t *dst = s_samples + sound * SAMPLE_COUNT;
        int length = s_lengths[sound];
        int steps = sound == DTEC_SFX_MOVE ? 1 : 3;
        int step_length = length / steps;
        for (int i = 0; i < length; i++) {
            int note = i / step_length;
            int pos = i % step_length;
            float phase = 6.283185307f * notes[sound][note] * pos / SAMPLE_RATE;
            float envelope = sinf(3.141592654f * pos / (step_length - 1));
            dst[i] = (int16_t)(9000.0f * envelope * envelope *
                (sinf(phase) + 0.15f * sinf(phase * 2.0f)));
        }
    }

    DSP_FlushDataCache(s_samples, (size_t)DTEC_SFX_COUNT * SAMPLE_COUNT * sizeof(int16_t));

    if (R_FAILED(ndspInit())) {
        linearFree(s_samples);
        s_samples = NULL;
        s_ready = false;
        return;
    }

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, SAMPLE_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 0.85f;
    mix[1] = 0.85f;
    ndspChnSetMix(0, mix);

    s_last_status = g_dtec.net_status;
    s_ready = true;
}

void dtec_audio_play(DTEC_Sound sound) {
    if (!s_ready || !g_dtec.sound_enabled || (unsigned)sound >= DTEC_SFX_COUNT) return;

    ndspChnWaveBufClear(0);
    memset(&s_wavebuf, 0, sizeof(s_wavebuf));
    s_wavebuf.data_vaddr = s_samples + (int)sound * SAMPLE_COUNT;
    s_wavebuf.nsamples = (u32)s_lengths[sound];
    s_wavebuf.looping = false;
    ndspChnWaveBufAdd(0, &s_wavebuf);
}

void dtec_audio_set_enabled(bool enabled) {
    g_dtec.sound_enabled = enabled;
    if (s_ready && !enabled) {
        ndspChnWaveBufClear(0);
    }
}

void dtec_audio_update(void) {
    DTEC_NetStatus status = g_dtec.net_status;
    if (status == s_last_status) return;
    if (status == NET_OBD_CONNECTED) dtec_audio_play(DTEC_SFX_CONNECTED);
    else if (s_last_status == NET_OBD_CONNECTED) dtec_audio_play(DTEC_SFX_ERROR);
    s_last_status = status;
}

void dtec_audio_shutdown(void) {
    if (s_ready) {
        ndspChnWaveBufClear(0);
        ndspExit();
        s_ready = false;
    }
    if (s_samples) {
        linearFree(s_samples);
        s_samples = NULL;
    }
}
