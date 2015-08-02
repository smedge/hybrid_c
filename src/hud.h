#ifndef HUD_H
#define HUD_H

#include "input.h"

void hud_update(const Input *input, const unsigned int ticks);
void hud_render();

#endif