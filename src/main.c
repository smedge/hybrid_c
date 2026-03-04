#include <stdio.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "sdlapp.h"

int main(void)
{
	// Set working directory to the executable's location so resource
	// paths resolve correctly when launched from Finder (double-click).
	char *basePath = SDL_GetBasePath();
	if (basePath) {
		chdir(basePath);
		SDL_free(basePath);
	}

	printf("Starting application.\n");
	Sdlapp_run();
	printf("Application exiting normally.\n");
	
	return 0;
}