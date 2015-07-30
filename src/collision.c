#include "collision.h"

bool collision_aabb_test(const Rectangle rect1, const Rectangle rect2) 
{
	return false;
}

bool collision_point_test(const double x, const double y, const Rectangle rect)
{	
	if (x >= rect.aX && x <= rect.bX &&
		y >= rect.aY && y <= rect.bY)
		return true;
	else
		return false;
}