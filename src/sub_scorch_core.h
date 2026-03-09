#ifndef SUB_SCORCH_CORE_H
#define SUB_SCORCH_CORE_H

#include <stdbool.h>
#include "position.h"
#include "collision.h"
#include "burn.h"
#include "sub_sprint_core.h"

/*
 * Sub Scorch Core — Burning sprint trail (fire variant of sprint)
 *
 * Speed boost that deposits burning footprints. Wraps SubSprintCore
 * for the movement boost, adds persistent fire trail management.
 * Used by both player (sub_scorch.c) and Fire Corruptor enemy.
 */

/* A single burning footprint left during scorch sprint */
typedef struct {
	bool active;
	Position position;
	int life_ms;
	int burn_tick_ms;   /* throttle per-footprint for interval burn */
	BurnState burn;     /* visual registration with burn system */
} ScorchFootprint;

typedef struct {
	int sprint_duration_ms;     /* shorter than base sprint */
	int sprint_cooldown_ms;
	int footprint_interval_ms;  /* ms between footprint deposits */
	double footprint_radius;    /* collision/render radius */
	int footprint_life_ms;      /* how long footprints persist */
	int burn_interval_ms;       /* min ms between burns per footprint */
} SubScorchConfig;

typedef struct {
	SubSprintCore sprint;
	int footprint_timer;        /* accumulator for deposit spacing */
} SubScorchCore;

/* Footprint pool handle — each user (player, corruptor) owns one */
typedef struct {
	ScorchFootprint *data;
	int max;
} ScorchFootprintPool;

const SubScorchConfig *SubScorch_get_config(void);

void SubScorch_init(SubScorchCore *core);
bool SubScorch_try_activate(SubScorchCore *core, const SubScorchConfig *cfg, Position pos);
bool SubScorch_update(SubScorchCore *core, const SubScorchConfig *cfg, unsigned int ticks);
bool SubScorch_is_active(const SubScorchCore *core);
float SubScorch_get_cooldown_fraction(const SubScorchCore *core, const SubScorchConfig *cfg);

/* Footprint pool — each caller owns a separate pool */
void SubScorch_pool_init(ScorchFootprintPool *pool, ScorchFootprint *buffer, int max);
void SubScorch_pool_spawn(ScorchFootprintPool *pool, const SubScorchConfig *cfg, Position pos);
void SubScorch_pool_update(ScorchFootprintPool *pool, const SubScorchConfig *cfg, unsigned int ticks);
int SubScorch_pool_check_burn(ScorchFootprintPool *pool, const SubScorchConfig *cfg, Rectangle target);
void SubScorch_pool_deactivate_all(ScorchFootprintPool *pool);

/* Rendering */
void SubScorch_pool_render(ScorchFootprintPool *pool, const SubScorchConfig *cfg);
void SubScorch_pool_render_bloom(ScorchFootprintPool *pool, const SubScorchConfig *cfg);
void SubScorch_pool_render_light(ScorchFootprintPool *pool);

/* Legacy global pool API — wraps a single default pool */
void SubScorch_init_footprints(ScorchFootprint *buffer, int max);
void SubScorch_spawn_footprint(SubScorchCore *core, const SubScorchConfig *cfg, Position pos);
void SubScorch_update_footprints(const SubScorchConfig *cfg, unsigned int ticks);
int SubScorch_check_footprint_burn(const SubScorchConfig *cfg, Rectangle target);
void SubScorch_deactivate_all_footprints(void);
void SubScorch_render_footprints(const SubScorchConfig *cfg);
void SubScorch_render_footprints_bloom(const SubScorchConfig *cfg);
void SubScorch_render_footprints_light(void);

/* Audio */
void SubScorch_initialize_audio(void);
void SubScorch_cleanup_audio(void);

#endif
