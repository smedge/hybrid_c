#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include "color.h"

#define MAP_SIZE 128
#define MAP_CELL_SIZE 100.0

typedef struct {
	bool empty;
	ColorRGB primaryColor;
	ColorRGB outlineColor;
} MapCell;

void Map_initialize(void);
void Map_render();

#endif