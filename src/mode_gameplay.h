#ifndef MODE_GAMEPLAY_H
#define MODE_GAMEPLAY_H

//#define GAMEPLAY_MUSIC_PATH "./resources/music/deadmau5_Avarita.mp3"
#define GAMEPLAY_MUSIC_PATH "./resources/music/deadmau5_ArcadiaRemasteredAgain.mp3"

#include "cursor.h"
#include "graphics.h"
#include "audio.h"
#include "hud.h"
#include "input.h"
#include "view.h"
#include "grid.h"
#include "ship.h"

void mode_gameplay_initialize(void);
void mode_gameplay_cleanup(void);
void mode_gameplay_update(const Input *input, const unsigned int ticks);
void mode_gameplay_render(void);

#endif