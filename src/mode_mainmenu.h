#ifndef MODE_MAINMENU_H
#define MODE_MAINMENU_H

#include <FTGL/ftgl.h>

#include "cursor.h"
#include "graphics.h"
#include "input.h"

void mode_mainmenu_update(const Input *input, const unsigned int ticks, bool *quit);
void mode_mainmenu_render();

#endif