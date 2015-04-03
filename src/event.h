#ifndef EVENT_H
#define EVENT_H

#include <SDL2/SDL.h>

#include "game.h"
#include "input.h"

void handle_sdl_events(Game *game, Input *input);

#endif