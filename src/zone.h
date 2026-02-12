#ifndef ZONE_H
#define ZONE_H

#include "map.h"
#include "color.h"

#define ZONE_MAX_CELL_TYPES 32
#define ZONE_MAX_SPAWNS 256
#define ZONE_MAX_UNDO 512

typedef struct {
	char id[32];
	ColorRGB primaryColor;
	ColorRGB outlineColor;
	char pattern[16];
} ZoneCellType;

typedef struct {
	char enemy_type[16];
	double world_x, world_y;
} ZoneSpawn;

typedef struct {
	char name[64];
	int size;
	char filepath[256];

	ZoneCellType cell_types[ZONE_MAX_CELL_TYPES];
	int cell_type_count;

	int cell_grid[MAP_SIZE][MAP_SIZE];

	ZoneSpawn spawns[ZONE_MAX_SPAWNS];
	int spawn_count;
} Zone;

void Zone_load(const char *path);
void Zone_unload(void);
void Zone_save(void);
const Zone *Zone_get(void);

/* Editing API (for God Mode) */
void Zone_place_cell(int grid_x, int grid_y, const char *type_id);
void Zone_remove_cell(int grid_x, int grid_y);
void Zone_place_spawn(const char *enemy_type, double world_x, double world_y);
void Zone_remove_spawn(int index);
void Zone_undo(void);

#endif
