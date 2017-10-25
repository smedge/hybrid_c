#ifndef RENDER_H
#define RENDER_H

#include "position.h"
#include "collision.h"
#include "color.h"
#include "view.h"
#include "graphics.h"

void Render_point(const Position *position, const float size, 
	const ColorFloat *color);

void Render_line();

void Render_triangle(const Position *position, const double heading, 
	const ColorFloat *color);

void Render_quad(const Position *position, const double rotation, 
	const Rectangle rectangle, const ColorFloat *color);

void Render_convex_poly();

void Render_bounding_box(const Position *position, 
	const Rectangle *rectangle);

void Render_grid_lines(const double gridSize, const double bigGridSize,
	const double minLineSize, const double minBigLineSize);
#endif
