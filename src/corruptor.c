#include "corruptor.h"
#include "enemy_util.h"
#include "enemy_variant.h"
#include "burn.h"
#include "enemy_feedback.h"
#include "enemy_registry.h"
#include "fragment.h"
#include "progression.h"
#include "player_stats.h"
#include "ship.h"
#include "sub_stealth.h"
#include "sub_emp_core.h"
#include "sub_sprint_core.h"
#include "sub_resist_core.h"
#include "sub_scorch_core.h"
#include "sub_heatwave_core.h"
#include "sub_temper_core.h"
#include "view.h"
#include "render.h"
#include "color.h"
#include "audio.h"
#include "map.h"
#include "spatial_grid.h"
#include "global_render.h"
#include "global_update.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL_mixer.h>

#define CORRUPTOR_COUNT 4096
#define CORRUPTOR_HP 70.0
#define NORMAL_SPEED 200.0
#define SPRINT_SPEED 600.0
#define RETREAT_SPEED 300.0
#define AGGRO_RANGE 1200.0
#define DEAGGRO_RANGE 3200.0
#define EMP_FEEDBACK_COST 30.0
#define EMP_PLAYER_DURATION_MS 10000
#define RESIST_RANGE 800.0
#define RESIST_FEEDBACK_COST 20.0


#define RESPAWN_MS 30000
#define BODY_SIZE 10.0
#define IDLE_DRIFT_RADIUS 400.0
#define IDLE_DRIFT_SPEED 80.0
#define IDLE_WANDER_INTERVAL 2000
#define WALL_CHECK_DIST 50.0
#define DEATH_FLASH_MS 200
#define SPRINT_ENGAGE_MIN 800.0
#define SPRINT_ENGAGE_MAX 1200.0
#define SPRINT_FIRE_RANGE 250.0
#define RETREAT_SAFE_DIST 1000.0

#define SPRINT_TRAIL_GHOSTS 12
#define SPRINT_TRAIL_LENGTH 3.0

/* Fire variant — scorch footprint pool */
#define CORRUPTOR_FOOTPRINT_MAX 64
static ScorchFootprint corruptorFootprintBuf[CORRUPTOR_FOOTPRINT_MAX];
static bool corruptorFootprintsInitialized = false;

/* Fire variant — heatwave feedback cost + player debuff */
#define HEATWAVE_FEEDBACK_COST 30.0

static const CarriedSubroutine corruptorCarried[] = {
	{ SUB_ID_SPRINT,  FRAG_TYPE_SPRINT },
	{ SUB_ID_EMP,     FRAG_TYPE_EMP },
	{ SUB_ID_RESIST, FRAG_TYPE_RESIST },
};

static const EnemyVariant corruptorVariants[THEME_COUNT] = {
	[THEME_FIRE] = {
		.tint = {1.0f, 0.6f, 0.0f, 1.0f},
		.carried = {
			{ SUB_ID_SCORCH,   FRAG_TYPE_SCORCH },
			{ SUB_ID_HEATWAVE, FRAG_TYPE_HEATWAVE },
			{ SUB_ID_TEMPER,   FRAG_TYPE_TEMPER },
		},
		.carried_count = 3,
		.speed_mult = 1.0f, .aggro_range_mult = 1.0f,
		.hp_mult = 1.0f, .attack_cadence_mult = 1.0f,
	},
};

typedef enum {
	CORRUPTOR_IDLE,
	CORRUPTOR_SUPPORTING,
	CORRUPTOR_SPRINTING,
	CORRUPTOR_EMP_FIRING,
	CORRUPTOR_RETREATING,
	CORRUPTOR_DYING,
	CORRUPTOR_DEAD
} CorruptorAIState;

typedef struct {
	bool alive;
	double hp;
	CorruptorAIState aiState;
	Position spawnPoint;
	double facing;
	bool killedByPlayer;

	/* Idle wander */
	Position wanderTarget;
	int wanderTimer;

	/* Sprint */
	Position prevPosition;
	SubSprintCore sprintCore;

	/* EMP */
	SubEmpCore empCore;

	/* Resist aura */
	SubResistCore resistCore;
	bool hasResistTarget;
	Position resistBeamTarget;

	/* Death/respawn */
	int deathTimer;
	int respawnTimer;

	/* Feedback */
	EnemyFeedback fb;

	/* Theme variant */
	ZoneTheme theme;

	/* Burn DOT */
	BurnState burn;

	/* Fire variant cores */
	SubScorchCore scorchCore;
	SubHeatwaveCore heatwaveCore;
	SubTemperCore temperCore;
} CorruptorState;

/* Shared singleton components */
static void corruptor_render_bloom(const void *state, const PlaceableComponent *placeable);
static void corruptor_render_light(const void *state, const PlaceableComponent *placeable);
static void corruptor_render_pool_bloom(void);
static void corruptor_footprint_burn_wrapper(const unsigned int ticks);
static RenderableComponent renderable = {.passes = {
	[RENDER_PASS_MAIN] = Corruptor_render,
	[RENDER_PASS_BLOOM_SOURCE] = corruptor_render_bloom,
	[RENDER_PASS_LIGHT_SOURCE] = corruptor_render_light,
}};
static CollidableComponent collidable = {{-BODY_SIZE, BODY_SIZE, BODY_SIZE, -BODY_SIZE},
										  true,
										  COLLISION_LAYER_ENEMY,
										  COLLISION_LAYER_PLAYER,
										  Corruptor_collide, Corruptor_resolve};
static AIUpdatableComponent updatable = {Corruptor_update};

/* Colors — yellow */
static const ColorFloat colorBody  = {1.0f, 0.9f, 0.0f, 1.0f};
static const ColorFloat colorAggro = {1.0f, 0.95f, 0.3f, 1.0f};

/* State arrays */
static CorruptorState corruptors[CORRUPTOR_COUNT];
static PlaceableComponent placeables[CORRUPTOR_COUNT];
static Entity *entityRefs[CORRUPTOR_COUNT];
static int highestUsedIndex = 0;
static int corruptorTypeId = -1;

/* Self-exclusion for find_wounded/find_aggro */
static int currentUpdaterIdx = -1;

/* Sparks */
#define SPARK_DURATION 80
#define SPARK_SIZE 12.0
#define SPARK_POOL_SIZE 8
static struct {
	bool active;
	Position position;
	int ticksLeft;
} sparks[SPARK_POOL_SIZE];

static void activate_spark(Position pos) {
	int slot = 0;
	for (int i = 0; i < SPARK_POOL_SIZE; i++) {
		if (!sparks[i].active) { slot = i; break; }
		if (sparks[i].ticksLeft < sparks[slot].ticksLeft) slot = i;
	}
	sparks[slot].active = true;
	sparks[slot].position = pos;
	sparks[slot].ticksLeft = SPARK_DURATION;
}

/* Audio — entity sounds only (damage/death/respawn) */
static Mix_Chunk *sampleDeath = 0;
static Mix_Chunk *sampleRespawn = 0;
static Mix_Chunk *sampleHit = 0;
static bool pipelineRegistered = false;

/* Helpers */
static void pick_wander_target(CorruptorState *c)
{
	Enemy_pick_wander_target(c->spawnPoint, IDLE_DRIFT_RADIUS, IDLE_WANDER_INTERVAL,
		&c->wanderTarget, &c->wanderTimer);
}

static bool can_engage_player(CorruptorState *c, Position myPos)
{
	if (Ship_is_destroyed() || Sub_Stealth_is_stealthed())
		return false;
	if (c->theme == THEME_FIRE) {
		if (c->heatwaveCore.cooldownMs > 0)
			return false;
		if (SubTemper_is_active(&c->temperCore))
			return false;
	} else {
		if (c->empCore.cooldownMs > 0)
			return false;
		if (c->resistCore.active)
			return false;
	}

	Position shipPos = Ship_get_position();
	double dist = Enemy_distance_between(myPos, shipPos);
	if (dist < SPRINT_ENGAGE_MIN || dist > SPRINT_ENGAGE_MAX)
		return false;
	if (!Enemy_has_line_of_sight(myPos, shipPos))
		return false;
	if (PlayerStats_is_shielded())
		return false;

	return true;
}

static void try_apply_resist(CorruptorState *c, Position myPos)
{
	if (c->resistCore.active || c->resistCore.cooldownMs > 0)
		return;

	/* Reactive resist: only activate when a nearby ally (or self) has taken damage */
	bool shouldActivate = false;

	/* Check nearby allies via registry — any damage at all triggers resist */
	int typeCount = EnemyRegistry_type_count();
	for (int t = 0; t < typeCount; t++) {
		const EnemyTypeCallbacks *et = EnemyRegistry_get_type(t);
		Position pos;
		int eidx;
		if (et->find_wounded && et->find_wounded(myPos, RESIST_RANGE, 9999.0, &pos, &eidx)) {
			shouldActivate = true;
			break;
		}
	}

	/* Self-check — corruptor itself is wounded */
	if (!shouldActivate && c->hp < CORRUPTOR_HP)
		shouldActivate = true;

	if (!shouldActivate)
		return;

	double hpBefore = c->hp;
	if (!EnemyFeedback_try_spend(&c->fb, RESIST_FEEDBACK_COST, &c->hp))
		return;
	if (c->hp < hpBefore) {
		activate_spark(myPos);
		Audio_play_sample(&sampleHit);
	}

	/* Feedback spillover self-kill */
	if (c->hp <= 0.0) {
		c->alive = false;
		c->aiState = CORRUPTOR_DYING;
		c->deathTimer = 0;
		c->killedByPlayer = false;
		return;
	}

	SubResist_try_activate(&c->resistCore, SubResist_get_config());
}

static void try_apply_temper(CorruptorState *c, Position myPos)
{
	if (SubTemper_is_active(&c->temperCore))
		return;
	if (SubTemper_get_cooldown_fraction(&c->temperCore, SubTemper_get_config()) > 0.0f)
		return;

	/* Same reactive logic as resist — protect wounded/aggro'd allies */
	bool shouldActivate = false;

	int typeCount = EnemyRegistry_type_count();
	for (int t = 0; t < typeCount; t++) {
		const EnemyTypeCallbacks *et = EnemyRegistry_get_type(t);
		Position pos;
		int eidx;
		if (et->find_wounded && et->find_wounded(myPos, RESIST_RANGE, 9999.0, &pos, &eidx)) {
			shouldActivate = true;
			break;
		}
	}

	if (!shouldActivate && c->hp < CORRUPTOR_HP)
		shouldActivate = true;

	if (!shouldActivate)
		return;

	double hpBefore = c->hp;
	if (!EnemyFeedback_try_spend(&c->fb, RESIST_FEEDBACK_COST, &c->hp))
		return;
	if (c->hp < hpBefore) {
		activate_spark(myPos);
		Audio_play_sample(&sampleHit);
	}

	if (c->hp <= 0.0) {
		c->alive = false;
		c->aiState = CORRUPTOR_DYING;
		c->deathTimer = 0;
		c->killedByPlayer = false;
		return;
	}

	SubTemper_try_activate(&c->temperCore, SubTemper_get_config());
}

/* ---- Public API ---- */

void Corruptor_initialize(Position position, ZoneTheme theme)
{
	if (highestUsedIndex >= CORRUPTOR_COUNT) {
		printf("FATAL ERROR: Too many corruptor entities.\n");
		return;
	}

	int idx = highestUsedIndex;
	CorruptorState *c = &corruptors[idx];

	c->alive = true;
	c->hp = CORRUPTOR_HP;
	c->aiState = CORRUPTOR_IDLE;
	c->spawnPoint = position;
	c->facing = 0.0;
	c->killedByPlayer = false;
	c->theme = theme;
	c->prevPosition = position;
	Burn_reset(&c->burn);
	if (theme == THEME_FIRE) {
		SubScorch_init(&c->scorchCore);
		SubHeatwave_init(&c->heatwaveCore);
		SubTemper_init(&c->temperCore);
	} else {
		SubSprint_init(&c->sprintCore);
		SubEmp_init(&c->empCore);
		SubResist_init(&c->resistCore);
	}
	c->hasResistTarget = false;
	c->resistBeamTarget = position;
	c->deathTimer = 0;
	c->respawnTimer = 0;
	EnemyFeedback_init(&c->fb);
	pick_wander_target(c);

	placeables[idx].position = position;
	placeables[idx].heading = 0.0;

	Entity entity = Entity_initialize_entity();
	entity.state = &corruptors[idx];
	entity.placeable = &placeables[idx];
	entity.renderable = &renderable;
	entity.collidable = &collidable;
	entity.aiUpdatable = &updatable;

	entityRefs[idx] = Entity_add_entity(entity);

	highestUsedIndex++;

	SpatialGrid_add((EntityRef){ENTITY_CORRUPTOR, idx}, position.x, position.y);

	/* Load audio and register with enemy registry once */
	if (!sampleDeath) {
		Audio_load_sample(&sampleDeath, "resources/sounds/bomb_explode.wav");
		Audio_load_sample(&sampleRespawn, "resources/sounds/door.wav");
		Audio_load_sample(&sampleHit, "resources/sounds/samus_hurt.wav");
		SubEmp_initialize_audio();
		SubHeatwave_initialize_audio();
		SubScorch_initialize_audio();
		EnemyTypeCallbacks cb = {Corruptor_find_wounded, Corruptor_find_aggro,
			Corruptor_heal, Corruptor_alert_nearby, Corruptor_apply_emp, Corruptor_apply_heatwave, Corruptor_cleanse_burn, Corruptor_apply_burn};
		corruptorTypeId = EnemyRegistry_register(cb);
	}

	/* Register pipeline callbacks (survives Zone_rebuild_enemies) */
	if (!pipelineRegistered) {
		GlobalRender_register(RENDER_PASS_BLOOM_SOURCE, corruptor_render_pool_bloom);
		GlobalRender_register(RENDER_PASS_WORLD_OVERLAY, Corruptor_render_footprints);
		GlobalRender_register(RENDER_PASS_BLOOM_SOURCE, Corruptor_render_footprint_bloom_source);
		GlobalRender_register(RENDER_PASS_LIGHT_SOURCE, Corruptor_render_footprint_light_source);
		GlobalUpdate_register_post_collision(Corruptor_update_footprints);
		GlobalUpdate_register_post_collision(corruptor_footprint_burn_wrapper);
		pipelineRegistered = true;
	}

	/* Init fire footprint pool on first fire corruptor */
	if (theme == THEME_FIRE && !corruptorFootprintsInitialized) {
		SubScorch_init_footprints(corruptorFootprintBuf, CORRUPTOR_FOOTPRINT_MAX);
		corruptorFootprintsInitialized = true;
	}
}

void Corruptor_cleanup(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		if (entityRefs[i]) {
			entityRefs[i]->empty = true;
			entityRefs[i] = NULL;
		}
	}
	highestUsedIndex = 0;
	corruptorTypeId = -1;
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;

	Audio_unload_sample(&sampleDeath);
	Audio_unload_sample(&sampleRespawn);
	Audio_unload_sample(&sampleHit);
	SubEmp_cleanup_audio();
	SubHeatwave_cleanup_audio();
	SubScorch_cleanup_audio();

	if (corruptorFootprintsInitialized) {
		SubScorch_deactivate_all_footprints();
		corruptorFootprintsInitialized = false;
	}
}

Collision Corruptor_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	CorruptorState *c = (CorruptorState *)state;
	Collision collision = {false, false};

	if (!c->alive)
		return collision;

	Rectangle thisBB = collidable.boundingBox;
	Rectangle transformed = Collision_transform_bounding_box(placeable->position, thisBB);

	if (Collision_aabb_test(transformed, boundingBox)) {
		collision.collisionDetected = true;
		collision.solid = true;
		Enemy_break_cloak();
	}

	return collision;
}

void Corruptor_resolve(void *state, const Collision collision)
{
	(void)state;
	(void)collision;
}

void Corruptor_update(void *state, const PlaceableComponent *placeable, unsigned int ticks)
{
	CorruptorState *c = (CorruptorState *)state;
	int idx = (int)(c - corruptors);
	PlaceableComponent *pl = &placeables[idx];
	double dt = ticks / 1000.0;

	/* Spark decay — idx 0 only */
	if (idx == 0) {
		for (int si = 0; si < SPARK_POOL_SIZE; si++) {
			if (sparks[si].active) {
				sparks[si].ticksLeft -= ticks;
				if (sparks[si].ticksLeft <= 0)
					sparks[si].active = false;
			}
		}
	}

	/* Dormancy check */
	if (!SpatialGrid_is_active(pl->position.x, pl->position.y)) {
		if (c->aiState == CORRUPTOR_DEAD) {
			c->respawnTimer += ticks;
			if (c->respawnTimer >= RESPAWN_MS) {
				c->alive = true;
				c->hp = CORRUPTOR_HP;
				c->aiState = CORRUPTOR_IDLE;
				c->killedByPlayer = false;
				Burn_reset(&c->burn);
				if (c->theme == THEME_FIRE) {
					SubScorch_init(&c->scorchCore);
					SubHeatwave_init(&c->heatwaveCore);
					SubTemper_init(&c->temperCore);
				} else {
					SubSprint_init(&c->sprintCore);
					SubEmp_init(&c->empCore);
					SubResist_init(&c->resistCore);
				}
				c->hasResistTarget = false;
				EnemyFeedback_reset(&c->fb);
				Position oldPos = pl->position;
				pl->position = c->spawnPoint;
				pick_wander_target(c);
				SpatialGrid_update((EntityRef){ENTITY_CORRUPTOR, idx},
					oldPos.x, oldPos.y,
					c->spawnPoint.x, c->spawnPoint.y);
			}
		}
		return;
	}

	Position oldPos = pl->position;

	/* Tick feedback decay */
	EnemyFeedback_update(&c->fb, ticks);

	/* Tick burn DOT */
	if (c->alive && Burn_tick_enemy(&c->burn, &c->hp, ticks)) {
		c->alive = false;
		c->aiState = CORRUPTOR_DYING;
		c->deathTimer = 0;
		c->killedByPlayer = true;
		Audio_play_sample(&sampleDeath);
	}
	if (c->alive && Burn_is_active(&c->burn))
		Burn_register(&c->burn, pl->position);

	c->prevPosition = pl->position;
	c->sprintCore.active = false;
	currentUpdaterIdx = idx;

	/* Tick cores */
	if (c->theme == THEME_FIRE) {
		SubHeatwave_update(&c->heatwaveCore, SubHeatwave_get_config(), ticks);
		SubTemper_update(&c->temperCore, SubTemper_get_config(), ticks);
		if (SubScorch_is_active(&c->scorchCore))
			SubScorch_update(&c->scorchCore, SubScorch_get_config(), ticks);
	} else {
		SubEmp_update(&c->empCore, SubEmp_get_config(), ticks);
		SubResist_update(&c->resistCore, SubResist_get_config(), ticks);
	}

	/* Update resist/temper beam target — find nearest ally in range each frame */
	bool auraActive = (c->theme == THEME_FIRE) ? SubTemper_is_active(&c->temperCore) : c->resistCore.active;
	if (auraActive) {
		c->hasResistTarget = false;
		double bestDist = RESIST_RANGE + 1.0;
		int typeCount = EnemyRegistry_type_count();
		for (int t = 0; t < typeCount; t++) {
			const EnemyTypeCallbacks *et = EnemyRegistry_get_type(t);
			Position pos;
			int eidx;
			/* Wounded allies first */
			if (et->find_wounded && et->find_wounded(pl->position, RESIST_RANGE, 9999.0, &pos, &eidx)) {
				double dist = Enemy_distance_between(pl->position, pos);
				if (dist < bestDist) {
					bestDist = dist;
					c->resistBeamTarget = pos;
					c->hasResistTarget = true;
				}
			}
			/* Aggro'd allies as fallback — tracks even at full HP */
			if (et->find_aggro && et->find_aggro(pl->position, RESIST_RANGE, &pos)) {
				double dist = Enemy_distance_between(pl->position, pos);
				if (dist < bestDist) {
					bestDist = dist;
					c->resistBeamTarget = pos;
					c->hasResistTarget = true;
				}
			}
		}
	} else {
		c->hasResistTarget = false;
	}

	if (c->alive)
		Enemy_check_stealth_proximity(pl->position, c->facing);

	/* --- Check for incoming player damage --- */
	if (c->alive && c->aiState != CORRUPTOR_DYING) {
		Rectangle body = {-BODY_SIZE, BODY_SIZE, BODY_SIZE, -BODY_SIZE};
		Rectangle hitBox = Collision_transform_bounding_box(pl->position, body);

		PlayerDamageResult dmg = Enemy_check_player_damage(hitBox, pl->position);

		if (dmg.hit) {
			activate_spark(pl->position);
			double totalDmg = dmg.damage + (dmg.mine_hit ? dmg.mine_damage : 0.0);
			c->hp -= totalDmg;
			Burn_apply_from_hits(&c->burn, dmg.burn_hits);
			Audio_play_sample(&sampleHit);

			/* Getting hit while idle triggers awareness */
			if (c->aiState == CORRUPTOR_IDLE) {
				c->aiState = CORRUPTOR_SUPPORTING;
				Enemy_alert_nearby(pl->position, 1600.0);
			}
		}

		if (dmg.hit && c->hp <= 0.0) {
			c->alive = false;
			c->aiState = CORRUPTOR_DYING;
			c->deathTimer = 0;
			c->killedByPlayer = true;
			Audio_play_sample(&sampleDeath);
			Enemy_on_player_kill(&dmg);
		}
	}

	/* --- State machine --- */
	switch (c->aiState) {
	case CORRUPTOR_IDLE: {
		/* Wander near spawn */
		Enemy_move_toward(pl, c->wanderTarget, IDLE_DRIFT_SPEED, dt, WALL_CHECK_DIST);
		c->wanderTimer -= ticks;
		if (c->wanderTimer <= 0)
			pick_wander_target(c);

		/* Check for nearby wounded or aggro allies */
		if (c->aiState == CORRUPTOR_IDLE) {
			int typeCount = EnemyRegistry_type_count();
			for (int t = 0; t < typeCount && c->aiState == CORRUPTOR_IDLE; t++) {
				const EnemyTypeCallbacks *et = EnemyRegistry_get_type(t);
				Position pos;
				int eidx;
				if (et->find_wounded && et->find_wounded(pl->position, RESIST_RANGE, 50.0, &pos, &eidx))
					c->aiState = CORRUPTOR_SUPPORTING;
				else if (et->find_aggro && et->find_aggro(pl->position, 1600.0, &pos))
					c->aiState = CORRUPTOR_SUPPORTING;
			}
			if (c->aiState != CORRUPTOR_IDLE)
				Enemy_alert_nearby(pl->position, 1600.0);
		}

		/* Check aggro — player proximity */
		if (c->aiState == CORRUPTOR_IDLE) {
			Position shipPos = Ship_get_position();
			double dist = Enemy_distance_between(pl->position, shipPos);
			bool nearbyShot = Enemy_check_any_nearby(pl->position, 200.0);
			if (!Ship_is_destroyed() && !Sub_Stealth_is_stealthed() &&
				((dist < AGGRO_RANGE && Enemy_has_line_of_sight(pl->position, shipPos)) || nearbyShot)) {
				c->aiState = CORRUPTOR_SUPPORTING;
				Enemy_alert_nearby(pl->position, 1600.0);
			}
		}

		c->facing = Position_get_heading(pl->position, c->wanderTarget);
		break;
	}
	case CORRUPTOR_SUPPORTING: {
		Position shipPos = Ship_get_position();
		double shipDist = Enemy_distance_between(pl->position, shipPos);

		/* De-aggro check */
		if (Ship_is_destroyed() || Sub_Stealth_is_stealthed() || shipDist > DEAGGRO_RANGE ||
		    !Enemy_has_line_of_sight(pl->position, shipPos)) {
			c->aiState = CORRUPTOR_IDLE;
			pick_wander_target(c);
			break;
		}

		/* Priority 1: Sprint in for EMP attack */
		if (can_engage_player(c, pl->position)) {
			c->aiState = CORRUPTOR_SPRINTING;
			break;
		}

		/* Priority 2: Reactive resist/temper — protect ally that's taking damage */
		if (c->theme == THEME_FIRE)
			try_apply_temper(c, pl->position);
		else
			try_apply_resist(c, pl->position);

		/* Orbit at medium distance — stay near allies but not too close to player */
		double targetDist = SPRINT_ENGAGE_MIN;
		if (shipDist < targetDist - 100.0) {
			Enemy_move_away_from(pl, shipPos, NORMAL_SPEED, dt, WALL_CHECK_DIST);
		} else if (shipDist > targetDist + 100.0) {
			Enemy_move_toward(pl, shipPos, NORMAL_SPEED, dt, WALL_CHECK_DIST);
		}

		c->facing = Position_get_heading(pl->position, shipPos);
		break;
	}
	case CORRUPTOR_SPRINTING: {
		Position shipPos = Ship_get_position();
		double shipDist = Enemy_distance_between(pl->position, shipPos);

		if (c->theme == THEME_FIRE) {
			c->scorchCore.sprint.active = true;
			/* Deposit scorch footprints while sprinting */
			c->scorchCore.footprint_timer += ticks;
			if (c->scorchCore.footprint_timer >= SubScorch_get_config()->footprint_interval_ms) {
				c->scorchCore.footprint_timer = 0;
				SubScorch_spawn_footprint(&c->scorchCore, SubScorch_get_config(), pl->position);
			}
		} else {
			c->sprintCore.active = true;
		}

		/* LOS broken — abort charge */
		if (!Enemy_has_line_of_sight(pl->position, shipPos)) {
			if (c->theme == THEME_FIRE)
				c->scorchCore.sprint.active = false;
			else
				c->sprintCore.active = false;
			c->aiState = CORRUPTOR_SUPPORTING;
			break;
		}

		/* Charge at player */
		Enemy_move_toward(pl, shipPos, SPRINT_SPEED, dt, WALL_CHECK_DIST);
		c->facing = Position_get_heading(pl->position, shipPos);

		/* Within range — fire EMP/heatwave */
		if (shipDist <= SPRINT_FIRE_RANGE) {
			double cost = (c->theme == THEME_FIRE) ? HEATWAVE_FEEDBACK_COST : EMP_FEEDBACK_COST;
			double hpBefore = c->hp;
			if (EnemyFeedback_try_spend(&c->fb, cost, &c->hp)) {
				if (c->hp < hpBefore) {
					activate_spark(pl->position);
					Audio_play_sample(&sampleHit);
				}
				/* Feedback spillover self-kill */
				if (c->hp <= 0.0) {
					c->alive = false;
					c->aiState = CORRUPTOR_DYING;
					c->deathTimer = 0;
					c->killedByPlayer = false;
					break;
				}

				if (c->theme == THEME_FIRE) {
					/* Fire heatwave — feedback multiplier debuff */
					const SubHeatwaveConfig *hwcfg = SubHeatwave_get_config();
					PlayerStats_apply_feedback_multiplier(hwcfg->feedback_multiplier, hwcfg->debuff_duration_ms);
					SubHeatwave_try_activate(&c->heatwaveCore, hwcfg, pl->position);
				} else {
					/* Fire EMP at player */
					PlayerStats_apply_emp(EMP_PLAYER_DURATION_MS);
					SubEmp_try_activate(&c->empCore, SubEmp_get_config(), pl->position);
				}
				c->aiState = CORRUPTOR_EMP_FIRING;
			} else {
				/* Can't afford — abort to retreat */
				c->aiState = CORRUPTOR_RETREATING;
			}
		}

		/* Abort if player destroyed or stealthed mid-charge */
		if (Ship_is_destroyed() || Sub_Stealth_is_stealthed()) {
			if (c->theme == THEME_FIRE)
				c->scorchCore.sprint.active = false;
			else
				c->sprintCore.active = false;
			c->aiState = CORRUPTOR_IDLE;
			pick_wander_target(c);
		}
		break;
	}
	case CORRUPTOR_EMP_FIRING: {
		/* Brief pause while EMP/heatwave visual plays, then retreat */
		bool visualDone = (c->theme == THEME_FIRE) ?
			!SubHeatwave_is_active(&c->heatwaveCore) :
			!SubEmp_is_active(&c->empCore);
		if (visualDone)
			c->aiState = CORRUPTOR_RETREATING;
		break;
	}
	case CORRUPTOR_RETREATING: {
		Position shipPos = Ship_get_position();
		double shipDist = Enemy_distance_between(pl->position, shipPos);

		/* De-aggro */
		if (Ship_is_destroyed() || Sub_Stealth_is_stealthed() || shipDist > DEAGGRO_RANGE) {
			c->aiState = CORRUPTOR_IDLE;
			pick_wander_target(c);
			break;
		}

		/* Flee from player */
		Enemy_move_away_from(pl, shipPos, RETREAT_SPEED, dt, WALL_CHECK_DIST);
		c->facing = Position_get_heading(pl->position, shipPos);

		/* Safe distance — return to supporting */
		if (shipDist > RETREAT_SAFE_DIST)
			c->aiState = CORRUPTOR_SUPPORTING;
		break;
	}
	case CORRUPTOR_DYING:
		c->deathTimer += ticks;
		if (c->deathTimer >= DEATH_FLASH_MS) {
			c->aiState = CORRUPTOR_DEAD;
			c->respawnTimer = 0;

			if (c->killedByPlayer) {
				int count;
				const CarriedSubroutine *carried = Variant_get_carried(
					corruptorVariants, c->theme, corruptorCarried, 3, &count);
				Enemy_drop_fragments(pl->position, carried, count);
			}
		}
		break;

	case CORRUPTOR_DEAD:
		c->respawnTimer += ticks;
		if (c->respawnTimer >= RESPAWN_MS) {
			c->alive = true;
			c->hp = CORRUPTOR_HP;
			c->aiState = CORRUPTOR_IDLE;
			c->killedByPlayer = false;
			Burn_reset(&c->burn);
			if (c->theme == THEME_FIRE) {
				SubScorch_init(&c->scorchCore);
				SubHeatwave_init(&c->heatwaveCore);
				SubTemper_init(&c->temperCore);
			} else {
				SubSprint_init(&c->sprintCore);
				SubEmp_init(&c->empCore);
				SubResist_init(&c->resistCore);
			}
			c->hasResistTarget = false;
			EnemyFeedback_reset(&c->fb);
			pl->position = c->spawnPoint;
			pick_wander_target(c);
			Audio_play_sample(&sampleRespawn);
		}
		break;
	}

	/* Gravity well pull */
	if (c->alive && c->aiState != CORRUPTOR_DYING && c->aiState != CORRUPTOR_DEAD)
		Enemy_apply_gravity(pl, dt);

	/* Update spatial grid */
	SpatialGrid_update((EntityRef){ENTITY_CORRUPTOR, idx},
		oldPos.x, oldPos.y, pl->position.x, pl->position.y);
}

static void render_circle(Position pos, float radius, float thickness,
	float r, float g, float b, float a)
{
	int segments = 16;
	for (int i = 0; i < segments; i++) {
		float a0 = (float)i / (float)segments * 2.0f * PI;
		float a1 = (float)(i + 1) / (float)segments * 2.0f * PI;
		float x0 = (float)pos.x + cosf(a0) * radius;
		float y0 = (float)pos.y + sinf(a0) * radius;
		float x1 = (float)pos.x + cosf(a1) * radius;
		float y1 = (float)pos.y + sinf(a1) * radius;
		Render_thick_line(x0, y0, x1, y1, thickness, r, g, b, a);
	}
}

void Corruptor_render(const void *state, const PlaceableComponent *placeable)
{
	CorruptorState *c = (CorruptorState *)state;
	int idx = (int)(c - corruptors);

	if (c->aiState == CORRUPTOR_DEAD)
		return;

	/* Death flash */
	if (c->aiState == CORRUPTOR_DYING) {
		Enemy_render_death_flash(placeable, (float)c->deathTimer, (float)DEATH_FLASH_MS);
		return;
	}

	float brightness = (c->aiState == CORRUPTOR_IDLE) ? 0.7f : 1.0f;
	const ColorFloat *baseColor = (c->aiState == CORRUPTOR_IDLE) ? &colorBody : &colorAggro;
	ColorFloat bodyColor = {baseColor->red * brightness, baseColor->green * brightness, baseColor->blue * brightness, baseColor->alpha};

	/* Sprint motion trail */
	bool sprinting = (c->theme == THEME_FIRE) ? c->scorchCore.sprint.active : c->sprintCore.active;
	if (sprinting) {
		double dx = placeable->position.x - c->prevPosition.x;
		double dy = placeable->position.y - c->prevPosition.y;
		for (int g = SPRINT_TRAIL_GHOSTS; g >= 1; g--) {
			float t = (float)g / (float)(SPRINT_TRAIL_GHOSTS + 1);
			Position ghost;
			ghost.x = placeable->position.x - dx * SPRINT_TRAIL_LENGTH * t;
			ghost.y = placeable->position.y - dy * SPRINT_TRAIL_LENGTH * t;
			float alpha = (1.0f - t) * 0.35f;
			render_circle(ghost, BODY_SIZE, 1.5f,
				bodyColor.red, bodyColor.green, bodyColor.blue, alpha);
		}
	}

	/* Body circle */
	render_circle(placeable->position, BODY_SIZE, 2.0f,
		bodyColor.red, bodyColor.green, bodyColor.blue, bodyColor.alpha);

	/* Center dot */
	Render_point(&placeable->position, 4.0, &bodyColor);

	/* Resist/temper aura + beam to protected ally */
	if (c->theme == THEME_FIRE) {
		SubTemper_render_ring(&c->temperCore, SubTemper_get_config(), placeable->position);
		if (c->hasResistTarget)
			SubTemper_render_beam(&c->temperCore, SubTemper_get_config(), placeable->position, c->resistBeamTarget);
		SubHeatwave_render_ring(&c->heatwaveCore, SubHeatwave_get_config());
	} else {
		SubResist_render_ring(&c->resistCore, SubResist_get_config(), placeable->position);
		if (c->hasResistTarget)
			SubResist_render_beam(&c->resistCore, SubResist_get_config(), placeable->position, c->resistBeamTarget);
		SubEmp_render_ring(&c->empCore, SubEmp_get_config());
	}

	/* Sparks (from idx 0 only) */
	if (idx == 0) {
		for (int si = 0; si < SPARK_POOL_SIZE; si++) {
			if (sparks[si].active) {
				Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
					SPARK_DURATION, SPARK_SIZE, false,
					1.0f, 0.9f, 0.0f);
			}
		}
	}
}

static void corruptor_render_bloom(const void *state, const PlaceableComponent *placeable)
{
	const CorruptorState *c = (const CorruptorState *)state;

	if (c->aiState == CORRUPTOR_DEAD)
		return;

	if (c->aiState == CORRUPTOR_DYING) {
		Enemy_render_death_flash(placeable, (float)c->deathTimer, (float)DEATH_FLASH_MS);
		return;
	}

	const ColorFloat *baseColor = (c->aiState == CORRUPTOR_IDLE) ? &colorBody : &colorAggro;
	ColorFloat bloomColor = Variant_get_color(corruptorVariants, c->theme, baseColor, 1.0f);
	const ColorFloat *bodyColor = &bloomColor;

	/* Sprint trail bloom */
	bool isSprinting = (c->theme == THEME_FIRE) ? c->scorchCore.sprint.active : c->sprintCore.active;
	if (isSprinting) {
		double dx = placeable->position.x - c->prevPosition.x;
		double dy = placeable->position.y - c->prevPosition.y;
		for (int g = SPRINT_TRAIL_GHOSTS; g >= 1; g--) {
			float t = (float)g / (float)(SPRINT_TRAIL_GHOSTS + 1);
			Position ghost;
			ghost.x = placeable->position.x - dx * SPRINT_TRAIL_LENGTH * t;
			ghost.y = placeable->position.y - dy * SPRINT_TRAIL_LENGTH * t;
			float alpha = (1.0f - t) * 0.25f;
			Render_point(&ghost, 6.0f, &(ColorFloat){bodyColor->red, bodyColor->green, bodyColor->blue, alpha});
		}
	}

	/* Body glow */
	Render_point(&placeable->position, 6.0, bodyColor);

	/* Resist/temper aura bloom + beam bloom */
	if (c->theme == THEME_FIRE) {
		SubTemper_render_bloom(&c->temperCore, SubTemper_get_config(), placeable->position);
		if (c->hasResistTarget)
			SubTemper_render_beam_bloom(&c->temperCore, SubTemper_get_config(), placeable->position, c->resistBeamTarget);
		SubHeatwave_render_bloom(&c->heatwaveCore, SubHeatwave_get_config());
	} else {
		SubResist_render_bloom(&c->resistCore, SubResist_get_config(), placeable->position);
		if (c->hasResistTarget)
			SubResist_render_beam_bloom(&c->resistCore, SubResist_get_config(), placeable->position, c->resistBeamTarget);
		SubEmp_render_bloom(&c->empCore, SubEmp_get_config());
	}
}

static void corruptor_render_light(const void *state, const PlaceableComponent *placeable)
{
	const CorruptorState *c = (const CorruptorState *)state;

	if (!c->alive || c->aiState == CORRUPTOR_DYING || c->aiState == CORRUPTOR_DEAD)
		return;

	/* Body light */
	const ColorFloat *lightColor = (c->theme == THEME_FIRE) ?
		&(ColorFloat){1.0f, 0.5f, 0.05f, 1.0f} : &colorBody;
	Render_point(&placeable->position, 4.0, lightColor);

	/* EMP/heatwave flash — bright light during firing */
	if (c->theme == THEME_FIRE)
		SubHeatwave_render_light(&c->heatwaveCore, SubHeatwave_get_config());
	else
		SubEmp_render_light(&c->empCore, SubEmp_get_config());
}

static void corruptor_render_pool_bloom(void)
{
	/* Spark bloom */
	for (int si = 0; si < SPARK_POOL_SIZE; si++) {
		if (sparks[si].active) {
			Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
				SPARK_DURATION, SPARK_SIZE, false,
				1.0f, 0.9f, 0.0f);
		}
	}
}

static void corruptor_footprint_burn_wrapper(const unsigned int ticks)
{
	(void)ticks;
	Corruptor_check_footprint_burn_player();
}

void Corruptor_deaggro_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		CorruptorState *c = &corruptors[i];
		if (c->aiState == CORRUPTOR_SUPPORTING || c->aiState == CORRUPTOR_SPRINTING ||
			c->aiState == CORRUPTOR_EMP_FIRING || c->aiState == CORRUPTOR_RETREATING) {
			c->aiState = CORRUPTOR_IDLE;
			if (c->theme == THEME_FIRE) {
				c->scorchCore.sprint.active = false;
				c->heatwaveCore.visualActive = false;
			} else {
				c->sprintCore.active = false;
				c->empCore.visualActive = false;
			}
			pick_wander_target(c);
		}
	}
}

void Corruptor_reset_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		CorruptorState *c = &corruptors[i];
		c->alive = true;
		c->hp = CORRUPTOR_HP;
		c->aiState = CORRUPTOR_IDLE;
		c->killedByPlayer = false;
		Burn_reset(&c->burn);
		if (c->theme == THEME_FIRE) {
			SubScorch_init(&c->scorchCore);
			SubHeatwave_init(&c->heatwaveCore);
			SubTemper_init(&c->temperCore);
		} else {
			SubSprint_init(&c->sprintCore);
			SubEmp_init(&c->empCore);
			SubResist_init(&c->resistCore);
		}
		c->hasResistTarget = false;
		c->deathTimer = 0;
		c->respawnTimer = 0;
		EnemyFeedback_reset(&c->fb);
		placeables[i].position = c->spawnPoint;
		c->prevPosition = c->spawnPoint;
		pick_wander_target(c);
	}
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;

	if (corruptorFootprintsInitialized)
		SubScorch_deactivate_all_footprints();
}

bool Corruptor_is_resist_buffing(Position pos)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		CorruptorState *c = &corruptors[i];
		if (!c->alive)
			continue;
		bool hasAura = (c->theme == THEME_FIRE) ?
			SubTemper_is_active(&c->temperCore) : c->resistCore.active;
		if (!hasAura)
			continue;
		double dist = Enemy_distance_between(placeables[i].position, pos);
		if (dist < RESIST_RANGE)
			return true;
	}
	return false;
}

bool Corruptor_find_wounded(Position from, double range, double hp_threshold, Position *out_pos, int *out_index)
{
	double bestDamage = 0.0;
	int bestIdx = -1;

	for (int i = 0; i < highestUsedIndex; i++) {
		if (i == currentUpdaterIdx)
			continue;
		CorruptorState *c = &corruptors[i];
		if (!c->alive || c->aiState == CORRUPTOR_DYING || c->aiState == CORRUPTOR_DEAD)
			continue;
		if (c->hp >= hp_threshold)
			continue;
		double missing = CORRUPTOR_HP - c->hp;
		if (missing <= 0.0)
			continue;
		double dist = Enemy_distance_between(from, placeables[i].position);
		if (dist > range)
			continue;
		if (missing > bestDamage) {
			bestDamage = missing;
			bestIdx = i;
		}
	}

	if (bestIdx >= 0) {
		*out_pos = placeables[bestIdx].position;
		*out_index = bestIdx;
		return true;
	}
	return false;
}

bool Corruptor_find_aggro(Position from, double range, Position *out_pos)
{
	double bestDist = range + 1.0;
	int bestIdx = -1;

	for (int i = 0; i < highestUsedIndex; i++) {
		if (i == currentUpdaterIdx)
			continue;
		CorruptorState *c = &corruptors[i];
		if (!c->alive)
			continue;
		if (c->aiState != CORRUPTOR_SUPPORTING && c->aiState != CORRUPTOR_SPRINTING &&
			c->aiState != CORRUPTOR_EMP_FIRING && c->aiState != CORRUPTOR_RETREATING)
			continue;
		double dist = Enemy_distance_between(from, placeables[i].position);
		if (dist > range)
			continue;
		if (dist < bestDist) {
			bestDist = dist;
			bestIdx = i;
		}
	}

	if (bestIdx >= 0) {
		*out_pos = placeables[bestIdx].position;
		return true;
	}
	return false;
}

void Corruptor_heal(int index, double amount)
{
	if (index < 0 || index >= highestUsedIndex)
		return;
	CorruptorState *c = &corruptors[index];
	if (!c->alive)
		return;
	c->hp += amount;
	if (c->hp > CORRUPTOR_HP)
		c->hp = CORRUPTOR_HP;
}

void Corruptor_alert_nearby(Position origin, double radius, Position threat)
{
	(void)threat;
	for (int i = 0; i < highestUsedIndex; i++) {
		CorruptorState *c = &corruptors[i];
		if (!c->alive || c->aiState != CORRUPTOR_IDLE)
			continue;
		if (Enemy_distance_between(placeables[i].position, origin) < radius)
			c->aiState = CORRUPTOR_SUPPORTING;
	}
}

void Corruptor_apply_emp(Position center, double half_size, unsigned int duration_ms)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		CorruptorState *c = &corruptors[i];
		if (!c->alive || c->aiState == CORRUPTOR_DYING || c->aiState == CORRUPTOR_DEAD)
			continue;
		double dx = placeables[i].position.x - center.x;
		double dy = placeables[i].position.y - center.y;
		if (dx < -half_size || dx > half_size || dy < -half_size || dy > half_size)
			continue;
		EnemyFeedback_apply_emp(&c->fb, duration_ms);
	}
}

void Corruptor_cleanse_burn(Position center, double radius, int immunity_ms)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		CorruptorState *c = &corruptors[i];
		if (!c->alive || c->aiState == CORRUPTOR_DYING || c->aiState == CORRUPTOR_DEAD)
			continue;
		double dist = Enemy_distance_between(placeables[i].position, center);
		if (dist <= radius)
			Burn_grant_immunity(&c->burn, immunity_ms);
	}
}

void Corruptor_apply_burn(Position center, double radius, int duration_ms)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		CorruptorState *c = &corruptors[i];
		if (!c->alive || c->aiState == CORRUPTOR_DYING || c->aiState == CORRUPTOR_DEAD)
			continue;
		double dist = Enemy_distance_between(placeables[i].position, center);
		if (dist <= radius)
			Burn_apply(&c->burn, duration_ms);
	}
}

void Corruptor_apply_heatwave(Position center, double half_size, double multiplier, unsigned int duration_ms)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		CorruptorState *c = &corruptors[i];
		if (!c->alive || c->aiState == CORRUPTOR_DYING || c->aiState == CORRUPTOR_DEAD)
			continue;
		double dx = placeables[i].position.x - center.x;
		double dy = placeables[i].position.y - center.y;
		if (dx < -half_size || dx > half_size || dy < -half_size || dy > half_size)
			continue;
		EnemyFeedback_apply_heatwave(&c->fb, multiplier, duration_ms);
	}
}

/* --- Scorch footprint public API --- */

void Corruptor_update_footprints(unsigned int ticks)
{
	if (!corruptorFootprintsInitialized)
		return;
	SubScorch_update_footprints(SubScorch_get_config(), ticks);
}

void Corruptor_check_footprint_burn_player(void)
{
	if (!corruptorFootprintsInitialized || Ship_is_destroyed())
		return;

	Position shipPos = Ship_get_position();
	Rectangle shipBB = {-SHIP_BB_HALF_SIZE, SHIP_BB_HALF_SIZE, SHIP_BB_HALF_SIZE, -SHIP_BB_HALF_SIZE};
	Rectangle shipWorld = Collision_transform_bounding_box(shipPos, shipBB);
	int hits = SubScorch_check_footprint_burn(SubScorch_get_config(), shipWorld);
	for (int i = 0; i < hits; i++)
		Burn_apply_to_player(BURN_DURATION_MS);
}

void Corruptor_render_footprints(void)
{
	if (!corruptorFootprintsInitialized)
		return;
	SubScorch_render_footprints(SubScorch_get_config());
}

void Corruptor_render_footprint_bloom_source(void)
{
	if (!corruptorFootprintsInitialized)
		return;
	SubScorch_render_footprints_bloom(SubScorch_get_config());
}

void Corruptor_render_footprint_light_source(void)
{
	if (!corruptorFootprintsInitialized)
		return;
	SubScorch_render_footprints_light();
}

int Corruptor_get_count(void)
{
	return highestUsedIndex;
}
