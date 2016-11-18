#ifndef COLLISION_H
#define COLLISION_H

#include <stdbool.h>

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

#endif