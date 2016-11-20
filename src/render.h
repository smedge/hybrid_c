#ifndef RENDER_H
#define RENDER_H

#include "position.h"
#include "collision.h"

extern void Render_point(const Position *position, const double colorR, 
	const double colorG, const double colorB, const double colorA);

extern void Render_line();

extern void Render_triangle(const Position *position, const double heading, 
	const double colorR, const double colorG, const double colorB, 
	const double colorA);

extern void Render_quad();

extern void Render_convex_poly();

extern void Render_bounding_box(const Position *position, 
	const Rectangle *rectangle);

#endif