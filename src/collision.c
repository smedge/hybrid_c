#include "collision.h"

bool Collision_aabb_test(const Rectangle rect1,
						 const Rectangle rect2)
{
	double minX1 = rect1.aX < rect1.bX ? rect1.aX : rect1.bX;
	double maxX1 = rect1.aX > rect1.bX ? rect1.aX : rect1.bX;
	double minX2 = rect2.aX < rect2.bX ? rect2.aX : rect2.bX;
	double maxX2 = rect2.aX > rect2.bX ? rect2.aX : rect2.bX;
	double minY1 = rect1.aY < rect1.bY ? rect1.aY : rect1.bY;
	double maxY1 = rect1.aY > rect1.bY ? rect1.aY : rect1.bY;
	double minY2 = rect2.aY < rect2.bY ? rect2.aY : rect2.bY;
	double maxY2 = rect2.aY > rect2.bY ? rect2.aY : rect2.bY;

	if (maxX1 < minX2 || minX1 > maxX2 || maxY1 < minY2 || minY1 > maxY2)
		return false;
	return true;
}

bool Collision_point_test(const double x,
						  const double y,
						  const Rectangle rect)
{
	double minY = rect.aY < rect.bY ? rect.aY : rect.bY;
	double maxY = rect.aY > rect.bY ? rect.aY : rect.bY;
	if (x >= rect.aX && x <= rect.bX &&
		y >= minY && y <= maxY)
		return true;
	else
		return false;
}

bool Collision_line_aabb_test(double x0, double y0, double x1, double y1,
							 const Rectangle rect, double *t_out)
{
	double minX = rect.aX < rect.bX ? rect.aX : rect.bX;
	double maxX = rect.aX > rect.bX ? rect.aX : rect.bX;
	double minY = rect.aY < rect.bY ? rect.aY : rect.bY;
	double maxY = rect.aY > rect.bY ? rect.aY : rect.bY;

	double tmin = 0.0;
	double tmax = 1.0;

	double dx = x1 - x0;
	if (dx != 0.0) {
		double t1 = (minX - x0) / dx;
		double t2 = (maxX - x0) / dx;
		if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
		if (t1 > tmin) tmin = t1;
		if (t2 < tmax) tmax = t2;
		if (tmin > tmax) return false;
	} else {
		if (x0 < minX || x0 > maxX) return false;
	}

	double dy = y1 - y0;
	if (dy != 0.0) {
		double t1 = (minY - y0) / dy;
		double t2 = (maxY - y0) / dy;
		if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; }
		if (t1 > tmin) tmin = t1;
		if (t2 < tmax) tmax = t2;
		if (tmin > tmax) return false;
	} else {
		if (y0 < minY || y0 > maxY) return false;
	}

	if (t_out)
		*t_out = tmin;
	return true;
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
