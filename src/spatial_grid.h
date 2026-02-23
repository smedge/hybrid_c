#ifndef SPATIAL_GRID_H
#define SPATIAL_GRID_H

#include <stdbool.h>

#define BUCKET_SIZE 64
#define GRID_MAX 16
#define BUCKET_CAPACITY 256

typedef enum {
	ENTITY_HUNTER,
	ENTITY_SEEKER,
	ENTITY_MINE,
	ENTITY_DEFENDER,
	ENTITY_STALKER
} SpatialEntityType;

typedef struct {
	SpatialEntityType type;
	int index;
} EntityRef;

void SpatialGrid_init(void);
void SpatialGrid_clear(void);
void SpatialGrid_add(EntityRef ref, double world_x, double world_y);
void SpatialGrid_remove(EntityRef ref, double world_x, double world_y);
void SpatialGrid_update(EntityRef ref, double old_x, double old_y, double new_x, double new_y);
bool SpatialGrid_is_active(double world_x, double world_y);
void SpatialGrid_set_player_bucket(double world_x, double world_y);
void SpatialGrid_validate(void);
int SpatialGrid_query_neighborhood(int bucket_x, int bucket_y, EntityRef *out, int out_capacity);

#endif
