#include "defender.h"
#include "sub_shield_core.h"
#include "sub_heal_core.h"
#include "enemy_util.h"
#include "enemy_registry.h"
#include "fragment.h"
#include "progression.h"
#include "player_stats.h"
#include "ship.h"
#include "sub_stealth.h"
#include "view.h"
#include "render.h"
#include "color.h"
#include "audio.h"
#include "map.h"

#include <math.h>
#include <stdlib.h>
#include <SDL2/SDL_mixer.h>

#define DEFENDER_COUNT 64
#define DEFENDER_HP 80.0
#define NORMAL_SPEED 250.0
#define FLEE_SPEED 400.0
#define BOOST_MULTIPLIER 2.0
#define AGGRO_RANGE 1200.0
#define DEAGGRO_RANGE 3200.0
#define HEAL_RANGE 800.0
#define FLEE_TRIGGER_RANGE 400.0
#define FLEE_SAFE_RANGE 600.0
#define IDLE_DRIFT_RADIUS 400.0
#define IDLE_DRIFT_SPEED 80.0
#define IDLE_WANDER_INTERVAL 2000
#define WALL_CHECK_DIST 50.0

static const SubShieldConfig defShieldCfg = {
	.duration_ms = 10000,
	.cooldown_ms = 30000,
	.break_grace_ms = 200,
	.ring_radius = 18.0f,
	.ring_thickness = 1.5f,
	.ring_segments = 6,
	.color_r = 0.6f, .color_g = 0.9f, .color_b = 1.0f,
	.pulse_speed = 5.0f,
	.pulse_alpha_min = 0.6f,
	.pulse_alpha_range = 0.4f,
	.radius_pulse_amount = 0.0f,
	.bloom_thickness = 2.0f,
	.bloom_alpha_min = 0.5f,
	.bloom_alpha_range = 0.0f,
	.light_radius = 0.0f,
	.light_segments = 0,
};

static const SubHealConfig defHealCfg = {
	.heal_amount = 50.0,
	.cooldown_ms = 4000,
	.beam_duration_ms = 200,
	.beam_thickness = 2.5f,
	.beam_color_r = 0.3f, .beam_color_g = 0.7f, .beam_color_b = 1.0f,
	.bloom_beam_thickness = 3.0f,
};

static const CarriedSubroutine defenderCarried[] = {
	{ SUB_ID_MEND, FRAG_TYPE_MEND },
	{ SUB_ID_AEGIS, FRAG_TYPE_AEGIS },
};

#define DEATH_FLASH_MS 200
#define RESPAWN_MS 30000

#define BODY_SIZE 10.0
#define PROTECT_RADIUS 100.0
#define SHIELD_CHASE_SPEED 800.0
#define AEGIS_STANDOFF_DIST 500.0
#define RELAY_STANDOFF_DIST 150.0
#define BOOST_TRAIL_GHOSTS 12
#define BOOST_TRAIL_LENGTH 3.0

typedef enum {
	DEFENDER_IDLE,
	DEFENDER_SUPPORTING,
	DEFENDER_FLEEING,
	DEFENDER_DYING,
	DEFENDER_DEAD
} DefenderAIState;

typedef struct {
	bool alive;
	double hp;
	DefenderAIState aiState;
	Position spawnPoint;
	double facing;
	bool killedByPlayer;

	/* Idle wander */
	Position wanderTarget;
	int wanderTimer;

	/* Healing */
	SubHealCore healCore;

	/* Aegis (enemy self-shield) */
	SubShieldCore shieldCore;

	/* Boost */
	bool boosting;
	Position prevPosition;

	/* Frame-level snapshot so protection survives same-frame mine break */
	bool aegisWasActive;

	/* Target deconfliction — set each frame during SUPPORTING */
	bool hasAssignment;
	Position assignedTarget;

	/* Death/respawn */
	int deathTimer;
	int respawnTimer;
} DefenderState;

/* Shared singleton components */
static RenderableComponent renderable = {Defender_render};
static CollidableComponent collidable = {{-BODY_SIZE, BODY_SIZE, BODY_SIZE, -BODY_SIZE},
										  true,
										  COLLISION_LAYER_ENEMY,
										  COLLISION_LAYER_PLAYER,
										  Defender_collide, Defender_resolve};
static AIUpdatableComponent updatable = {Defender_update};

/* Colors */
static const ColorFloat colorBody   = {0.3f, 0.7f, 1.0f, 1.0f};
static const ColorFloat colorAggro  = {0.5f, 0.8f, 1.0f, 1.0f};

/* State arrays */
static DefenderState defenders[DEFENDER_COUNT];
static PlaceableComponent placeables[DEFENDER_COUNT];
static Entity *entityRefs[DEFENDER_COUNT];
static int highestUsedIndex = 0;
static int defenderTypeId = -1;

/* Self-exclusion: skip this index in find_wounded/find_aggro to prevent self-targeting */
static int currentUpdaterIdx = -1;

/* Sparks */
#define SPARK_DURATION 80
#define SPARK_SIZE 12.0
#define SPARK_POOL_SIZE 8
static struct {
	bool active;
	bool shielded;
	Position position;
	int ticksLeft;
} sparks[SPARK_POOL_SIZE];

static void activate_spark(Position pos, bool shielded) {
	int slot = 0;
	for (int i = 0; i < SPARK_POOL_SIZE; i++) {
		if (!sparks[i].active) { slot = i; break; }
		if (sparks[i].ticksLeft < sparks[slot].ticksLeft) slot = i;
	}
	sparks[slot].active = true;
	sparks[slot].shielded = shielded;
	sparks[slot].position = pos;
	sparks[slot].ticksLeft = SPARK_DURATION;
}

/* Audio — entity sounds only (damage/death/respawn) */
static Mix_Chunk *sampleDeath = 0;
static Mix_Chunk *sampleRespawn = 0;
static Mix_Chunk *sampleHit = 0;

/* Helpers */
static void pick_wander_target(DefenderState *d)
{
	Enemy_pick_wander_target(d->spawnPoint, IDLE_DRIFT_RADIUS, IDLE_WANDER_INTERVAL,
		&d->wanderTarget, &d->wanderTimer);
}

static bool another_defender_shielding(int selfIdx, Position targetPos)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		if (i == selfIdx)
			continue;
		DefenderState *other = &defenders[i];
		if (!other->alive || !SubShield_is_active(&other->shieldCore))
			continue;
		double dist = Enemy_distance_between(placeables[i].position, targetPos);
		if (dist < PROTECT_RADIUS)
			return true;
	}
	return false;
}

static bool another_defender_assigned(int selfIdx, Position targetPos)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		if (i == selfIdx)
			continue;
		DefenderState *other = &defenders[i];
		if (!other->alive || !other->hasAssignment)
			continue;
		if (other->aiState != DEFENDER_SUPPORTING)
			continue;
		double dist = Enemy_distance_between(other->assignedTarget, targetPos);
		if (dist < PROTECT_RADIUS * 2)
			return true;
	}
	return false;
}

static bool try_heal_ally(DefenderState *d, PlaceableComponent *pl)
{
	if (!SubHeal_is_ready(&d->healCore))
		return false;

	int typeCount = EnemyRegistry_type_count();
	int bestType = -1;
	Position bestPos = {0, 0};
	int bestIdx = -1;
	double bestDist = 99999.0;

	/* Two-pass: prefer damage-dealing types, fallback to fellow defenders */
	for (int priority = 0; priority < 2; priority++) {
		for (int t = 0; t < typeCount; t++) {
			bool isDefType = (defenderTypeId >= 0 && t == defenderTypeId);
			if ((priority == 0 && isDefType) || (priority == 1 && !isDefType))
				continue;
			const EnemyTypeCallbacks *et = EnemyRegistry_get_type(t);
			if (!et->find_wounded || !et->heal)
				continue;
			Position pos;
			int eidx;
			if (et->find_wounded(pl->position, HEAL_RANGE, 50.0, &pos, &eidx)) {
				double dist = Enemy_distance_between(pl->position, pos);
				if (dist < bestDist) {
					bestDist = dist;
					bestPos = pos;
					bestType = t;
					bestIdx = eidx;
				}
			}
		}
		if (bestType >= 0)
			break;
	}

	if (bestType < 0)
		return false;

	EnemyRegistry_heal(bestType, bestIdx, defHealCfg.heal_amount);

	SubHeal_try_activate(&d->healCore, &defHealCfg, pl->position, bestPos);
	return true;
}

static void activate_aegis(DefenderState *d)
{
	SubShield_try_activate(&d->shieldCore, &defShieldCfg);
}

/* ---- Public API ---- */

void Defender_initialize(Position position)
{
	if (highestUsedIndex >= DEFENDER_COUNT) {
		printf("FATAL ERROR: Too many defender entities.\n");
		return;
	}

	int idx = highestUsedIndex;
	DefenderState *d = &defenders[idx];

	d->alive = true;
	d->hp = DEFENDER_HP;
	d->aiState = DEFENDER_IDLE;
	d->spawnPoint = position;
	d->facing = 0.0;
	d->killedByPlayer = false;
	SubHeal_init(&d->healCore);
	SubShield_init(&d->shieldCore);
	d->boosting = false;
	d->prevPosition = position;
	d->deathTimer = 0;
	d->respawnTimer = 0;
	pick_wander_target(d);

	placeables[idx].position = position;
	placeables[idx].heading = 0.0;

	Entity entity = Entity_initialize_entity();
	entity.state = &defenders[idx];
	entity.placeable = &placeables[idx];
	entity.renderable = &renderable;
	entity.collidable = &collidable;
	entity.aiUpdatable = &updatable;

	entityRefs[idx] = Entity_add_entity(entity);

	highestUsedIndex++;

	/* Load audio and register with enemy registry once, not per-entity */
	if (!sampleDeath) {
		Audio_load_sample(&sampleDeath, "resources/sounds/bomb_explode.wav");
		Audio_load_sample(&sampleRespawn, "resources/sounds/door.wav");
		Audio_load_sample(&sampleHit, "resources/sounds/samus_hurt.wav");
		SubShield_initialize_audio();
		SubHeal_initialize_audio();
		EnemyTypeCallbacks cb = {Defender_find_wounded, Defender_find_aggro, Defender_heal};
		defenderTypeId = EnemyRegistry_register(cb);
	}
}

void Defender_cleanup(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		if (entityRefs[i]) {
			entityRefs[i]->empty = true;
			entityRefs[i] = NULL;
		}
	}
	highestUsedIndex = 0;
	defenderTypeId = -1;
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;

	Audio_unload_sample(&sampleDeath);
	Audio_unload_sample(&sampleRespawn);
	Audio_unload_sample(&sampleHit);
	SubShield_cleanup_audio();
	SubHeal_cleanup_audio();
}

Collision Defender_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	DefenderState *d = (DefenderState *)state;
	Collision collision = {false, false};

	if (!d->alive)
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

void Defender_resolve(void *state, const Collision collision)
{
	(void)state;
	(void)collision;
}

void Defender_update(void *state, const PlaceableComponent *placeable, unsigned int ticks)
{
	DefenderState *d = (DefenderState *)state;
	int idx = (int)(d - defenders);
	PlaceableComponent *pl = &placeables[idx];
	double dt = ticks / 1000.0;

	d->prevPosition = pl->position;
	d->boosting = false;
	d->hasAssignment = false;
	d->aegisWasActive = SubShield_is_active(&d->shieldCore);
	currentUpdaterIdx = idx;

	/* --- Tick timers --- */
	SubHeal_update(&d->healCore, &defHealCfg, ticks);

	/* Aegis timer + grace (all handled by core) */
	SubShield_update(&d->shieldCore, &defShieldCfg, ticks);

	if (d->alive)
		Enemy_check_stealth_proximity(pl->position, d->facing);

	/* --- Check for incoming player projectiles --- */
	if (d->alive && d->aiState != DEFENDER_DYING && !SubShield_in_grace(&d->shieldCore)) {
		Rectangle body = {-BODY_SIZE, BODY_SIZE, BODY_SIZE, -BODY_SIZE};
		Rectangle hitBox = Collision_transform_bounding_box(pl->position, body);

		PlayerDamageResult dmg = Enemy_check_player_damage(hitBox, pl->position);
		bool hit = dmg.hit;

		if (hit) {
			bool shielded = SubShield_is_active(&d->shieldCore) && !dmg.ambush;
			activate_spark(pl->position, shielded);

			if (shielded) {
				if (dmg.mine_hit) {
					/* Mine breaks aegis shield — no damage, grace outlasts explosion */
					SubShield_break(&d->shieldCore, &defShieldCfg);
				}
				SubShield_on_hit(&d->shieldCore);
			} else {
				d->hp -= dmg.damage + (dmg.mine_hit ? dmg.mine_damage : 0.0);
				Audio_play_sample(&sampleHit);

				/* Self-shield reaction — pop aegis after taking damage */
				if (!dmg.ambush)
					activate_aegis(d);

				/* Getting hit while idle triggers flee */
				if (d->aiState == DEFENDER_IDLE) {
					d->aiState = DEFENDER_FLEEING;
				}
			}
		}

		if ((!SubShield_is_active(&d->shieldCore) || dmg.ambush) && hit && d->hp <= 0.0) {
			d->alive = false;
			d->aiState = DEFENDER_DYING;
			d->deathTimer = 0;
			d->killedByPlayer = true;
			Audio_play_sample(&sampleDeath);
			Enemy_on_player_kill(&dmg);
		}
	}

	/* --- Self-shield on nearby shots --- */
	/* In SUPPORTING, aegis is managed by the relay logic — don't preemptively pop */
	if (d->alive && d->aiState != DEFENDER_DYING && d->aiState != DEFENDER_DEAD
			&& d->aiState != DEFENDER_SUPPORTING) {
		bool nearbyShot = Enemy_check_any_nearby(pl->position, 200.0);
		if (nearbyShot && !Sub_Stealth_is_ambush_active())
			activate_aegis(d);
	}

	/* --- State machine --- */
	switch (d->aiState) {
	case DEFENDER_IDLE: {
		/* Wander */
		Enemy_move_toward(pl, d->wanderTarget, IDLE_DRIFT_SPEED, dt, WALL_CHECK_DIST);
		d->wanderTimer -= ticks;
		if (d->wanderTimer <= 0)
			pick_wander_target(d);

		/* Check for nearby wounded or aggro allies — transition to supporting */
		if (d->aiState == DEFENDER_IDLE) {
			int typeCount = EnemyRegistry_type_count();
			for (int t = 0; t < typeCount && d->aiState == DEFENDER_IDLE; t++) {
				const EnemyTypeCallbacks *et = EnemyRegistry_get_type(t);
				Position pos;
				int eidx;
				if (et->find_wounded && et->find_wounded(pl->position, HEAL_RANGE, 50.0, &pos, &eidx))
					d->aiState = DEFENDER_SUPPORTING;
				else if (et->find_aggro && et->find_aggro(pl->position, 1600.0, &pos))
					d->aiState = DEFENDER_SUPPORTING;
			}
		}

		/* Check aggro — player proximity triggers awareness */
		Position shipPos = Ship_get_position();
		double dist = Enemy_distance_between(pl->position, shipPos);
		bool nearbyShot = Enemy_check_any_nearby(pl->position, 200.0);
		if (!Ship_is_destroyed() && !Sub_Stealth_is_stealthed() &&
			((dist < AGGRO_RANGE && Enemy_has_line_of_sight(pl->position, shipPos)) || nearbyShot)) {
			d->aiState = DEFENDER_SUPPORTING;
		}

		d->facing = Position_get_heading(pl->position, d->wanderTarget);
		break;
	}
	case DEFENDER_SUPPORTING: {
		Position shipPos = Ship_get_position();
		double shipDist = Enemy_distance_between(pl->position, shipPos);

		/* De-aggro check */
		if (Ship_is_destroyed() || Sub_Stealth_is_stealthed() || shipDist > DEAGGRO_RANGE) {
			d->aiState = DEFENDER_IDLE;
			pick_wander_target(d);
			break;
		}

		/* Try to heal an ally */
		try_heal_ally(d, pl);

		/* Flee if player gets close — but hold ground while actively shielding */
		if (shipDist < FLEE_TRIGGER_RANGE && !SubShield_is_active(&d->shieldCore)) {
			d->aiState = DEFENDER_FLEEING;
			break;
		}

		/* Find ally to protect — deconflict with other defenders */
		Position allyPos;
		bool hasTarget = false;
		int typeCount = EnemyRegistry_type_count();

		/* Collect per-type candidates */
		Position woundedPos[MAX_ENEMY_TYPES];
		Position aggroPos[MAX_ENEMY_TYPES];
		bool hasWounded[MAX_ENEMY_TYPES];
		bool hasAggro[MAX_ENEMY_TYPES];

		for (int t = 0; t < typeCount; t++) {
			const EnemyTypeCallbacks *et = EnemyRegistry_get_type(t);
			int eidx;
			hasWounded[t] = et->find_wounded &&
				et->find_wounded(pl->position, HEAL_RANGE * 1.5, 50.0, &woundedPos[t], &eidx);
			hasAggro[t] = et->find_aggro &&
				et->find_aggro(pl->position, 1600.0, &aggroPos[t]);
		}

		/* Two-pass priority: damage-dealing types first, fellow defenders only as fallback */
		for (int priority = 0; priority < 2 && !hasTarget; priority++) {
			/* Prefer uncontested wounded */
			for (int t = 0; t < typeCount && !hasTarget; t++) {
				bool isDefType = (defenderTypeId >= 0 && t == defenderTypeId);
				if ((priority == 0 && isDefType) || (priority == 1 && !isDefType))
					continue;
				if (hasWounded[t] && !another_defender_assigned(idx, woundedPos[t])) {
					allyPos = woundedPos[t]; hasTarget = true;
				}
			}
			/* Prefer uncontested aggro */
			for (int t = 0; t < typeCount && !hasTarget; t++) {
				bool isDefType = (defenderTypeId >= 0 && t == defenderTypeId);
				if ((priority == 0 && isDefType) || (priority == 1 && !isDefType))
					continue;
				if (hasAggro[t] && !another_defender_assigned(idx, aggroPos[t])) {
					allyPos = aggroPos[t]; hasTarget = true;
				}
			}
			/* Fallback: contested wounded */
			for (int t = 0; t < typeCount && !hasTarget; t++) {
				bool isDefType = (defenderTypeId >= 0 && t == defenderTypeId);
				if ((priority == 0 && isDefType) || (priority == 1 && !isDefType))
					continue;
				if (hasWounded[t]) { allyPos = woundedPos[t]; hasTarget = true; }
			}
			/* Fallback: contested aggro */
			for (int t = 0; t < typeCount && !hasTarget; t++) {
				bool isDefType = (defenderTypeId >= 0 && t == defenderTypeId);
				if ((priority == 0 && isDefType) || (priority == 1 && !isDefType))
					continue;
				if (hasAggro[t]) { allyPos = aggroPos[t]; hasTarget = true; }
			}
		}

		if (hasTarget) {
			d->hasAssignment = true;
			d->assignedTarget = allyPos;
		}

		if (hasTarget) {
			bool aegisReady = SubShield_is_active(&d->shieldCore) ||
				(d->shieldCore.cooldownMs <= 0 && !another_defender_shielding(idx, allyPos));
			double allyDist = Enemy_distance_between(pl->position, allyPos);

			if (aegisReady) {
				/* Aegis available — rush in to shield ally */
				double chaseSpeed = SubShield_is_active(&d->shieldCore) ? SHIELD_CHASE_SPEED : NORMAL_SPEED * BOOST_MULTIPLIER;
				d->boosting = true;
				Enemy_move_toward(pl, allyPos, chaseSpeed, dt, WALL_CHECK_DIST);

				/* Close enough to protect — pop aegis */
				if (allyDist < PROTECT_RADIUS)
					activate_aegis(d);
			} else {
				/* Aegis not ready — use relay distance if partner is shielding, else full standoff */
				double standoff = another_defender_shielding(idx, allyPos) ?
					RELAY_STANDOFF_DIST : AEGIS_STANDOFF_DIST;
				if (allyDist < standoff - 50.0) {
					Enemy_move_away_from(pl, allyPos, NORMAL_SPEED, dt, WALL_CHECK_DIST);
				} else if (allyDist > standoff + 50.0) {
					d->boosting = true;
					Enemy_move_toward(pl, allyPos, NORMAL_SPEED, dt, WALL_CHECK_DIST);
				}
			}
			d->facing = Position_get_heading(pl->position, allyPos);
		} else {
			/* No allies in need — return to idle */
			d->aiState = DEFENDER_IDLE;
			pick_wander_target(d);
		}
		break;
	}
	case DEFENDER_FLEEING: {
		Position shipPos = Ship_get_position();
		double shipDist = Enemy_distance_between(pl->position, shipPos);

		/* De-aggro */
		if (Ship_is_destroyed() || Sub_Stealth_is_stealthed() || shipDist > DEAGGRO_RANGE) {
			d->aiState = DEFENDER_IDLE;
			pick_wander_target(d);
			break;
		}

		/* Run away from player */
		Enemy_move_away_from(pl, shipPos, FLEE_SPEED, dt, WALL_CHECK_DIST);
		d->facing = Position_get_heading(pl->position, shipPos);

		/* Still try to heal allies while fleeing */
		try_heal_ally(d, pl);

		/* Safe distance — go back to supporting */
		if (shipDist > FLEE_SAFE_RANGE)
			d->aiState = DEFENDER_SUPPORTING;
		break;
	}
	case DEFENDER_DYING:
		d->deathTimer += ticks;
		if (d->deathTimer >= DEATH_FLASH_MS) {
			d->aiState = DEFENDER_DEAD;
			d->respawnTimer = 0;

			/* Drop fragment */
			if (d->killedByPlayer)
				Enemy_drop_fragments(pl->position, defenderCarried, 2);
		}
		break;

	case DEFENDER_DEAD:
		d->respawnTimer += ticks;
		if (d->respawnTimer >= RESPAWN_MS) {
			d->alive = true;
			d->hp = DEFENDER_HP;
			d->aiState = DEFENDER_IDLE;
			d->killedByPlayer = false;
			SubHeal_init(&d->healCore);
			SubShield_init(&d->shieldCore);
			pl->position = d->spawnPoint;
			pick_wander_target(d);
			Audio_play_sample(&sampleRespawn);
		}
		break;
	}

	/* Gravity well pull (alive, not dying/dead) */
	if (d->alive && d->aiState != DEFENDER_DYING && d->aiState != DEFENDER_DEAD)
		Enemy_apply_gravity(pl, dt);

	/* --- Spark decay (from idx 0 only) --- */
	if (idx == 0) {
		for (int si = 0; si < SPARK_POOL_SIZE; si++) {
			if (sparks[si].active) {
				sparks[si].ticksLeft -= ticks;
				if (sparks[si].ticksLeft <= 0)
					sparks[si].active = false;
			}
		}
	}
}

static void render_hexagon(Position pos, float radius, float thickness,
	float r, float g, float b, float a)
{
	for (int i = 0; i < 6; i++) {
		float a0 = i * 60.0f * PI / 180.0f;
		float a1 = (i + 1) * 60.0f * PI / 180.0f;
		float x0 = (float)pos.x + cosf(a0) * radius;
		float y0 = (float)pos.y + sinf(a0) * radius;
		float x1 = (float)pos.x + cosf(a1) * radius;
		float y1 = (float)pos.y + sinf(a1) * radius;
		Render_thick_line(x0, y0, x1, y1, thickness, r, g, b, a);
	}
}

void Defender_render(const void *state, const PlaceableComponent *placeable)
{
	DefenderState *d = (DefenderState *)state;
	int idx = (int)(d - defenders);

	if (d->aiState == DEFENDER_DEAD)
		return;

	/* Death flash */
	if (d->aiState == DEFENDER_DYING) {
		Enemy_render_death_flash(placeable, (float)d->deathTimer, (float)DEATH_FLASH_MS);
		return;
	}

	/* Body hexagon */
	const ColorFloat *bodyColor = (d->aiState == DEFENDER_IDLE) ? &colorBody : &colorAggro;

	/* Boost motion trail */
	if (d->boosting) {
		double dx = placeable->position.x - d->prevPosition.x;
		double dy = placeable->position.y - d->prevPosition.y;
		for (int g = BOOST_TRAIL_GHOSTS; g >= 1; g--) {
			float t = (float)g / (float)(BOOST_TRAIL_GHOSTS + 1);
			Position ghost;
			ghost.x = placeable->position.x - dx * BOOST_TRAIL_LENGTH * t;
			ghost.y = placeable->position.y - dy * BOOST_TRAIL_LENGTH * t;
			float alpha = (1.0f - t) * 0.35f;
			render_hexagon(ghost, BODY_SIZE, 1.5f,
				bodyColor->red, bodyColor->green, bodyColor->blue, alpha);
		}
	}

	render_hexagon(placeable->position, BODY_SIZE, 2.0f,
		bodyColor->red, bodyColor->green, bodyColor->blue, bodyColor->alpha);

	/* Center dot */
	Render_point(&placeable->position, 4.0, bodyColor);

	/* Aegis shield ring */
	SubShield_render_ring(&d->shieldCore, &defShieldCfg, placeable->position);

	/* Heal beam */
	SubHeal_render_beam(&d->healCore, &defHealCfg);

	/* Sparks (from idx 0 only) */
	if (idx == 0) {
		for (int si = 0; si < SPARK_POOL_SIZE; si++) {
			if (sparks[si].active) {
				Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
					SPARK_DURATION, SPARK_SIZE, sparks[si].shielded,
					0.5f, 0.8f, 1.0f);
			}
		}
	}
}

void Defender_render_bloom_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		DefenderState *d = &defenders[i];
		PlaceableComponent *pl = &placeables[i];

		if (d->aiState == DEFENDER_DEAD)
			continue;

		if (d->aiState == DEFENDER_DYING) {
			Enemy_render_death_flash(pl, (float)d->deathTimer, (float)DEATH_FLASH_MS);
			continue;
		}

		/* Boost trail bloom */
		const ColorFloat *bodyColor = (d->aiState == DEFENDER_IDLE) ? &colorBody : &colorAggro;
		if (d->boosting) {
			double dx = pl->position.x - d->prevPosition.x;
			double dy = pl->position.y - d->prevPosition.y;
			for (int g = BOOST_TRAIL_GHOSTS; g >= 1; g--) {
				float t = (float)g / (float)(BOOST_TRAIL_GHOSTS + 1);
				Position ghost;
				ghost.x = pl->position.x - dx * BOOST_TRAIL_LENGTH * t;
				ghost.y = pl->position.y - dy * BOOST_TRAIL_LENGTH * t;
				float alpha = (1.0f - t) * 0.25f;
				Render_point(&ghost, 6.0f, &(ColorFloat){bodyColor->red, bodyColor->green, bodyColor->blue, alpha});
			}
		}

		/* Body glow */
		Render_point(&pl->position, 6.0, bodyColor);

		/* Aegis ring bloom */
		SubShield_render_bloom(&d->shieldCore, &defShieldCfg, pl->position);

		/* Heal beam bloom */
		SubHeal_render_beam_bloom(&d->healCore, &defHealCfg);
	}

	/* Spark bloom */
	for (int si = 0; si < SPARK_POOL_SIZE; si++) {
		if (sparks[si].active) {
			Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
				SPARK_DURATION, SPARK_SIZE, sparks[si].shielded,
				0.5f, 0.8f, 1.0f);
		}
	}
}

void Defender_deaggro_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		DefenderState *d = &defenders[i];
		if (d->aiState == DEFENDER_SUPPORTING || d->aiState == DEFENDER_FLEEING) {
			d->aiState = DEFENDER_IDLE;
			pick_wander_target(d);
		}
	}
}

void Defender_reset_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		DefenderState *d = &defenders[i];
		d->alive = true;
		d->hp = DEFENDER_HP;
		d->aiState = DEFENDER_IDLE;
		d->killedByPlayer = false;
		SubHeal_init(&d->healCore);
		SubShield_init(&d->shieldCore);
		d->boosting = false;
		d->deathTimer = 0;
		d->respawnTimer = 0;
		placeables[i].position = d->spawnPoint;
		d->prevPosition = d->spawnPoint;
		pick_wander_target(d);
	}
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;
}

bool Defender_is_protecting(Position pos)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		DefenderState *d = &defenders[i];
		if (!d->alive)
			continue;
		/* Protected if aegis was active this frame OR in post-break grace */
		if (!d->aegisWasActive && !SubShield_in_grace(&d->shieldCore))
			continue;
		double dist = Enemy_distance_between(placeables[i].position, pos);
		if (dist < PROTECT_RADIUS)
			return true;
	}
	return false;
}

void Defender_notify_shield_hit(Position pos)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		DefenderState *d = &defenders[i];
		if (!d->alive)
			continue;
		if (!SubShield_is_active(&d->shieldCore) && !SubShield_in_grace(&d->shieldCore))
			continue;
		double dist = Enemy_distance_between(placeables[i].position, pos);
		if (dist < PROTECT_RADIUS) {
			SubShield_on_hit(&d->shieldCore);
			return;
		}
	}
}

bool Defender_find_wounded(Position from, double range, double hp_threshold, Position *out_pos, int *out_index)
{
	double bestDamage = 0.0;
	int bestIdx = -1;

	for (int i = 0; i < highestUsedIndex; i++) {
		if (i == currentUpdaterIdx)
			continue;
		DefenderState *d = &defenders[i];
		if (!d->alive || d->aiState == DEFENDER_DYING || d->aiState == DEFENDER_DEAD)
			continue;
		if (d->hp >= hp_threshold)
			continue;
		double missing = DEFENDER_HP - d->hp;
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

bool Defender_find_aggro(Position from, double range, Position *out_pos)
{
	double bestDist = range + 1.0;
	int bestIdx = -1;

	for (int i = 0; i < highestUsedIndex; i++) {
		if (i == currentUpdaterIdx)
			continue;
		DefenderState *d = &defenders[i];
		if (!d->alive)
			continue;
		if (d->aiState != DEFENDER_SUPPORTING && d->aiState != DEFENDER_FLEEING)
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

void Defender_heal(int index, double amount)
{
	if (index < 0 || index >= highestUsedIndex)
		return;
	DefenderState *d = &defenders[index];
	if (!d->alive)
		return;
	d->hp += amount;
	if (d->hp > DEFENDER_HP)
		d->hp = DEFENDER_HP;
}
