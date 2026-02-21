#include "procgen.h"
#include "chunk.h"
#include "noise.h"
#include "prng.h"

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
	int grid_x, grid_y;
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
	int center = zone->size / 2;

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

			/* Center exclusion — only if zone has a center anchor */
			if (zone->has_center_anchor) {
				double dx = cx - center;
				double dy = cy - center;
				if (sqrt(dx * dx + dy * dy) < zone->hotspot_center_exclusion)
					continue;
			}

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
		ChunkTemplate chunk;
		if (Chunk_load(&chunk, def->chunk_path)) {
			ChunkTransform transform = (ChunkTransform)Prng_range(rng, 0, TRANSFORM_COUNT - 1);

			/* Origin = hotspot minus half-size (already grid-aligned) */
			int origin_x = hotspots[h_idx].x - chunk.width / 2;
			int origin_y = hotspots[h_idx].y - chunk.height / 2;
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

				/* Probability check */
				float roll = Prng_float(rng);
				if (roll > chunk.spawns[si].probability)
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
					/* Enemy spawn */
					if (zone->spawn_count < ZONE_MAX_SPAWNS) {
						ZoneSpawn *sp = &zone->spawns[zone->spawn_count++];
						strncpy(sp->enemy_type, chunk.spawns[si].entity_type,
						        sizeof(sp->enemy_type) - 1);
						sp->enemy_type[sizeof(sp->enemy_type) - 1] = '\0';
						/* Convert grid to world coords */
						sp->world_x = (gx - HALF_MAP_SIZE) * MAP_CELL_SIZE;
						sp->world_y = (gy - HALF_MAP_SIZE) * MAP_CELL_SIZE;
					}
				}
			}
		}

		out[placed_count].def = def;
		out[placed_count].grid_x = hotspots[h_idx].x;
		out[placed_count].grid_y = hotspots[h_idx].y;
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

void Procgen_generate(Zone *zone)
{
	if (!zone->procgen) return;

	uint32_t zone_seed = Procgen_derive_zone_seed(master_seed, zone->filepath);
	debug_zone_seed = zone_seed;
	Prng rng;
	Prng_seed(&rng, zone_seed);

	/* ─── Center Anchor (optional) ─── */
	if (zone->has_center_anchor) {
		ChunkTemplate anchor;
		if (Chunk_load(&anchor, zone->center_anchor_path)) {
			ChunkTransform transform = (ChunkTransform)(zone_seed % TRANSFORM_COUNT);
			int center = zone->size / 2;
			int origin_x = center - anchor.width / 2;
			int origin_y = center - anchor.height / 2;
			Chunk_stamp(&anchor, zone, origin_x, origin_y, transform);
			printf("Procgen: center anchor '%s' stamped with transform %d\n",
			       anchor.name, transform);
		}
	} else {
		/* Legacy fallback: clear 64x64 center zone */
		int center = zone->size / 2;
		int fortress_half = 32;
		int fort_min = center - fortress_half;
		int fort_max = center + fortress_half;
		for (int y = fort_min; y < fort_max; y++)
			for (int x = fort_min; x < fort_max; x++)
				if (x >= 0 && x < zone->size && y >= 0 && y < zone->size)
					zone->cell_chunk_stamped[x][y] = true;
	}

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

	printf("Procgen_generate: seed=%u zone_seed=%u walls=%d hotspots=%d landmarks=%d\n",
	       master_seed, zone_seed, walls_placed, hotspot_count, placed_count);
}
