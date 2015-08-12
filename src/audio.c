#include "audio.h"

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