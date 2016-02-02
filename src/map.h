#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include "color.h"

#define MAP_SIZE 256

typedef struct {
	bool empty;
	ColorRGB primaryColor;
	ColorRGB outlineColor;
} MapCell;

void Map_initialize(void);

#endif