#ifndef EVENT_H
#define EVENT_H

#include <SDL2/SDL_opengl.h>
#include "input.h"

void cursor_update(const Input *input);
void cursor_render();

#endif