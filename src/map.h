#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include "color.h"

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
bool Map_collide();
void Map_render();

#endif