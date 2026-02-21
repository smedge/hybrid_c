#ifndef HUD_H
#define HUD_H

#include "input.h"
#include "screen.h"

void Hud_initialize(void);
void Hud_cleanup(void);
void Hud_update(const Input *input, const unsigned int ticks);
void Hud_render(const Screen *screen);
float Hud_get_minimap_range(void);

#endif
