#ifndef SUB_CINDER_CORE_H
#define SUB_CINDER_CORE_H

#include <stdbool.h>
#include "position.h"
#include "collision.h"
#include "burn.h"
#include "sub_mine_core.h"

/* Config — single source of truth for cinder fire pool tuning */
typedef struct {
	float pool_radius;           /* collision/render radius */
	int pool_duration_ms;        /* how long the fire pool persists */
	int burn_interval_ms;        /* min ms between burns per pool */
	int detonation_burn_stacks;  /* burn stacks applied on detonation */
} SubCinderConfig;

/* A lingering fire pool spawned on cinder mine detonation */
typedef struct {
	bool active;
	Position position;
	int life_ms;
	int burn_tick_ms;   /* countdown for interval burn */
	BurnState burn;     /* for visual registration with burn system */
} CinderFirePool;

const SubCinderConfig *SubCinder_get_config(void);
const SubMineConfig *SubCinder_get_fire_mine_config(void);

void SubCinder_init_pool(CinderFirePool *pool);
void SubCinder_spawn_pool(CinderFirePool *pools, int max, Position pos);
void SubCinder_update_pools(CinderFirePool *pools, int max, const SubCinderConfig *cfg, unsigned int ticks);
int SubCinder_check_pool_burn(CinderFirePool *pools, int max, const SubCinderConfig *cfg, Rectangle target);
void SubCinder_deactivate_pools(CinderFirePool *pools, int max);

/* Rendering */
void SubCinder_render_pools(const CinderFirePool *pools, int max, const SubCinderConfig *cfg);
void SubCinder_render_pools_bloom(const CinderFirePool *pools, int max, const SubCinderConfig *cfg);
void SubCinder_render_pools_light(const CinderFirePool *pools, int max, const SubCinderConfig *cfg);

#endif
