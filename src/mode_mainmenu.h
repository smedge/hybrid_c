#ifndef MODE_MAINMENU_H
#define MODE_MAINMENU_H

#define TITLE_FONT_PATH "./resources/fonts/square_sans_serif_7.ttf"
#define MENU_MUSIC_PATH "./resources/music/Xilent-Infinity.mp3"

#define TITLE_TEXT "Hybrid"
#define NEW_BUTTON_TEXT "New"
#define LOAD_BUTTON_TEXT "Load"
#define EXIT_BUTTON_TEXT "Exit"

#include <stdlib.h>
#include <FTGL/ftgl.h>

#include "cursor.h"
#include "graphics.h"
#include "input.h"
#include "imgui.h"
#include "mode.h"
#include "graphics.h"
#include "audio.h"

void mode_mainmenu_initialize(
	void (*quit)(),
	void (*gameplay_mode)());
void mode_mainmenu_cleanup(void);
void mode_mainmenu_update(
	const Input *input, 
	const unsigned int ticks);
void mode_mainmenu_render(void);

#endif