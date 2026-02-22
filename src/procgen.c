#include "procgen.h"
#include "chunk.h"
#include "noise.h"
#include "prng.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static uint32_t master_seed = 0;

/* --- Debug data (accessible for godmode rendering) --- */

#define MAX_HOTSPOTS 32

typedef struct {
	int x, y;
	bool used;
} Hotspot;

typedef struct {
	const LandmarkDef *def;
	int grid_x, grid_y;       /* hotspot center */
	int origin_x, origin_y;   /* chunk stamp origin (top-left) */
	int stamp_w, stamp_h;     /* chunk dims after transform */
} PlacedLandmark;

static Hotspot         debug_hotspots[MAX_HOTSPOTS];
static int             debug_hotspot_count = 0;
static PlacedLandmark  debug_landmarks[ZONE_MAX_LANDMARKS];
static int             debug_landmark_count = 0;
static uint32_t        debug_zone_seed = 0;

/* --- Public accessors for debug data --- */

int Procgen_get_hotspot_count(void) { return debug_hotspot_count; }

void Procgen_get_hotspot(int i, int *x, int *y, bool *used)
{
	if (i < 0 || i >= debug_hotspot_count) return;
	*x = debug_hotspots[i].x;
	*y = debug_hotspots[i].y;
	*used = debug_hotspots[i].used;
}

int Procgen_get_landmark_count(void) { return debug_landmark_count; }

void Procgen_get_landmark(int i, int *x, int *y, const char **type,
                          float *radius, int *inf_type)
{
	if (i < 0 || i >= debug_landmark_count) return;
	*x = debug_landmarks[i].grid_x;
	*y = debug_landmarks[i].grid_y;
	*type = debug_landmarks[i].def->type;
	*radius = debug_landmarks[i].def->influence.radius;
	*inf_type = (int)debug_landmarks[i].def->influence.type;
}

uint32_t Procgen_get_zone_seed(void) { return debug_zone_seed; }

/* --- Seed management --- */

void Procgen_set_master_seed(uint32_t seed)
{
	master_seed = seed;
}

uint32_t Procgen_get_master_seed(void)
{
	return master_seed;
}

uint32_t Procgen_derive_zone_seed(uint32_t ms, const char *zone_id)
{
	/* FNV-1a hash of zone identifier */
	uint32_t hash = 2166136261u;
	for (const char *p = zone_id; *p; p++) {
		hash ^= (uint8_t)*p;
		hash *= 16777619u;
	}
	/* Combine with master seed (boost hash_combine style) */
	return ms ^ (hash + 0x9E3779B9u + (ms << 6) + (ms >> 2));
}

#define SECTION_SIZE 16

/* --- Hotspot generation --- */

/* Hotspots land on major grid intersections (multiples of SECTION_SIZE).
 * Enumerate valid intersections, shuffle, pick with separation. */

#define MAX_CANDIDATES 4096

typedef struct { int x, y; } GridCenter;

static int generate_hotspots(Zone *zone, Prng *rng, Hotspot *out)
{
	int margin = zone->hotspot_edge_margin;
	int min_sep = zone->hotspot_min_separation;
	int target = zone->hotspot_count;

	/* Enumerate all 16x16 section centers within valid area */
	GridCenter candidates[MAX_CANDIDATES];
	int candidate_count = 0;

	int sections = zone->size / SECTION_SIZE;
	for (int sy = 0; sy <= sections; sy++) {
		for (int sx = 0; sx <= sections; sx++) {
			int cx = sx * SECTION_SIZE;
			int cy = sy * SECTION_SIZE;

			/* Edge margin check */
			if (cx < margin || cx >= zone->size - margin) continue;
			if (cy < margin || cy >= zone->size - margin) continue;

			if (candidate_count < MAX_CANDIDATES) {
				candidates[candidate_count].x = cx;
				candidates[candidate_count].y = cy;
				candidate_count++;
			}
		}
	}

	/* Fisher-Yates shuffle */
	for (int i = candidate_count - 1; i > 0; i--) {
		int j = Prng_range(rng, 0, i);
		GridCenter tmp = candidates[i];
		candidates[i] = candidates[j];
		candidates[j] = tmp;
	}

	/* Pick from shuffled candidates with separation check */
	int count = 0;
	for (int ci = 0; ci < candidate_count && count < target; ci++) {
		int x = candidates[ci].x;
		int y = candidates[ci].y;

		bool too_close = false;
		for (int i = 0; i < count; i++) {
			double dx = x - out[i].x;
			double dy = y - out[i].y;
			if (sqrt(dx * dx + dy * dy) < min_sep) {
				too_close = true;
				break;
			}
		}
		if (too_close) continue;

		out[count].x = x;
		out[count].y = y;
		out[count].used = false;
		count++;
	}

	if (count < target)
		printf("Procgen: warning — only placed %d of %d hotspots\n", count, target);

	return count;
}

/* --- Landmark resolution --- */

static int resolve_landmarks(Zone *zone, Prng *rng,
                             Hotspot *hotspots, int hotspot_count,
                             PlacedLandmark *out)
{
	if (zone->landmark_count == 0) return 0;

	/* Sort landmarks by priority (simple insertion sort — max 16 elements) */
	int order[ZONE_MAX_LANDMARKS];
	for (int i = 0; i < zone->landmark_count; i++) order[i] = i;
	for (int i = 1; i < zone->landmark_count; i++) {
		int key = order[i];
		int j = i - 1;
		while (j >= 0 && zone->landmarks[order[j]].priority > zone->landmarks[key].priority) {
			order[j + 1] = order[j];
			j--;
		}
		order[j + 1] = key;
	}

	int placed_count = 0;

	for (int oi = 0; oi < zone->landmark_count; oi++) {
		LandmarkDef *def = &zone->landmarks[order[oi]];

		/* Build candidate list */
		int candidates[MAX_HOTSPOTS];
		int candidate_count = 0;

		for (int h = 0; h < hotspot_count; h++) {
			if (hotspots[h].used) continue;

			bool far_enough = true;
			for (int p = 0; p < placed_count; p++) {
				double dx = hotspots[h].x - out[p].grid_x;
				double dy = hotspots[h].y - out[p].grid_y;
				if (sqrt(dx * dx + dy * dy) < zone->landmark_min_separation) {
					far_enough = false;
					break;
				}
			}
			if (far_enough)
				candidates[candidate_count++] = h;
		}

		/* Fallback: any unused hotspot */
		if (candidate_count == 0) {
			for (int h = 0; h < hotspot_count; h++) {
				if (!hotspots[h].used)
					candidates[candidate_count++] = h;
			}
		}

		if (candidate_count == 0) {
			printf("Procgen: warning — no hotspot for landmark '%s'\n", def->type);
			continue;
		}

		/* Pick randomly */
		int pick = Prng_range(rng, 0, candidate_count - 1);
		int h_idx = candidates[pick];
		hotspots[h_idx].used = true;

		/* Load and stamp chunk */
		int origin_x = hotspots[h_idx].x;
		int origin_y = hotspots[h_idx].y;
		int sw = 0, sh = 0;
		ChunkTemplate chunk;
		if (Chunk_load(&chunk, def->chunk_path)) {
			ChunkTransform transform = (ChunkTransform)Prng_range(rng, 0, TRANSFORM_COUNT - 1);

			/* Origin = hotspot minus half-size (already grid-aligned) */
			origin_x = hotspots[h_idx].x - chunk.width / 2;
			origin_y = hotspots[h_idx].y - chunk.height / 2;

			/* Compute transformed dimensions */
			if (transform == TRANSFORM_ROT90 || transform == TRANSFORM_ROT270 ||
			    transform == TRANSFORM_MIRROR_H_ROT90 || transform == TRANSFORM_MIRROR_V_ROT90) {
				sw = chunk.height;
				sh = chunk.width;
			} else {
				sw = chunk.width;
				sh = chunk.height;
			}

			Chunk_stamp(&chunk, zone, origin_x, origin_y, transform);

			/* Resolve chunk spawns — portals, savepoints, enemies */
			int portal_wire_idx = 0;
			int savepoint_wire_idx = 0;

			for (int si = 0; si < chunk.spawn_count; si++) {
				/* Transform spawn position — spawns sit at intersections,
			 * not cell centers, so use the intersection transform */
				int tx, ty;
				Chunk_transform_spawn(chunk.spawns[si].x, chunk.spawns[si].y,
				                      chunk.width, chunk.height,
				                      transform, &tx, &ty);
				int gx = origin_x + tx;
				int gy = origin_y + ty;

				if (gx < 0 || gx >= zone->size || gy < 0 || gy >= zone->size)
					continue;

				/* Consume PRNG for determinism (portals/savepoints filter here,
				 * enemies store probability and re-roll on each spawn) */
				float roll = Prng_float(rng);
				bool is_enemy = strcmp(chunk.spawns[si].entity_type, "portal") != 0 &&
				                strcmp(chunk.spawns[si].entity_type, "savepoint") != 0;
				if (!is_enemy && roll > chunk.spawns[si].probability)
					continue;

				if (strcmp(chunk.spawns[si].entity_type, "portal") == 0) {
					/* Wire portal using landmark def's wiring data */
					if (portal_wire_idx < def->portal_count &&
					    zone->portal_count < ZONE_MAX_PORTALS) {
						const LandmarkPortalWiring *w = &def->portals[portal_wire_idx++];
						ZonePortal *p = &zone->portals[zone->portal_count++];
						p->grid_x = gx;
						p->grid_y = gy;
						strncpy(p->id, w->portal_id, 31);
						p->id[31] = '\0';
						strncpy(p->dest_zone, w->dest_zone, 255);
						p->dest_zone[255] = '\0';
						strncpy(p->dest_portal_id, w->dest_portal_id, 31);
						p->dest_portal_id[31] = '\0';
					}
				}
				else if (strcmp(chunk.spawns[si].entity_type, "savepoint") == 0) {
					/* Wire savepoint using landmark def's wiring data */
					if (savepoint_wire_idx < def->savepoint_count &&
					    zone->savepoint_count < ZONE_MAX_SAVEPOINTS) {
						const LandmarkSavepointWiring *w = &def->savepoints[savepoint_wire_idx++];
						ZoneSavepoint *sp = &zone->savepoints[zone->savepoint_count++];
						sp->grid_x = gx;
						sp->grid_y = gy;
						strncpy(sp->id, w->savepoint_id, 31);
						sp->id[31] = '\0';
					}
				}
				else {
					/* Enemy spawn — store with probability for re-roll on death */
					if (zone->spawn_count < ZONE_MAX_SPAWNS) {
						ZoneSpawn *sp = &zone->spawns[zone->spawn_count++];
						strncpy(sp->enemy_type, chunk.spawns[si].entity_type,
						        sizeof(sp->enemy_type) - 1);
						sp->enemy_type[sizeof(sp->enemy_type) - 1] = '\0';
						/* Convert grid to world coords */
						sp->world_x = (gx - HALF_MAP_SIZE) * MAP_CELL_SIZE;
						sp->world_y = (gy - HALF_MAP_SIZE) * MAP_CELL_SIZE;
						sp->probability = chunk.spawns[si].probability;
					}
				}
			}
		}

		out[placed_count].def = def;
		out[placed_count].grid_x = hotspots[h_idx].x;
		out[placed_count].grid_y = hotspots[h_idx].y;
		out[placed_count].origin_x = origin_x;
		out[placed_count].origin_y = origin_y;
		out[placed_count].stamp_w = sw;
		out[placed_count].stamp_h = sh;
		placed_count++;
	}

	return placed_count;
}

/* --- Influence field --- */

#define DENSE_WALL_SHIFT    0.35
#define SPARSE_WALL_SHIFT   0.30
#define MODERATE_WALL_SHIFT 0.10

static double compute_wall_threshold(int gx, int gy, double base_threshold,
                                     const PlacedLandmark *landmarks, int count)
{
	double threshold = base_threshold;

	for (int i = 0; i < count; i++) {
		double dx = gx - landmarks[i].grid_x;
		double dy = gy - landmarks[i].grid_y;
		double dist = sqrt(dx * dx + dy * dy);
		const TerrainInfluence *inf = &landmarks[i].def->influence;

		if (dist >= inf->radius) continue;

		double t = 1.0 - (dist / inf->radius);
		double weight = pow(t, (double)inf->falloff) * inf->strength;

		switch (inf->type) {
		case INFLUENCE_DENSE:
			threshold += weight * DENSE_WALL_SHIFT;
			break;
		case INFLUENCE_SPARSE:
			threshold -= weight * SPARSE_WALL_SHIFT;
			break;
		case INFLUENCE_MODERATE:
			threshold += weight * MODERATE_WALL_SHIFT;
			break;
		}
	}

	if (threshold < -0.8) threshold = -0.8;
	if (threshold > 0.8) threshold = 0.8;

	return threshold;
}

/* --- Main generation --- */

/* Circuit vein noise: separate noise field determines where circuit
 * patterns appear within walls. Gives organic veins/clusters instead
 * of random scatter. Lower threshold = more circuit tiles. */
#define CIRCUIT_VEIN_OCTAVES   3
#define CIRCUIT_VEIN_FREQ      0.03
#define CIRCUIT_VEIN_THRESHOLD 0.30

/* Landmark edge erosion: smooths the hard rectangular boundary between
 * chunk-stamped areas and noise terrain by eroding nearby wall cells
 * using a noise-modulated depth. */
#define ERODE_MAX_DIST       16      /* BFS reach from chunk boundary */
#define ERODE_MIN_DEPTH      2       /* minimum erosion (always breaks edges) */
#define ERODE_NOISE_OCTAVES  2       /* low = smooth, slowly curving edges */
#define ERODE_NOISE_FREQ     0.04    /* low frequency = broad curves */
#define ERODE_SEED_OFFSET    77777u  /* offset from zone_seed */

static void erode_landmark_edges(Zone *zone, uint32_t zone_seed)
{
	int size = zone->size;
	uint32_t erode_seed = zone_seed + ERODE_SEED_OFFSET;

	/* Distance field: -1 = not near boundary, 1..MAX = distance from edge */
	int8_t *dist = malloc(size * size * sizeof(int8_t));
	int *qx = malloc(size * size * sizeof(int));
	int *qy = malloc(size * size * sizeof(int));
	if (!dist || !qx || !qy) {
		fprintf(stderr, "Procgen: erode_landmark_edges allocation failed\n");
		free(dist); free(qx); free(qy);
		return;
	}
	memset(dist, -1, size * size);
	int qhead = 0, qtail = 0;

	/* Seed BFS: non-stamped cells adjacent (8-connected) to stamped cells */
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			if (zone->cell_chunk_stamped[x][y]) continue;
			bool adj = false;
			for (int dy = -1; dy <= 1 && !adj; dy++)
				for (int dx = -1; dx <= 1 && !adj; dx++) {
					if (dx == 0 && dy == 0) continue;
					int nx = x + dx, ny = y + dy;
					if (nx >= 0 && nx < size && ny >= 0 && ny < size
					    && zone->cell_chunk_stamped[nx][ny])
						adj = true;
				}
			if (adj) {
				dist[y * size + x] = 1;
				qx[qtail] = x; qy[qtail] = y; qtail++;
			}
		}
	}

	/* BFS outward (8-connected / Chebyshev) up to ERODE_MAX_DIST */
	while (qhead < qtail) {
		int cx = qx[qhead], cy = qy[qhead]; qhead++;
		int d = dist[cy * size + cx];
		if (d >= ERODE_MAX_DIST) continue;
		for (int dy = -1; dy <= 1; dy++)
			for (int dx = -1; dx <= 1; dx++) {
				if (dx == 0 && dy == 0) continue;
				int nx = cx + dx, ny = cy + dy;
				if (nx < 0 || nx >= size || ny < 0 || ny >= size) continue;
				if (zone->cell_chunk_stamped[nx][ny]) continue;
				if (dist[ny * size + nx] >= 0) continue;
				dist[ny * size + nx] = d + 1;
				qx[qtail] = nx; qy[qtail] = ny; qtail++;
			}
	}

	/* Erosion pass: clear walls within noise-modulated depth of boundary */
	int eroded = 0;
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			int d = dist[y * size + x];
			if (d < 1 || d > ERODE_MAX_DIST) continue;
			if (zone->cell_grid[x][y] < 0) continue;       /* not a wall */
			if (zone->cell_hand_placed[x][y]) continue;     /* protected */

			double noise = Noise_fbm((double)x, (double)y,
				ERODE_NOISE_OCTAVES, ERODE_NOISE_FREQ,
				2.0, 0.5, erode_seed);
			double depth = ERODE_MIN_DEPTH
				+ (noise + 1.0) * 0.5 * (ERODE_MAX_DIST - ERODE_MIN_DEPTH);
			if ((double)d <= depth) {
				zone->cell_grid[x][y] = -1;
				eroded++;
			}
		}
	}

	free(dist); free(qx); free(qy);
	printf("Procgen: eroded %d wall cells near landmark edges\n", eroded);
}

void Procgen_generate(Zone *zone)
{
	if (!zone->procgen) return;

	uint32_t zone_seed = Procgen_derive_zone_seed(master_seed, zone->filepath);
	debug_zone_seed = zone_seed;
	Prng rng;
	Prng_seed(&rng, zone_seed);

	/* ─── Hotspot Generation ─── */
	Hotspot hotspots[MAX_HOTSPOTS];
	int hotspot_count = generate_hotspots(zone, &rng, hotspots);

	/* ─── Landmark Resolution ─── */
	PlacedLandmark placed[ZONE_MAX_LANDMARKS];
	int placed_count = resolve_landmarks(zone, &rng, hotspots, hotspot_count, placed);

	/* Store debug data */
	debug_hotspot_count = hotspot_count;
	memcpy(debug_hotspots, hotspots, sizeof(Hotspot) * hotspot_count);
	debug_landmark_count = placed_count;
	memcpy(debug_landmarks, placed, sizeof(PlacedLandmark) * placed_count);

	/* ─── Terrain Generation with Influence ─── */

	/* Find circuit vs solid type indices */
	int circuit_idx = -1;
	int solid_idx = -1;
	bool has_both = false;
	for (int i = 0; i < zone->wall_type_count; i++) {
		int idx = zone->wall_type_indices[i];
		if (strcmp(zone->cell_types[idx].pattern, "circuit") == 0)
			circuit_idx = idx;
		else
			solid_idx = idx;
	}
	has_both = (circuit_idx >= 0 && solid_idx >= 0);

	int default_wall = (zone->wall_type_count > 0)
		? zone->wall_type_indices[0] : 0;

	int walls_placed = 0;
	uint32_t vein_seed = zone_seed + 12345u;

	for (int y = 0; y < zone->size; y++) {
		for (int x = 0; x < zone->size; x++) {
			/* Always consume PRNG for determinism */
			Prng_float(&rng);

			if (zone->cell_hand_placed[x][y])
				continue;
			if (zone->cell_chunk_stamped[x][y])
				continue;

			double noise_val = Noise_fbm(
				(double)x, (double)y,
				zone->noise_octaves,
				zone->noise_frequency,
				zone->noise_lacunarity,
				zone->noise_persistence,
				zone_seed
			);

			/* Influence-modulated wall threshold */
			double wall_thresh = compute_wall_threshold(
				x, y, zone->noise_wall_threshold,
				placed, placed_count);

			if (noise_val < wall_thresh) {
				int wall_type = default_wall;
				if (has_both) {
					double vein = Noise_fbm(
						(double)x, (double)y,
						CIRCUIT_VEIN_OCTAVES,
						CIRCUIT_VEIN_FREQ,
						2.0, 0.5,
						vein_seed
					);
					wall_type = (vein > CIRCUIT_VEIN_THRESHOLD)
						? circuit_idx : solid_idx;
				}
				zone->cell_grid[x][y] = wall_type;
				walls_placed++;
			}
			/* else: leave as -1 (empty space over cloudscape) */
		}
	}

	/* ─── Landmark Edge Erosion ─── */
	erode_landmark_edges(zone, zone_seed);

	printf("Procgen_generate: seed=%u zone_seed=%u walls=%d hotspots=%d landmarks=%d\n",
	       master_seed, zone_seed, walls_placed, hotspot_count, placed_count);
}
