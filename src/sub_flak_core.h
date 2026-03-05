#ifndef SUB_FLAK_CORE_H
#define SUB_FLAK_CORE_H

#include <stdbool.h>
#include "sub_projectile_core.h"

/* Flak config — narrow cone of fire pellets with burn DOT */
typedef struct {
	int pellets_per_shot;
	double spread_half_angle_deg; /* half-angle of cone */
	int fire_cooldown_ms;
	float feedback_cost;
	SubProjectileConfig proj; /* per-pellet physics/visuals */
} SubFlakConfig;

const SubFlakConfig *SubFlak_get_config(void);

/* Audio lifecycle (owns its own fire sound) */
void SubFlak_initialize_audio(void);
void SubFlak_cleanup_audio(void);

/* Fire a spread shot. Returns true if fired (cooldown ready). */
bool SubFlak_try_fire(SubProjectilePool *pool, const SubFlakConfig *cfg,
	Position origin, Position target);

#endif
