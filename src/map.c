#include "map.h"

static MapCell *map[MAP_SIZE][MAP_SIZE];

static MapCell emptyCell = {true, {0,0,0,0}, {0,0,0,0}};
static MapCell cell001 = {false, {16,0,16,255}, {128,0,128,255}};

void Map_initialize(void)
{
	unsigned int i = 0, j = 0;
	for(i = 0; i < MAP_SIZE; i++) {
		for(i = 0; i < MAP_SIZE; i++) {
			map[i][j] = &emptyCell;
		}
	}

	map[127][127] = &cell001;
}