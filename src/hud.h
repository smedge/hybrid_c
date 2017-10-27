#ifndef HUD_H
#define HUD_H

#include <SDL2/SDL_opengl.h>

#define TITLE_FONT_PATH "./resources/fonts/square_sans_serif_7.ttf"

#include "input.h"
#include "screen.h"

void Hud_initialize();
void Hud_cleanup();
void Hud_update(const Input *input, const unsigned int ticks);
void Hud_render(const Screen *screen);

#endif