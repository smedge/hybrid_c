#include "corruptor.h"
#include "enemy_util.h"
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
#include "view.h"
#include "render.h"
#include "color.h"
#include "audio.h"
#include "map.h"
#include "spatial_grid.h"

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

static const SubEmpConfig corruptorEmpCfg = {
	.cooldown_ms = 30000,
	.visual_ms = 167,
	.half_size = 400.0f,
	.ring_max_radius = 400.0f,
	.inner_ring_ratio = 0.7f,
	.segments = 16,
	.outer_r = 0.4f, .outer_g = 0.7f, .outer_b = 1.0f,
	.outer_thickness = 2.5f,
	.inner_r = 0.8f, .inner_g = 0.9f, .inner_b = 1.0f,
	.inner_thickness = 1.5f,
	.inner_alpha_mult = 0.6f,
};

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

static const CarriedSubroutine corruptorCarriedFull[] = {
	{ SUB_ID_SPRINT, FRAG_TYPE_CORRUPTOR },
	{ SUB_ID_EMP,    FRAG_TYPE_CORRUPTOR },
	{ SUB_ID_RESIST, FRAG_TYPE_CORRUPTOR },
};

static const CarriedSubroutine corruptorCarriedNoEmp[] = {
	{ SUB_ID_SPRINT, FRAG_TYPE_CORRUPTOR },
	{ SUB_ID_RESIST, FRAG_TYPE_CORRUPTOR },
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

	/* Death/respawn */
	int deathTimer;
	int respawnTimer;

	/* Feedback */
	EnemyFeedback fb;
} CorruptorState;

/* Shared singleton components */
static RenderableComponent renderable = {Corruptor_render};
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
	if (c->empCore.cooldownMs > 0)
		return false;
	/* Stay near allies while actively buffing them with resist */
	if (c->resistCore.active)
		return false;

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

	/* Check if there's a threatened ally nearby — or use self */
	int typeCount = EnemyRegistry_type_count();
	bool hasAlly = false;
	for (int t = 0; t < typeCount; t++) {
		const EnemyTypeCallbacks *et = EnemyRegistry_get_type(t);
		Position pos;
		if (et->find_aggro && et->find_aggro(myPos, RESIST_RANGE, &pos)) {
			hasAlly = true;
			break;
		}
	}

	/* Only spend feedback if there's someone to protect (including self in combat) */
	if (!hasAlly && c->aiState != CORRUPTOR_SUPPORTING)
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

/* ---- Public API ---- */

void Corruptor_initialize(Position position)
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
	c->prevPosition = position;
	SubSprint_init(&c->sprintCore);
	SubEmp_init(&c->empCore);
	SubResist_init(&c->resistCore);
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
		EnemyTypeCallbacks cb = {Corruptor_find_wounded, Corruptor_find_aggro,
			Corruptor_heal, Corruptor_alert_nearby, Corruptor_apply_emp};
		corruptorTypeId = EnemyRegistry_register(cb);
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
		Sub_Stealth_break();
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
				SubSprint_init(&c->sprintCore);
				SubEmp_init(&c->empCore);
				SubResist_init(&c->resistCore);
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

	c->prevPosition = pl->position;
	c->sprintCore.active = false;
	currentUpdaterIdx = idx;

	/* Tick cores */
	SubEmp_update(&c->empCore, &corruptorEmpCfg, ticks);
	SubResist_update(&c->resistCore, SubResist_get_config(), ticks);

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
		if (Ship_is_destroyed() || Sub_Stealth_is_stealthed() || shipDist > DEAGGRO_RANGE) {
			c->aiState = CORRUPTOR_IDLE;
			pick_wander_target(c);
			break;
		}

		/* Try to apply resist aura to protect allies (or self) */
		try_apply_resist(c, pl->position);

		/* Check if we should sprint in for EMP attack */
		if (can_engage_player(c, pl->position)) {
			/* Committed charge — transition to sprinting */
			c->aiState = CORRUPTOR_SPRINTING;
			break;
		}

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

		c->sprintCore.active = true;

		/* Charge at player */
		Enemy_move_toward(pl, shipPos, SPRINT_SPEED, dt, WALL_CHECK_DIST);
		c->facing = Position_get_heading(pl->position, shipPos);

		/* Within range — fire EMP */
		if (shipDist <= SPRINT_FIRE_RANGE) {
			/* Try to spend feedback for EMP */
			double hpBefore = c->hp;
			if (EnemyFeedback_try_spend(&c->fb, EMP_FEEDBACK_COST, &c->hp)) {
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

				/* Fire EMP at player */
				PlayerStats_apply_emp(EMP_PLAYER_DURATION_MS);
				SubEmp_try_activate(&c->empCore, &corruptorEmpCfg, pl->position);
				c->aiState = CORRUPTOR_EMP_FIRING;
			} else {
				/* Can't afford — abort to retreat */
				c->aiState = CORRUPTOR_RETREATING;
			}
		}

		/* Abort if player destroyed or stealthed mid-charge */
		if (Ship_is_destroyed() || Sub_Stealth_is_stealthed()) {
			c->aiState = CORRUPTOR_IDLE;
			pick_wander_target(c);
		}
		break;
	}
	case CORRUPTOR_EMP_FIRING: {
		/* Brief pause while EMP visual plays, then retreat */
		if (!SubEmp_is_active(&c->empCore)) {
			c->aiState = CORRUPTOR_RETREATING;
		}
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

			/* Drop fragment — special gating for EMP */
			if (c->killedByPlayer) {
				bool canDropEmp = Progression_is_unlocked(SUB_ID_SPRINT) &&
					Progression_is_unlocked(SUB_ID_RESIST);
				if (canDropEmp) {
					/* 50% chance to drop EMP fragment (only locked sub remaining) */
					if (rand() % 2 == 0)
						Enemy_drop_fragments(pl->position, corruptorCarriedFull, 3);
					else
						Enemy_drop_fragments(pl->position, corruptorCarriedNoEmp, 2);
				} else {
					Enemy_drop_fragments(pl->position, corruptorCarriedNoEmp, 2);
				}
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
			SubSprint_init(&c->sprintCore);
			SubEmp_init(&c->empCore);
			SubResist_init(&c->resistCore);
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

	const ColorFloat *bodyColor = (c->aiState == CORRUPTOR_IDLE) ? &colorBody : &colorAggro;

	/* Sprint motion trail */
	if (c->sprintCore.active) {
		double dx = placeable->position.x - c->prevPosition.x;
		double dy = placeable->position.y - c->prevPosition.y;
		for (int g = SPRINT_TRAIL_GHOSTS; g >= 1; g--) {
			float t = (float)g / (float)(SPRINT_TRAIL_GHOSTS + 1);
			Position ghost;
			ghost.x = placeable->position.x - dx * SPRINT_TRAIL_LENGTH * t;
			ghost.y = placeable->position.y - dy * SPRINT_TRAIL_LENGTH * t;
			float alpha = (1.0f - t) * 0.35f;
			render_circle(ghost, BODY_SIZE, 1.5f,
				bodyColor->red, bodyColor->green, bodyColor->blue, alpha);
		}
	}

	/* Body circle */
	render_circle(placeable->position, BODY_SIZE, 2.0f,
		bodyColor->red, bodyColor->green, bodyColor->blue, bodyColor->alpha);

	/* Center dot */
	Render_point(&placeable->position, 4.0, bodyColor);

	/* Resist aura */
	SubResist_render_ring(&c->resistCore, SubResist_get_config(), placeable->position);

	/* EMP visual — expanding ring */
	SubEmp_render_ring(&c->empCore, &corruptorEmpCfg);

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

void Corruptor_render_bloom_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		CorruptorState *c = &corruptors[i];
		PlaceableComponent *pl = &placeables[i];

		if (c->aiState == CORRUPTOR_DEAD)
			continue;

		if (c->aiState == CORRUPTOR_DYING) {
			Enemy_render_death_flash(pl, (float)c->deathTimer, (float)DEATH_FLASH_MS);
			continue;
		}

		const ColorFloat *bodyColor = (c->aiState == CORRUPTOR_IDLE) ? &colorBody : &colorAggro;

		/* Sprint trail bloom */
		if (c->sprintCore.active) {
			double dx = pl->position.x - c->prevPosition.x;
			double dy = pl->position.y - c->prevPosition.y;
			for (int g = SPRINT_TRAIL_GHOSTS; g >= 1; g--) {
				float t = (float)g / (float)(SPRINT_TRAIL_GHOSTS + 1);
				Position ghost;
				ghost.x = pl->position.x - dx * SPRINT_TRAIL_LENGTH * t;
				ghost.y = pl->position.y - dy * SPRINT_TRAIL_LENGTH * t;
				float alpha = (1.0f - t) * 0.25f;
				Render_point(&ghost, 6.0f, &(ColorFloat){bodyColor->red, bodyColor->green, bodyColor->blue, alpha});
			}
		}

		/* Body glow */
		Render_point(&pl->position, 6.0, bodyColor);

		/* Resist aura bloom */
		SubResist_render_bloom(&c->resistCore, SubResist_get_config(), pl->position);

		/* EMP ring bloom */
		SubEmp_render_bloom(&c->empCore, &corruptorEmpCfg);
	}

	/* Spark bloom */
	for (int si = 0; si < SPARK_POOL_SIZE; si++) {
		if (sparks[si].active) {
			Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
				SPARK_DURATION, SPARK_SIZE, false,
				1.0f, 0.9f, 0.0f);
		}
	}
}

void Corruptor_render_light_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		CorruptorState *c = &corruptors[i];
		PlaceableComponent *pl = &placeables[i];

		if (!c->alive || c->aiState == CORRUPTOR_DYING || c->aiState == CORRUPTOR_DEAD)
			continue;

		/* Body light */
		Render_point(&pl->position, 4.0, &colorBody);

		/* EMP flash — bright light during firing */
		SubEmp_render_light(&c->empCore, &corruptorEmpCfg);
	}
}

void Corruptor_deaggro_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		CorruptorState *c = &corruptors[i];
		if (c->aiState == CORRUPTOR_SUPPORTING || c->aiState == CORRUPTOR_SPRINTING ||
			c->aiState == CORRUPTOR_EMP_FIRING || c->aiState == CORRUPTOR_RETREATING) {
			c->aiState = CORRUPTOR_IDLE;
			c->sprintCore.active = false;
			c->empCore.visualActive = false;
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
		SubSprint_init(&c->sprintCore);
		SubEmp_init(&c->empCore);
		SubResist_init(&c->resistCore);
		c->deathTimer = 0;
		c->respawnTimer = 0;
		EnemyFeedback_reset(&c->fb);
		placeables[i].position = c->spawnPoint;
		c->prevPosition = c->spawnPoint;
		pick_wander_target(c);
	}
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;
}

bool Corruptor_is_resist_buffing(Position pos)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		CorruptorState *c = &corruptors[i];
		if (!c->alive || !c->resistCore.active)
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

int Corruptor_get_count(void)
{
	return highestUsedIndex;
}
