#include <stdio.h>

#include <SDL2/SDL.h>

#include "sdlapp.h"

int main() 
{
	sdlapp_initialize();
	sdlapp_loop();
	sdlapp_cleanup();
	return 0;
}