#ifndef AUDIO_H
#define AUDIO_H

#include <SDL2/SDL_mixer.h>

void audio_initialize();
void audio_cleanup();
void audio_loop_music(const char *path);
void audio_stop_music();

#endif