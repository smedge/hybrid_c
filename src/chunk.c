#include "chunk.h"
#include "map.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* --- Transform --- */

/* Origin-based discrete grid transform. Input coords are 0-based
 * within a WxH chunk. Output coords are 0-based in the transformed
 * bounding box. Uses (size-1-coord) for reflections instead of
 * negation, so even-sized chunks stay grid-aligned. */

void Chunk_transform_point(int lx, int ly, int w, int h,
                           ChunkTransform t, int *out_x, int *out_y)
{
	switch (t) {
	case TRANSFORM_IDENTITY:       *out_x = lx;         *out_y = ly;         break;
	case TRANSFORM_ROT90:          *out_x = h - 1 - ly; *out_y = lx;         break;
	case TRANSFORM_ROT180:         *out_x = w - 1 - lx; *out_y = h - 1 - ly; break;
	case TRANSFORM_ROT270:         *out_x = ly;          *out_y = w - 1 - lx; break;
	case TRANSFORM_MIRROR_H:       *out_x = w - 1 - lx; *out_y = ly;         break;
	case TRANSFORM_MIRROR_V:       *out_x = lx;          *out_y = h - 1 - ly; break;
	case TRANSFORM_MIRROR_H_ROT90: *out_x = ly;          *out_y = lx;         break;
	case TRANSFORM_MIRROR_V_ROT90: *out_x = h - 1 - ly; *out_y = w - 1 - lx; break;
	default:                       *out_x = lx;          *out_y = ly;         break;
	}
}

/* Spawn/intersection transform: spawns sit at grid intersections (corners
 * of cells), not at cell centers. Intersection indices range [0, size]
 * (one more than cell indices), so mirror uses (size - coord). */

void Chunk_transform_spawn(int lx, int ly, int w, int h,
                           ChunkTransform t, int *out_x, int *out_y)
{
	switch (t) {
	case TRANSFORM_IDENTITY:       *out_x = lx;     *out_y = ly;     break;
	case TRANSFORM_ROT90:          *out_x = h - ly; *out_y = lx;     break;
	case TRANSFORM_ROT180:         *out_x = w - lx; *out_y = h - ly; break;
	case TRANSFORM_ROT270:         *out_x = ly;     *out_y = w - lx; break;
	case TRANSFORM_MIRROR_H:       *out_x = w - lx; *out_y = ly;     break;
	case TRANSFORM_MIRROR_V:       *out_x = lx;     *out_y = h - ly; break;
	case TRANSFORM_MIRROR_H_ROT90: *out_x = ly;     *out_y = lx;     break;
	case TRANSFORM_MIRROR_V_ROT90: *out_x = h - ly; *out_y = w - lx; break;
	default:                       *out_x = lx;     *out_y = ly;     break;
	}
}

/* --- Loading --- */

bool Chunk_load(ChunkTemplate *out, const char *filepath)
{
	FILE *f = fopen(filepath, "r");
	if (!f) {
		printf("Chunk_load: failed to open '%s'\n", filepath);
		return false;
	}

	memset(out, 0, sizeof(*out));

	char line[512];
	while (fgets(line, sizeof(line), f)) {
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
		if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';
		if (len == 0 || line[0] == '#') continue;

		if (strncmp(line, "chunk ", 6) == 0) {
			strncpy(out->name, line + 6, sizeof(out->name) - 1);
		}
		else if (strncmp(line, "size ", 5) == 0) {
			sscanf(line + 5, "%d %d", &out->width, &out->height);
		}
		else if (strncmp(line, "wall ", 5) == 0) {
			if (out->wall_count >= CHUNK_MAX_CELLS) continue;
			ChunkCell *c = &out->walls[out->wall_count];
			c->drop_sub[0] = '\0';
			char drop[64] = "";
			int n = sscanf(line + 5, "%d %d %d %63s", &c->x, &c->y, &c->celltype_index, drop);
			if (n >= 3) {
				if (n >= 4 && strncmp(drop, "drop:", 5) == 0) {
					strncpy(c->drop_sub, drop + 5, sizeof(c->drop_sub) - 1);
					c->drop_sub[sizeof(c->drop_sub) - 1] = '\0';
				}
				out->wall_count++;
			}
		}
		else if (strncmp(line, "empty ", 6) == 0) {
			if (out->empty_count >= CHUNK_MAX_CELLS) continue;
			ChunkEmpty *e = &out->empties[out->empty_count];
			if (sscanf(line + 6, "%d %d", &e->x, &e->y) == 2)
				out->empty_count++;
		}
		else if (strncmp(line, "style ", 6) == 0) {
			if (strncmp(line + 6, "structured", 10) == 0)
				out->obstacle_style = OBSTACLE_STRUCTURED;
			else
				out->obstacle_style = OBSTACLE_ORGANIC;
		}
		else if (strncmp(line, "spawn ", 6) == 0) {
			if (out->spawn_count >= CHUNK_MAX_SPAWNS) continue;
			ChunkSpawn *s = &out->spawns[out->spawn_count];
			s->probability = 1.0f;
			int n = sscanf(line + 6, "%d %d %31s %f", &s->x, &s->y,
			               s->entity_type, &s->probability);
			if (n >= 3)
				out->spawn_count++;
		}
	}

	fclose(f);

	if (out->width <= 0 || out->height <= 0) {
		printf("Chunk_load: invalid size in '%s'\n", filepath);
		return false;
	}

	printf("Chunk_load: '%s' %dx%d (%d walls, %d empties, %d spawns)\n",
	       out->name, out->width, out->height,
	       out->wall_count, out->empty_count, out->spawn_count);
	return true;
}

/* --- Stamping --- */

void Chunk_stamp(const ChunkTemplate *chunk, Zone *zone,
                 int origin_x, int origin_y, ChunkTransform transform)
{
	/* First pass: mark the entire chunk footprint as chunk-stamped */
	for (int ly = 0; ly < chunk->height; ly++) {
		for (int lx = 0; lx < chunk->width; lx++) {
			int tx, ty;
			Chunk_transform_point(lx, ly, chunk->width, chunk->height,
			                      transform, &tx, &ty);
			int gx = origin_x + tx;
			int gy = origin_y + ty;
			if (gx >= 0 && gx < zone->size && gy >= 0 && gy < zone->size) {
				zone->cell_chunk_stamped[gx][gy] = true;
				/* Clear any existing cell in the footprint */
				if (!zone->cell_hand_placed[gx][gy])
					zone->cell_grid[gx][gy] = -1;
			}
		}
	}

	/* Second pass: stamp walls */
	for (int i = 0; i < chunk->wall_count; i++) {
		int tx, ty;
		Chunk_transform_point(chunk->walls[i].x, chunk->walls[i].y,
		                      chunk->width, chunk->height,
		                      transform, &tx, &ty);
		int gx = origin_x + tx;
		int gy = origin_y + ty;
		if (gx >= 0 && gx < zone->size && gy >= 0 && gy < zone->size) {
			if (!zone->cell_hand_placed[gx][gy]) {
				int ct_idx = chunk->walls[i].celltype_index;
				if (ct_idx >= 0 && ct_idx < zone->cell_type_count)
					zone->cell_grid[gx][gy] = ct_idx;
				if (chunk->walls[i].drop_sub[0] != '\0' &&
				    zone->destructible_count < ZONE_MAX_DESTRUCTIBLES) {
					ZoneDestructible *d = &zone->destructibles[zone->destructible_count++];
					d->grid_x = gx;
					d->grid_y = gy;
					strncpy(d->drop_sub, chunk->walls[i].drop_sub, sizeof(d->drop_sub) - 1);
					d->drop_sub[sizeof(d->drop_sub) - 1] = '\0';
				}
			}
		}
	}

	/* Third pass: stamp empties (ensure no cell) */
	for (int i = 0; i < chunk->empty_count; i++) {
		int tx, ty;
		Chunk_transform_point(chunk->empties[i].x, chunk->empties[i].y,
		                      chunk->width, chunk->height,
		                      transform, &tx, &ty);
		int gx = origin_x + tx;
		int gy = origin_y + ty;
		if (gx >= 0 && gx < zone->size && gy >= 0 && gy < zone->size) {
			if (!zone->cell_hand_placed[gx][gy])
				zone->cell_grid[gx][gy] = -1;
		}
	}
}

/* --- Export --- */

bool Chunk_export(int min_gx, int min_gy, int max_gx, int max_gy,
                  const char *filepath)
{
	const Zone *z = Zone_get();
	if (!z) return false;

	/* Normalize corners */
	if (min_gx > max_gx) { int t = min_gx; min_gx = max_gx; max_gx = t; }
	if (min_gy > max_gy) { int t = min_gy; min_gy = max_gy; max_gy = t; }

	/* Clamp to map bounds */
	if (min_gx < 0) min_gx = 0;
	if (min_gy < 0) min_gy = 0;
	if (max_gx >= MAP_SIZE) max_gx = MAP_SIZE - 1;
	if (max_gy >= MAP_SIZE) max_gy = MAP_SIZE - 1;

	int width = max_gx - min_gx + 1;
	int height = max_gy - min_gy + 1;
	if (width <= 0 || height <= 0) return false;

	FILE *f = fopen(filepath, "w");
	if (!f) {
		printf("Chunk_export: failed to open '%s' for writing\n", filepath);
		return false;
	}

	/* Extract chunk name from filepath */
	const char *name = filepath;
	const char *slash = strrchr(filepath, '/');
	if (slash) name = slash + 1;
	/* Strip .chunk extension for the name */
	char chunk_name[64];
	strncpy(chunk_name, name, sizeof(chunk_name) - 1);
	chunk_name[sizeof(chunk_name) - 1] = '\0';
	char *dot = strrchr(chunk_name, '.');
	if (dot) *dot = '\0';

	fprintf(f, "# Exported from %s at grid (%d,%d)-(%d,%d)\n",
	        z->name, min_gx, min_gy, max_gx, max_gy);
	fprintf(f, "chunk %s\n", chunk_name);
	fprintf(f, "size %d %d\n", width, height);
	fprintf(f, "\n");

	/* Export wall cells */
	int wall_count = 0;
	for (int gy = min_gy; gy <= max_gy; gy++) {
		for (int gx = min_gx; gx <= max_gx; gx++) {
			int idx = z->cell_grid[gx][gy];
			if (idx >= 0) {
				const ZoneDestructible *d = Zone_get_destructible(gx, gy);
				if (d)
					fprintf(f, "wall %d %d %d drop:%s\n",
					        gx - min_gx, gy - min_gy, idx, d->drop_sub);
				else
					fprintf(f, "wall %d %d %d\n",
					        gx - min_gx, gy - min_gy, idx);
				wall_count++;
			}
		}
	}

	/* Export entities within bounds */
	double world_min_x = (min_gx - HALF_MAP_SIZE) * MAP_CELL_SIZE - MAP_CELL_SIZE * 0.5;
	double world_min_y = (min_gy - HALF_MAP_SIZE) * MAP_CELL_SIZE - MAP_CELL_SIZE * 0.5;
	double world_max_x = (max_gx - HALF_MAP_SIZE + 1) * MAP_CELL_SIZE + MAP_CELL_SIZE * 0.5;
	double world_max_y = (max_gy - HALF_MAP_SIZE + 1) * MAP_CELL_SIZE + MAP_CELL_SIZE * 0.5;

	int spawn_count = 0;

	/* Savepoints */
	for (int i = 0; i < z->savepoint_count; i++) {
		int sgx = z->savepoints[i].grid_x;
		int sgy = z->savepoints[i].grid_y;
		if (sgx >= min_gx && sgx <= max_gx && sgy >= min_gy && sgy <= max_gy) {
			fprintf(f, "spawn %d %d savepoint 1.0\n",
			        sgx - min_gx, sgy - min_gy);
			spawn_count++;
		}
	}

	/* Data nodes */
	for (int i = 0; i < z->datanode_count; i++) {
		int dgx = z->datanodes[i].grid_x;
		int dgy = z->datanodes[i].grid_y;
		if (dgx >= min_gx && dgx <= max_gx && dgy >= min_gy && dgy <= max_gy) {
			fprintf(f, "spawn %d %d datanode 1.0\n",
			        dgx - min_gx, dgy - min_gy);
			spawn_count++;
		}
	}

	/* Portals */
	for (int i = 0; i < z->portal_count; i++) {
		int pgx = z->portals[i].grid_x;
		int pgy = z->portals[i].grid_y;
		if (pgx >= min_gx && pgx <= max_gx && pgy >= min_gy && pgy <= max_gy) {
			fprintf(f, "spawn %d %d portal 1.0\n",
			        pgx - min_gx, pgy - min_gy);
			spawn_count++;
		}
	}

	/* Enemy spawns */
	for (int i = 0; i < z->spawn_count; i++) {
		double wx = z->spawns[i].world_x;
		double wy = z->spawns[i].world_y;
		if (wx >= world_min_x && wx <= world_max_x &&
		    wy >= world_min_y && wy <= world_max_y) {
			/* Convert world pos to grid, then to local */
			int gx = (int)floor(wx / MAP_CELL_SIZE) + HALF_MAP_SIZE;
			int gy = (int)floor(wy / MAP_CELL_SIZE) + HALF_MAP_SIZE;
			fprintf(f, "spawn %d %d %s 1.0\n",
			        gx - min_gx, gy - min_gy, z->spawns[i].enemy_type);
			spawn_count++;
		}
	}

	fclose(f);

	printf("Chunk_export: '%s' %dx%d (%d walls, %d spawns)\n",
	       filepath, width, height, wall_count, spawn_count);
	return true;
}
