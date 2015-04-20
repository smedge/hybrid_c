#ifndef SDLAPP_H
#define SDLAPP_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "graphics.h"
#include "input.h"
#include "timer.h"
#include "cursor.h"

typedef struct {
	bool iconified;
	bool hasFocus;
	bool quit;
} SdlApp;

void sdlapp_initialize(); 
void sdlapp_cleanup();
void sdlapp_loop();

#endif
