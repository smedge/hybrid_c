#ifndef RENDER_H
#define RENDER_H

#include "position.h"

extern void Render_point(const Position *position, const double colorR, 
	const double colorG, const double colorB, const double colorA);

extern void Render_line();

extern void Render_triangle(const Position *position, const double heading, 
	const double colorR, const double colorG, const double colorB, 
	const double colorA);

extern void Render_quad();

extern void Render_convex_poly();

#endif