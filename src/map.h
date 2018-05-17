#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include "color.h"
#include "collision.h"
#include "entity.h"

#define MAP_SIZE 128 
#define HALF_MAP_SIZE MAP_SIZE / 2
#define MAP_CELL_SIZE 100.0
#define MAP_MIN_LINE_SIZE 1.0

typedef struct {
	bool empty;
	ColorRGB primaryColor;
	ColorRGB outlineColor;
} MapCell;

void Map_initialize(void);
Collision Map_collide(const void *state, const PlaceableComponent *placeable, const Rectangle boundingBox,
	void (*resolve)(const void *state, const Collision collision), void *stateOther);
void Map_render();

#endif
