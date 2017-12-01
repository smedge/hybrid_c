#include "position.h"

#include "math.h"

#define M_PI (3.14159265358979323846)

double Position_get_heading(const Position p1, const Position p2)
{
	double theta_radians = atan2(p1.x - p2.x, p1.y - p2.y);
	double theta_degrees = (theta_radians + M_PI) * 360.0 / (2.0 * M_PI);
	return theta_degrees;
}