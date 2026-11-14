#include <math.h>
#include "dtec_audio.h"

// Short, enveloped PCM cues. Generate once; ARM7 plays them asynchronously.
// One reserved channel bounds memory/voices and lets mute stop a cue immediately.
#define SAMPLE_RATE 16000
#define SAMPLE_COUNT 4096
#define SFX_CHANNEL 8
static int16_t s_samples[DTEC_SFX_COUNT][SAMPLE_COUNT] __attribute__((aligned(32)));
static const int s_lengths[DTEC_SFX_COUNT] = {640, 1920, 3840, 3072};
static bool s_ready;
static DTEC_NetStatus s_last_status;

void dtec_audio_init(void) {
    const float notes[DTEC_SFX_COUNT][3] = {
        {960, 960, 960}, {660, 880, 1100}, {523.25f, 659.25f, 783.99f}, {330, 277, 220}
    };
    for (int sound = 0; sound < DTEC_SFX_COUNT; sound++) {
        int length = s_lengths[sound];
        int steps = sound == DTEC_SFX_MOVE ? 1 : 3;
        int step_length = length / steps;
        for (int i = 0; i < length; i++) {
            int note = i / step_length;
            int pos = i % step_length;
            float phase = 6.283185307f * notes[sound][note] * pos / SAMPLE_RATE;
            float envelope = sinf(3.141592654f * pos / (step_length - 1));
            s_samples[sound][i] = (int16_t)(9000.0f * envelope * envelope *
                (sinf(phase) + 0.15f * sinf(phase * 2.0f)));
        }
    }
    DC_FlushRange(s_samples, sizeof(s_samples));
    soundEnable();
    s_last_status = g_dtec.net_status;
    s_ready = true;
}

void dtec_audio_play(DTEC_Sound sound) {
    if (!s_ready || !g_dtec.sound_enabled || (unsigned)sound >= DTEC_SFX_COUNT) return;
    soundPlaySampleChannel(SFX_CHANNEL, s_samples[sound], SoundFormat_16Bit,
                           s_lengths[sound] * sizeof(int16_t), SAMPLE_RATE, 80, 64, false, 0);
}

void dtec_audio_set_enabled(bool enabled) {
    g_dtec.sound_enabled = enabled;
    if (s_ready && !enabled) soundKill(SFX_CHANNEL);
}

void dtec_audio_update(void) {
    DTEC_NetStatus status = g_dtec.net_status;
    if (status == s_last_status) return;
    if (status == NET_OBD_CONNECTED) dtec_audio_play(DTEC_SFX_CONNECTED);
    else if (s_last_status == NET_OBD_CONNECTED) dtec_audio_play(DTEC_SFX_ERROR);
    s_last_status = status;
}
