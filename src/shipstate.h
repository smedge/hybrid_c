#ifndef SHIP_STATE_H
#define SHIP_STATE_H

#include <stdbool.h>

typedef struct {
	bool destroyed;
	unsigned int ticksDestroyed;
} ShipState;

#endif