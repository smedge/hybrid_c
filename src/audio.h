#ifndef AUDIO_H
#define AUDIO_H

#include <SDL2/SDL_mixer.h>

// TODO need to incororate this struct into sample calls
typedef struct {
	Mix_Chunk *mix_chunk;
	const char *path;
} Sample;

void Audio_initialize(void);
void Audio_cleanup(void);
void Audio_loop_music(const char *path);
void Audio_play_music(const char *path);
void Audio_stop_music(void);
void Audio_load_sample(Mix_Chunk **sample, const char *path);
void Audio_unload_sample(Mix_Chunk **sample);
void Audio_play_sample(Mix_Chunk **sample);
int Audio_play_sample_on_channel(Mix_Chunk **sample, int channel);
int Audio_loop_sample_on_channel(Mix_Chunk **sample, int channel);
void Audio_fade_out_channel(int channel, int ms);
void Audio_boost_sample(Mix_Chunk *chunk, float gain);

#endif
