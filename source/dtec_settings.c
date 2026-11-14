#include <fat.h>
#include <stdio.h>
#include "dtec_common.h"
#include "dtec_settings.h"
#include "dtec_audio.h"

static char s_path[512];

void dtec_settings_init(void) {
    g_dtec.sound_enabled = true;
    g_dtec.settings_saved = false;
    g_dtec.settings_session_only = true;
    s_path[0] = '\0';
    if (!fatInitDefault()) return;
    const char *drive = fatGetDefaultDrive();
    if (!drive || snprintf(s_path, sizeof(s_path), "%sdsobd-settings.ini", drive) >= (int)sizeof(s_path)) {
        s_path[0] = '\0';
        return;
    }
    g_dtec.settings_session_only = false;
    FILE *f = fopen(s_path, "r");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        int value;
        if (sscanf(line, "sound_effects=%d", &value) == 1 && (value == 0 || value == 1)) {
            g_dtec.sound_enabled = value != 0;
            g_dtec.settings_saved = true;
        }
    }
    fclose(f);
}

bool dtec_settings_save(void) {
    g_dtec.settings_saved = false;
    g_dtec.settings_session_only = true;
    if (!s_path[0]) return false;
    FILE *f = fopen(s_path, "w");
    if (!f) return false;
    bool ok = fprintf(f, "sound_effects=%d\n", g_dtec.sound_enabled ? 1 : 0) > 0;
    if (fclose(f) != 0) ok = false;
    g_dtec.settings_saved = ok;
    g_dtec.settings_session_only = !ok;
    return ok;
}

void dtec_settings_toggle_sound(void) {
    dtec_audio_set_enabled(!g_dtec.sound_enabled);
    dtec_log(dtec_settings_save() ? "Sound preference saved" : "Sound changed for this session");
    dtec_audio_play(DTEC_SFX_CONFIRM);
}
