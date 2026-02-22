#include "spatial_grid.h"
#include "map.h"
#include "hunter.h"
#include "seeker.h"
#include "mine.h"
#include "defender.h"
#include "stalker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
	EntityRef entities[BUCKET_CAPACITY];
	int count;
} Bucket;

static Bucket grid[GRID_MAX][GRID_MAX];
static int player_bx = 0;
static int player_by = 0;

/* Check if entity index is valid (within module's allocated range).
   Dead enemies are NOT stale — they stay registered for respawn.
   Stale = index beyond module's highestUsedIndex (cleanup without grid clear). */
static bool entity_is_valid(EntityRef ref)
{
	switch (ref.type) {
	case ENTITY_HUNTER:   return ref.index < Hunter_get_count();
	case ENTITY_SEEKER:   return ref.index < Seeker_get_count();
	case ENTITY_MINE:     return ref.index < Mine_get_count();
	case ENTITY_DEFENDER: return ref.index < Defender_get_count();
	case ENTITY_STALKER:  return ref.index < Stalker_get_count();
	}
	return false;
}

static void world_to_bucket(double world_x, double world_y, int *bx, int *by)
{
	/* Convert world coords to grid coords (0-indexed), then to bucket */
	int grid_x = (int)floor(world_x / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	int grid_y = (int)floor(world_y / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	*bx = grid_x / BUCKET_SIZE;
	*by = grid_y / BUCKET_SIZE;

	/* Clamp to valid range */
	if (*bx < 0) *bx = 0;
	if (*bx >= GRID_MAX) *bx = GRID_MAX - 1;
	if (*by < 0) *by = 0;
	if (*by >= GRID_MAX) *by = GRID_MAX - 1;
}

void SpatialGrid_init(void)
{
	memset(grid, 0, sizeof(grid));
	player_bx = 0;
	player_by = 0;
}

void SpatialGrid_clear(void)
{
	for (int y = 0; y < GRID_MAX; y++)
		for (int x = 0; x < GRID_MAX; x++)
			grid[y][x].count = 0;
}

void SpatialGrid_add(EntityRef ref, double world_x, double world_y)
{
	int bx, by;
	world_to_bucket(world_x, world_y, &bx, &by);

	Bucket *b = &grid[by][bx];
	if (b->count >= BUCKET_CAPACITY) {
		fprintf(stderr, "SPATIAL: bucket [%d,%d] full (%d), cannot add type=%d idx=%d\n",
			bx, by, BUCKET_CAPACITY, ref.type, ref.index);
		return;
	}

	b->entities[b->count] = ref;
	b->count++;
}

void SpatialGrid_remove(EntityRef ref, double world_x, double world_y)
{
	int bx, by;
	world_to_bucket(world_x, world_y, &bx, &by);

	Bucket *b = &grid[by][bx];
	for (int i = 0; i < b->count; i++) {
		if (b->entities[i].type == ref.type && b->entities[i].index == ref.index) {
			/* Swap with last */
			b->entities[i] = b->entities[b->count - 1];
			b->count--;
			return;
		}
	}

	/* Not found in expected bucket — scan all buckets as fallback */
	for (int gy = 0; gy < GRID_MAX; gy++) {
		for (int gx = 0; gx < GRID_MAX; gx++) {
			Bucket *fb = &grid[gy][gx];
			for (int i = 0; i < fb->count; i++) {
				if (fb->entities[i].type == ref.type && fb->entities[i].index == ref.index) {
					fb->entities[i] = fb->entities[fb->count - 1];
					fb->count--;
					fprintf(stderr, "SPATIAL: removed type=%d idx=%d from [%d,%d] (expected [%d,%d])\n",
						ref.type, ref.index, gx, gy, bx, by);
					return;
				}
			}
		}
	}

	fprintf(stderr, "SPATIAL: failed to remove type=%d idx=%d (not found anywhere)\n",
		ref.type, ref.index);
}

void SpatialGrid_update(EntityRef ref, double old_x, double old_y, double new_x, double new_y)
{
	int old_bx, old_by, new_bx, new_by;
	world_to_bucket(old_x, old_y, &old_bx, &old_by);
	world_to_bucket(new_x, new_y, &new_bx, &new_by);

	if (old_bx == new_bx && old_by == new_by)
		return;

	SpatialGrid_remove(ref, old_x, old_y);
	SpatialGrid_add(ref, new_x, new_y);
}

bool SpatialGrid_is_active(double world_x, double world_y)
{
	int bx, by;
	world_to_bucket(world_x, world_y, &bx, &by);
	return abs(bx - player_bx) <= 1 && abs(by - player_by) <= 1;
}

void SpatialGrid_set_player_bucket(double world_x, double world_y)
{
	world_to_bucket(world_x, world_y, &player_bx, &player_by);
}

void SpatialGrid_validate(void)
{
	int stale = 0;
	for (int gy = 0; gy < GRID_MAX; gy++) {
		for (int gx = 0; gx < GRID_MAX; gx++) {
			Bucket *b = &grid[gy][gx];
			for (int i = b->count - 1; i >= 0; i--) {
				if (!entity_is_valid(b->entities[i])) {
					/* Stale ref — remove it */
					fprintf(stderr, "SPATIAL: stale ref type=%d idx=%d in [%d,%d], removing\n",
						b->entities[i].type, b->entities[i].index, gx, gy);
					b->entities[i] = b->entities[b->count - 1];
					b->count--;
					stale++;
				}
			}
		}
	}

	if (stale > 0)
		fprintf(stderr, "SPATIAL: validate removed %d stale refs\n", stale);
}

int SpatialGrid_query_neighborhood(int bucket_x, int bucket_y, EntityRef *out, int out_capacity)
{
	int count = 0;

	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			int bx = bucket_x + dx;
			int by = bucket_y + dy;
			if (bx < 0 || bx >= GRID_MAX || by < 0 || by >= GRID_MAX)
				continue;

			Bucket *b = &grid[by][bx];
			for (int i = 0; i < b->count && count < out_capacity; i++) {
				out[count++] = b->entities[i];
			}
		}
	}

	return count;
}
