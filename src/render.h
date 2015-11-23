#ifndef RENDER_H
#define RENDER_H

#include "position.h"

extern void Render_point();

extern void Render_line();

extern void Render_triangle(const Position *position, const double heading, 
	const double colorR, const double colorG, const double colorB, 
	const double colorA);

#endif