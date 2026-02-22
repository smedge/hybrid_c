#ifndef CHUNK_H
#define CHUNK_H

#include <stdbool.h>
#include "zone.h"

#define CHUNK_MAX_CELLS  4096
#define CHUNK_MAX_SPAWNS 32

typedef enum {
	OBSTACLE_ORGANIC,
	OBSTACLE_STRUCTURED
} ObstacleStyle;

typedef enum {
	TRANSFORM_IDENTITY,
	TRANSFORM_ROT90,
	TRANSFORM_ROT180,
	TRANSFORM_ROT270,
	TRANSFORM_MIRROR_H,
	TRANSFORM_MIRROR_V,
	TRANSFORM_MIRROR_H_ROT90,
	TRANSFORM_MIRROR_V_ROT90,
	TRANSFORM_COUNT
} ChunkTransform;

typedef struct {
	int x, y;
	int celltype_index;
	char drop_sub[32];
} ChunkCell;

typedef struct {
	int x, y;
} ChunkEmpty;

typedef struct {
	int x, y;
	char entity_type[32];
	float probability;
} ChunkSpawn;

typedef struct {
	char name[64];
	int width, height;

	ChunkCell walls[CHUNK_MAX_CELLS];
	int wall_count;

	ChunkEmpty empties[CHUNK_MAX_CELLS];
	int empty_count;

	ChunkSpawn spawns[CHUNK_MAX_SPAWNS];
	int spawn_count;

	ObstacleStyle obstacle_style;
} ChunkTemplate;

/* Load a chunk template from file. Returns true on success. */
bool Chunk_load(ChunkTemplate *out, const char *filepath);

/* Stamp a chunk onto the zone at origin (origin_x, origin_y) with transform.
 * Origin is the top-left corner in world grid coords.
 * Marks stamped cells in zone->cell_chunk_stamped so terrain gen skips them. */
void Chunk_stamp(const ChunkTemplate *chunk, Zone *zone,
                 int origin_x, int origin_y, ChunkTransform transform);

/* Export a rectangular region from the current zone to a .chunk file.
 * Scans cell grid + entity pools for content within bounds.
 * Returns true on success. */
bool Chunk_export(int min_gx, int min_gy, int max_gx, int max_gy,
                  const char *filepath);

/* Apply a transform to a 0-based local CELL coordinate within a WxH chunk.
 * Uses (size-1-coord) for cell indices that range [0, size-1]. */
void Chunk_transform_point(int lx, int ly, int w, int h,
                           ChunkTransform t, int *out_x, int *out_y);

/* Apply a transform to a local INTERSECTION coordinate (spawn position).
 * Uses (size-coord) because intersections range [0, size] and portals/
 * savepoints sit at grid corners, not cell centers. */
void Chunk_transform_spawn(int lx, int ly, int w, int h,
                           ChunkTransform t, int *out_x, int *out_y);

#endif
