#ifndef RENDER_H
#define RENDER_H

#include <SDL2/SDL_opengl.h>

#include "position.h"

void render_triangle(Position *position, double heading, 
	double colorR, double colorG, double colorB, double colorA);

#endif