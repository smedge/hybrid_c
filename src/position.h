#ifndef POSITION_H
#define POSITION_H

typedef struct {
	double x;
	double y;
} Position;

double Position_get_heading(const Position p1, const Position p2);

#endif