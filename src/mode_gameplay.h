#ifndef MODE_GAMEPLAY_H
#define MODE_GAMEPLAY_H

#include <stdbool.h>

#define GAMEPLAY_MUSIC_01_PATH "./resources/music/deadmau5_GG.mp3"
#define GAMEPLAY_MUSIC_02_PATH "./resources/music/deadmau5_Snowcone.mp3"
#define GAMEPLAY_MUSIC_03_PATH "./resources/music/deadmau5_ArcadiaRemasteredAgain.mp3"
#define GAMEPLAY_MUSIC_04_PATH "./resources/music/deadmau5_Avarita.mp3"
#define GAMEPLAY_MUSIC_05_PATH "./resources/music/deadmau5_Contact.mp3"
#define GAMEPLAY_MUSIC_06_PATH "./resources/music/deadmau5_Ameonna.mp3"
#define GAMEPLAY_MUSIC_07_PATH "./resources/music/deadmau5_Quezacotl.mp3"
#define REBIRTH_MUSIC_PATH "./resources/music/deadmau5_MemoryMan.mp3"

#include "cursor.h"
#include "graphics.h"
#include "audio.h"
#include "hud.h"
#include "input.h"
#include "view.h"
#include "grid.h"
#include "ship.h"
#include "mine.h"
#include "fragment.h"
#include "progression.h"
#include "portal.h"

void Mode_Gameplay_initialize(void);
void Mode_Gameplay_cleanup(void);
void Mode_Gameplay_update(const Input *input, const unsigned int ticks);
void Mode_Gameplay_render(void);
bool Mode_Gameplay_consumed_esc(void);

#endif
