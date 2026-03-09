#ifndef AUDIO_H
#define AUDIO_H

#include <SDL2/SDL_mixer.h>
#include "position.h"

#define VOICE_CHANNEL 2

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
void Audio_play_sample_at(Mix_Chunk **sample, Position pos);
int Audio_play_sample_on_channel(Mix_Chunk **sample, int channel);
int Audio_loop_sample_on_channel(Mix_Chunk **sample, int channel);
void Audio_fade_out_channel(int channel, int ms);
void Audio_boost_sample(Mix_Chunk *chunk, float gain);

void Audio_set_listener_position(float x, float y);

void  Audio_set_master_music(float vol);
float Audio_get_master_music(void);
void  Audio_set_master_sfx(float vol);
float Audio_get_master_sfx(void);
void  Audio_set_master_voice(float vol);
float Audio_get_master_voice(void);

#endif
