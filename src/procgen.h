#ifndef PROCGEN_H
#define PROCGEN_H

#include <stdint.h>
#include "zone.h"

void     Procgen_set_master_seed(uint32_t seed);
uint32_t Procgen_get_master_seed(void);

uint32_t Procgen_derive_zone_seed(uint32_t master_seed, const char *zone_id);

/* Generate terrain for the given zone. Fills zone->cell_grid using noise.
 * Skips cells already placed by hand (cell_hand_placed is true).
 * Call after zone file parsing, before apply_zone_to_world(). */
void Procgen_generate(Zone *zone);

#endif
