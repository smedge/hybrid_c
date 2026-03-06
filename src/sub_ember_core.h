#ifndef SUB_EMBER_CORE_H
#define SUB_EMBER_CORE_H

#include <stdbool.h>
#include "sub_projectile_core.h"

/* Ember config — fire projectile with burn on hit + AOE ignite */
typedef struct {
	SubProjectileConfig proj;
	int fire_cooldown_ms;
	float feedback_cost;
	float ignite_radius;   /* AOE burn splash radius on impact */
} SubEmberConfig;

const SubEmberConfig *SubEmber_get_config(void);

/* AOE ignite — store impact position, checked by enemies this frame */
void SubEmber_add_burst(Position impact);
int SubEmber_check_burst(Rectangle target);
void SubEmber_clear_bursts(void);

/* Audio */
void SubEmber_initialize_audio(void);
void SubEmber_cleanup_audio(void);

#endif
