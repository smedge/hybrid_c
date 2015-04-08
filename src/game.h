#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "cursor.h"
#include "event.h"
#include "graphics.h"
#include "input.h"
#include "timer.h"

typedef struct Game {
	bool iconified;
	bool hasFocus;
	bool quit;
} Game;

void game_initialize(); 
void game_cleanup();
void game_loop();

#endif
