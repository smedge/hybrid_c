#include "collision.h"

bool Collision_aabb_test(const Rectangle rect1, 
						 const Rectangle rect2) 
{
	if (rect1.bX < rect2.aX || rect1.aX > rect2.bX ||
		rect1.bY > rect2.aY || rect1.aY < rect2.bY)
		return false;
    else 
    	return true;
}

bool Collision_point_test(const double x, 
						  const double y, 
						  const Rectangle rect)
{	
	if (x >= rect.aX && x <= rect.bX &&
		y >= rect.aY && y <= rect.bY)
		return true;
	else
		return false;
}

Rectangle Collision_transform_bounding_box(const Position position, const Rectangle boundingBox)
{
	Rectangle transformedBoundingBox = {
		boundingBox.aX + position.x,
		boundingBox.aY + position.y,
		boundingBox.bX + position.x,
		boundingBox.bY + position.y,
	};

	return transformedBoundingBox;
}
