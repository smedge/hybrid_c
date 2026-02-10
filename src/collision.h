#ifndef COLLISION_H
#define COLLISION_H

#include <stdbool.h>
#include "position.h"

typedef struct {
	double aX;
	double aY;
	double bX;
	double bY;
} Rectangle;

typedef struct {
	bool collisionDetected;
	bool solid;
} Collision;

bool Collision_aabb_test(const Rectangle rect1, 
						 const Rectangle rect2);
bool Collision_point_test(const double x, 
						  const double y, 
						  const Rectangle rect);

Rectangle Collision_transform_bounding_box(const Position position, const Rectangle boundingBox);

bool Collision_line_aabb_test(double x0, double y0, double x1, double y1,
							 const Rectangle rect, double *t_out);

#endif
