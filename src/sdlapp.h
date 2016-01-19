#ifndef SDLAPP_H
#define SDLAPP_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "mode.h"

typedef struct {
	bool iconified;
	bool hasFocus;
	bool quit;
	Mode mode;
} SdlApp;

void Sdlapp_run(void);

#endif
