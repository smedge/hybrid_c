#ifndef MODE_MAINMENU_H
#define MODE_MAINMENU_H

#define SQUARE_FONT_PATH "./resources/fonts/square_sans_serif_7.ttf"

#include <FTGL/ftgl.h>

#include "cursor.h"
#include "collision.h"
#include "graphics.h"
#include "input.h"
#include "sdlapp.h"

#include "mode_t.h"

void mode_mainmenu_update(const Input *input, const unsigned int ticks, 
	bool *quit, Mode *mode);
void mode_mainmenu_render();

#endif