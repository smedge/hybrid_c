#include <stdio.h>

#include <SDL2/SDL.h>

#include "sdlapp.h"

int main(void) 
{
	printf("Starting application.\n");
	Sdlapp_run();
	printf("Application exiting normally.\n");
	
	return 0;
}