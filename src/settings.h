#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include "input.h"
#include "screen.h"

void Settings_load(void);
void Settings_save(void);
void Settings_initialize(void);
void Settings_cleanup(void);
void Settings_toggle(void);
bool Settings_is_open(void);
void Settings_update(const Input *input, const unsigned int ticks);
void Settings_render(const Screen *screen);

#endif
