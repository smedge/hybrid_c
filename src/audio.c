#include "audio.h"

static Mix_Music *music = NULL;

// TODO audio files/resonrces SHOUD be managed here, not in the entities

void Audio_initialize(void)
{
	if( Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 4096 ) < 0 ) {
		printf( "SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError() );
		exit(-1);
	}
	Mix_AllocateChannels(32);
}

void Audio_cleanup(void)
{
	Mix_Quit();
}

void Audio_loop_music(const char *path)
{
	if (music) Mix_FreeMusic(music);
	music = Mix_LoadMUS(path);
	if (!music) return;
	Mix_PlayMusic(music, -1);
}

void Audio_play_music(const char *path)
{
	if (music) Mix_FreeMusic(music);
	music = Mix_LoadMUS(path);
	if (!music) return;
	Mix_PlayMusic(music, 0);
}

void Audio_stop_music(void)
{
	if (music) {
		Mix_FreeMusic(music);
		music = NULL;
	}
}

void Audio_load_sample(Mix_Chunk **sample, char *path)
{
	if (!*sample) {
		*sample = Mix_LoadWAV(path);
		if (!*sample) {
			printf("FATAL ERROR: error loading sound: %s\n", path);
			exit(-1);
		}
	}
}

void Audio_unload_sample(Mix_Chunk **sample)
{
	Mix_FreeChunk(*sample);
	*sample = 0;
}

void Audio_play_sample(Mix_Chunk **sample)
{
	if (Mix_PlayChannel(-1, *sample, 0) == -1)
		printf("WARNING: out of audio channels: %s\n", Mix_GetError());
}

int Audio_play_sample_on_channel(Mix_Chunk **sample, int channel)
{
	return Mix_PlayChannel(channel, *sample, 0);
}

int Audio_loop_sample_on_channel(Mix_Chunk **sample, int channel)
{
	return Mix_PlayChannel(channel, *sample, -1);
}

void Audio_fade_out_channel(int channel, int ms)
{
	Mix_FadeOutChannel(channel, ms);
}
