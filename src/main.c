#include <stdio.h>

#include <SDL2/SDL.h>

#include "sdlapp.h"

int main(void) 
{
	printf("INFO: Starting application.\n");
	Sdlapp_run();
	printf("INFO: Application exiting normally.\n");
	return 0;
}