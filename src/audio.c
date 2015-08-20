#include "audio.h"

static Mix_Music *music = NULL;

void audio_initialize()
{
	if( Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 2048 ) < 0 ) {
		printf( "SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError() );
		exit(-1);
	}
}

void audio_cleanup()
{
	Mix_Quit();
}

void audio_loop_music(const char *path)
{
	music = Mix_LoadMUS(path);
	Mix_PlayMusic(music, 2);
}

void audio_play_music_fade(const char *path, int playTime, 
						   int fadeTime, void (*on_end)())
{

}

void audio_stop_music()
{
	Mix_FreeMusic(music);
}