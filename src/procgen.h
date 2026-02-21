#ifndef PROCGEN_H
#define PROCGEN_H

#include <stdbool.h>
#include <stdint.h>
#include "zone.h"

void     Procgen_set_master_seed(uint32_t seed);
uint32_t Procgen_get_master_seed(void);

uint32_t Procgen_derive_zone_seed(uint32_t master_seed, const char *zone_id);

/* Generate terrain for the given zone. Fills zone->cell_grid using noise,
 * stamps center anchor + landmark chunks, modulates terrain with influence.
 * Skips cells already placed by hand (cell_hand_placed is true).
 * Call after zone file parsing, before apply_zone_to_world(). */
void Procgen_generate(Zone *zone);

/* --- Debug data accessors (for godmode rendering) --- */

int      Procgen_get_hotspot_count(void);
void     Procgen_get_hotspot(int i, int *x, int *y, bool *used);

int      Procgen_get_landmark_count(void);
void     Procgen_get_landmark(int i, int *x, int *y, const char **type,
                              float *radius, int *inf_type);

uint32_t Procgen_get_zone_seed(void);

#endif
