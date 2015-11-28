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

void Mode_Mainmenu_initialize(
	void (*quit)(void),
	void (*gameplay_mode)(void));
void Mode_Mainmenu_cleanup(void);
void Mode_Mainmenu_update(
	const Input *input, 
	const unsigned int ticks);
void Mode_Mainmenu_render(void);

#endif