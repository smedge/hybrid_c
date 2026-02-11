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

void Render_line_segment(float x0, float y0, float x1, float y1,
	float r, float g, float b, float a);

void Render_quad_absolute(float ax, float ay, float bx, float by,
	float r, float g, float b, float a);

void Render_thick_line(float x0, float y0, float x1, float y1,
	float thickness, float r, float g, float b, float a);

void Render_filled_circle(float cx, float cy, float radius, int segments,
	float r, float g, float b, float a);

void Render_flush(const Mat4 *projection, const Mat4 *view);
void Render_flush_additive(const Mat4 *projection, const Mat4 *view);

#endif
