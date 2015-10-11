#ifndef SDLAPP_H
#define SDLAPP_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "graphics.h"
#include "audio.h"
#include "input.h"
#include "timer.h"

#include "mode_mainmenu.h"
#include "mode_gameplay.h"
#include "mode.h"

typedef struct {
	bool iconified;
	bool hasFocus;
	bool quit;
	Mode mode;
} SdlApp;

void sdlapp_run(void);

#endif
