#ifndef PORTAL_H
#define PORTAL_H

#include "position.h"
#include "entity.h"
#include <stdbool.h>

#define PORTAL_COUNT 16
#define PORTAL_SIZE 100.0
#define PORTAL_HALF_SIZE (PORTAL_SIZE * 0.5)

void Portal_initialize(Position position, const char *id,
	const char *dest_zone, const char *dest_portal_id);
void Portal_cleanup(void);
void Portal_update_all(unsigned int ticks);
void Portal_render(const void *state, const PlaceableComponent *placeable);
void Portal_render_deactivated(void);
void Portal_render_bloom_source(void);
void Portal_render_god_labels(void);

/* Returns true if a portal transition was triggered this frame */
bool Portal_has_pending_transition(void);
const char *Portal_get_pending_dest_zone(void);
const char *Portal_get_pending_dest_portal_id(void);
void Portal_clear_pending_transition(void);

/* Find arrival position for a portal by id. Returns false if not found. */
bool Portal_get_position_by_id(const char *id, Position *out);

/* Suppress re-trigger after arriving at destination */
void Portal_suppress_arrival(const char *portal_id);

/* Minimap rendering */
void Portal_render_minimap(float ship_x, float ship_y,
	float screen_x, float screen_y, float size, float range);

#endif
