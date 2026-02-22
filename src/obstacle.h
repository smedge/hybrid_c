#ifndef OBSTACLE_H
#define OBSTACLE_H

#include "chunk.h"

/* Load all .chunk files from resources/obstacles/ directory.
 * Safe to call multiple times — reloads only if not already loaded. */
void Obstacle_load_library(void);

/* Free all loaded obstacle templates. */
void Obstacle_cleanup_library(void);

/* Number of loaded obstacle templates. */
int Obstacle_get_count(void);

/* Find an obstacle template by name (without path/extension).
 * Returns NULL if not found. */
const ChunkTemplate *Obstacle_find(const char *name);

#endif
