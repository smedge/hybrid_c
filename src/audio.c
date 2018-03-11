#include "audio.h"

static Mix_Music *music = NULL;
static int timer_ticks;

// TODO audio files/resonrces SHOUD be managed here, not in the entities

void Audio_initialize(void)
{
	if( Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 4096 ) < 0 ) {
		printf( "SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError() );
		exit(-1);
	}
}

void Audio_update(const int ticks)
{
	timer_ticks = ticks;
}

void Audio_cleanup(void)
{
	Mix_Quit();
}

void Audio_loop_music(const char *path)
{
	music = Mix_LoadMUS(path);
	Mix_PlayMusic(music, -1);
}

void Audio_play_music_fade(const char *path, int playTime, 
						   int fadeTime, void (*on_end)())
{

}

void Audio_stop_music(void)
{
	Mix_FreeMusic(music);
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
	Mix_PlayChannel(-1, *sample, 0);
}
