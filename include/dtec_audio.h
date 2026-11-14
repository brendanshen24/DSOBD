#ifndef DTEC_AUDIO_H
#define DTEC_AUDIO_H

#include "dtec_common.h"

typedef enum {
    DTEC_SFX_MOVE, DTEC_SFX_CONFIRM, DTEC_SFX_CONNECTED, DTEC_SFX_ERROR,
    DTEC_SFX_COUNT
} DTEC_Sound;

void dtec_audio_init(void);
void dtec_audio_play(DTEC_Sound sound);
void dtec_audio_set_enabled(bool enabled);
void dtec_audio_update(void);

#endif
