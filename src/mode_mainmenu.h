#ifndef MODE_MAINMENU_H
#define MODE_MAINMENU_H

#define SQUARE_FONT_PATH "./resources/fonts/square_sans_serif_7.ttf"
#include <stdlib.h>

#include <FTGL/ftgl.h>

#include "cursor.h"
#include "graphics.h"
#include "input.h"
#include "imgui.h"

#include "mode_t.h"

void mode_mainmenu_initialize();
void mode_mainmenu_cleanup();
void mode_mainmenu_update(const Input *input, 
						  const unsigned int ticks, 
						  void (*quit)(), 
						  void (*mode)());
void mode_mainmenu_render();

#endif