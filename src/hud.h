#ifndef HUD_H
#define HUD_H

#include <SDL2/SDL_opengl.h>

#include "input.h"
#include "screen.h"

void hud_update(const Input *input, const unsigned int ticks);
void hud_render(const Screen *screen);

#endif