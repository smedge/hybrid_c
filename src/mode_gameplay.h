#ifndef MODE_GAMEPLAY_H
#define MODE_GAMEPLAY_H

#define GAMEPLAY_MUSIC_PATH "./resources/music/deadmau5_Avarita.mp3"

#include "cursor.h"
#include "graphics.h"
#include "audio.h"
#include "hud.h"
#include "input.h"
#include "view.h"

void mode_gameplay_initialize();
void mode_gameplay_cleanup();
void mode_gameplay_update();
void mode_gameplay_render();

#endif