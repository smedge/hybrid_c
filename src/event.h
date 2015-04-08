#ifndef EVENT_H
#define EVENT_H

#include <SDL2/SDL.h>

#include "input.h"
#include "game.h"
#include "graphics.h"

void handle_sdl_events(Game *game, Input *input);

#endif