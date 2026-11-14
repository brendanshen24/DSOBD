#ifndef DTEC_SETTINGS_H
#define DTEC_SETTINGS_H

#include <stdbool.h>

void dtec_settings_init(void);
bool dtec_settings_save(void);
void dtec_settings_toggle_sound(void);

#endif
