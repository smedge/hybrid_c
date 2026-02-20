#include "procgen.h"
#include "noise.h"
#include "prng.h"

#include <string.h>
#include <stdio.h>

static uint32_t master_seed = 0;

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

/* Circuit vein noise: separate noise field determines where circuit
 * patterns appear within walls. Gives organic veins/clusters instead
 * of random scatter. threshold 0.5 ≈ top ~15-20% of noise range. */
#define CIRCUIT_VEIN_OCTAVES   3
#define CIRCUIT_VEIN_FREQ      0.03
#define CIRCUIT_VEIN_THRESHOLD 0.5

void Procgen_generate(Zone *zone)
{
	if (!zone->procgen) return;

	uint32_t zone_seed = Procgen_derive_zone_seed(master_seed, zone->filepath);
	Prng rng;
	Prng_seed(&rng, zone_seed);

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

	/* Fallback: if only one type, just use that */
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

			double noise_val = Noise_fbm(
				(double)x, (double)y,
				zone->noise_octaves,
				zone->noise_frequency,
				zone->noise_lacunarity,
				zone->noise_persistence,
				zone_seed
			);

			if (noise_val < zone->noise_wall_threshold) {
				int wall_type = default_wall;
				if (has_both) {
					/* Second noise layer for circuit veins */
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

	printf("Procgen_generate: seed=%u zone_seed=%u walls=%d\n",
		master_seed, zone_seed, walls_placed);
}
