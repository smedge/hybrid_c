#ifndef AUDIO_H
#define AUDIO_H

#include <SDL2/SDL_mixer.h>

void Audio_initialize(void);
void Audio_cleanup(void);
void Audio_loop_music(const char *path);
void Audio_play_music(const char *path, int playTime, 
					  int fadeTime, void (*on_end)());
void Audio_stop_music(void);
void Audio_load_sample(Mix_Chunk **sample, char *path);
void Audio_unload_sample(Mix_Chunk **sample);
void Audio_play_sample(Mix_Chunk **sample);

#endif
