#include "destructible.h"

#include "zone.h"
#include "map.h"
#include "sub_mine.h"
#include "fragment.h"
#include "collision.h"
#include "progression.h"
#include "audio.h"

#include <string.h>

#define RESPAWN_TIME 10000

typedef struct {
	bool destroyed;
	int respawnTimer;
	int grid_x, grid_y;
	int cell_type_idx;
} DestructibleState;

static DestructibleState cells[ZONE_MAX_DESTRUCTIBLES];
static int count;
static Mix_Chunk *respawnSample = 0;

void Destructible_initialize(void)
{
	const Zone *z = Zone_get();
	count = z->destructible_count;

	for (int i = 0; i < count; i++) {
		cells[i].destroyed = false;
		cells[i].respawnTimer = 0;
		cells[i].grid_x = z->destructibles[i].grid_x;
		cells[i].grid_y = z->destructibles[i].grid_y;
		cells[i].cell_type_idx = z->cell_grid[cells[i].grid_x][cells[i].grid_y];
	}

	Audio_load_sample(&respawnSample, "resources/sounds/door.wav");
}

void Destructible_update(unsigned int ticks)
{
	const Zone *z = Zone_get();

	for (int i = 0; i < count; i++) {
		DestructibleState *ds = &cells[i];

		if (!ds->destroyed) {
			/* Build world-space AABB from grid coords */
			float wx = (float)(ds->grid_x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
			float wy = (float)(ds->grid_y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
			Rectangle cellRect = {wx, wy + MAP_CELL_SIZE, wx + MAP_CELL_SIZE, wy};

			if (Sub_Mine_check_hit(cellRect)) {
				Map_clear_cell(ds->grid_x, ds->grid_y);

				/* Only drop fragment if skill not yet unlocked */
				if (!Progression_is_unlocked(SUB_ID_BOOST)) {
					Position center = {wx + MAP_CELL_SIZE * 0.5,
					                   wy + MAP_CELL_SIZE * 0.5};
					Fragment_spawn(center, FRAG_TYPE_ELITE);
				}

				ds->destroyed = true;
				ds->respawnTimer = RESPAWN_TIME;
			}
		} else {
			ds->respawnTimer -= (int)ticks;
			if (ds->respawnTimer <= 0) {
				/* Restore cell from zone data */
				int idx = ds->cell_type_idx;
				if (idx >= 0) {
					const ZoneCellType *ct = &z->cell_types[idx];
					MapCell cell = {false, strcmp(ct->pattern, "circuit") == 0,
						ct->primaryColor, ct->outlineColor};
					Map_set_cell(ds->grid_x, ds->grid_y, &cell);
					Audio_play_sample(&respawnSample);
				}
				ds->destroyed = false;
			}
		}
	}
}
