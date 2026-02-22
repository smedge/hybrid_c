#include "seeker.h"
#include "sub_dash_core.h"
#include "enemy_util.h"
#include "defender.h"
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
#include "enemy_registry.h"

#include <math.h>
#include <stdlib.h>
#include <SDL2/SDL_mixer.h>

#define SEEKER_COUNT 128

#define SEEKER_HP 60.0
#define STALK_SPEED 300.0
#define ORBIT_SPEED 500.0
#define AGGRO_RANGE 1000.0
#define DEAGGRO_RANGE 3200.0
#define ORBIT_RADIUS 500.0
#define IDLE_DRIFT_RADIUS 500.0
#define IDLE_DRIFT_SPEED 80.0
#define IDLE_WANDER_INTERVAL 2000

#define ORBIT_MIN_MS 1000
#define ORBIT_MAX_MS 2000
#define WINDUP_MS 300
static const SubDashConfig seekerDashCfg = {
	.duration_ms = 150,
	.speed = 5000.0,
	.cooldown_ms = 0,
	.damage = 80.0,
};

#define RECOVER_MS 2000

#define DEATH_FLASH_MS 200
#define RESPAWN_MS 30000

#define BODY_LENGTH 18.0
#define BODY_WIDTH 8.0

static const CarriedSubroutine seekerCarried[] = {
	{ SUB_ID_EGRESS, FRAG_TYPE_SEEKER },
};
#define WALL_CHECK_DIST 50.0
#define NEAR_MISS_RADIUS 200.0

typedef enum {
	SEEKER_IDLE,
	SEEKER_STALKING,
	SEEKER_ORBITING,
	SEEKER_WINDING_UP,
	SEEKER_DASHING,
	SEEKER_RECOVERING,
	SEEKER_DYING,
	SEEKER_DEAD
} SeekerAIState;

typedef struct {
	bool alive;
	double hp;
	SeekerAIState aiState;
	Position spawnPoint;
	double facing;
	bool killedByPlayer;

	/* Idle wander */
	Position wanderTarget;
	int wanderTimer;

	/* Orbiting */
	double orbitAngle;    /* current angle around player (radians) */
	int orbitDirection;   /* +1 or -1 */
	int orbitTimer;       /* how long to orbit before striking */

	/* Windup — direction locked before dash */
	double pendingDirX;
	double pendingDirY;

	/* Dash */
	SubDashCore dashCore;
	Position dashStartPos;

	/* Recovery */
	int recoverTimer;
	double recoverVelX;
	double recoverVelY;

	/* Death/respawn */
	int deathTimer;
	int respawnTimer;

	/* Windup flash */
	int windupTimer;
} SeekerState;

/* Shared singleton components */
static RenderableComponent renderable = {Seeker_render};
static CollidableComponent collidable = {{-BODY_WIDTH, BODY_LENGTH, BODY_WIDTH, -BODY_LENGTH},
										  true,
										  COLLISION_LAYER_ENEMY,
										  COLLISION_LAYER_PLAYER,
										  Seeker_collide, Seeker_resolve};
static AIUpdatableComponent updatable = {Seeker_update};

/* Colors */
static const ColorFloat colorIdle    = {0.0f, 0.5f, 0.1f, 1.0f};
static const ColorFloat colorStalk   = {0.0f, 0.8f, 0.15f, 1.0f};
static const ColorFloat colorOrbit   = {0.0f, 1.0f, 0.2f, 1.0f};
static const ColorFloat colorRecover = {0.0f, 0.4f, 0.1f, 0.6f};

/* State arrays */
static SeekerState seekers[SEEKER_COUNT];
static PlaceableComponent placeables[SEEKER_COUNT];
static Entity *entityRefs[SEEKER_COUNT];
static int highestUsedIndex = 0;

/* Sparks */
#define SPARK_DURATION 80
#define SPARK_SIZE 15.0
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
static void pick_wander_target(SeekerState *s)
{
	Enemy_pick_wander_target(s->spawnPoint, IDLE_DRIFT_RADIUS, IDLE_WANDER_INTERVAL,
		&s->wanderTarget, &s->wanderTimer);
}

/* ---- Public API ---- */

void Seeker_initialize(Position position)
{
	if (highestUsedIndex >= SEEKER_COUNT) {
		printf("FATAL ERROR: Too many seeker entities.\n");
		return;
	}

	int idx = highestUsedIndex;
	SeekerState *s = &seekers[idx];

	s->alive = true;
	s->hp = SEEKER_HP;
	s->aiState = SEEKER_IDLE;
	s->spawnPoint = position;
	s->facing = 0.0;
	s->killedByPlayer = false;
	s->orbitAngle = 0.0;
	s->orbitDirection = 1;
	s->orbitTimer = 0;
	s->pendingDirX = 0.0;
	s->pendingDirY = 0.0;
	SubDash_init(&s->dashCore);
	s->recoverTimer = 0;
	s->recoverVelX = 0.0;
	s->recoverVelY = 0.0;
	s->deathTimer = 0;
	s->respawnTimer = 0;
	s->windupTimer = 0;
	pick_wander_target(s);

	placeables[idx].position = position;
	placeables[idx].heading = 0.0;

	Entity entity = Entity_initialize_entity();
	entity.state = &seekers[idx];
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

		EnemyTypeCallbacks cb = {Seeker_find_wounded, Seeker_find_aggro, Seeker_heal};
		EnemyRegistry_register(cb);
	}
}

void Seeker_cleanup(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		if (entityRefs[i]) {
			entityRefs[i]->empty = true;
			entityRefs[i] = NULL;
		}
	}
	highestUsedIndex = 0;
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;

	Audio_unload_sample(&sampleDeath);
	Audio_unload_sample(&sampleRespawn);
	Audio_unload_sample(&sampleHit);
}

Collision Seeker_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	SeekerState *s = (SeekerState *)state;
	Collision collision = {false, false};

	if (!s->alive)
		return collision;

	Rectangle thisBB = collidable.boundingBox;
	Rectangle transformed = Collision_transform_bounding_box(placeable->position, thisBB);

	if (Collision_aabb_test(transformed, boundingBox)) {
		collision.collisionDetected = true;
		/* Dash damage handled in update via explicit AABB check + hitThisDash.
		   Do NOT set solid here — Ship_resolve treats solid as wall → force_kill. */
		Sub_Stealth_break();
	}

	return collision;
}

void Seeker_resolve(void *state, const Collision collision)
{
	(void)state;
	(void)collision;
}

void Seeker_update(void *state, const PlaceableComponent *placeable, unsigned int ticks)
{
	SeekerState *s = (SeekerState *)state;
	int idx = (int)(s - seekers);
	PlaceableComponent *pl = &placeables[idx];
	double dt = ticks / 1000.0;

	if (s->alive)
		Enemy_check_stealth_proximity(pl->position, s->facing);

	/* --- Check for incoming player projectiles (if alive and not dying) --- */
	if (s->alive && s->aiState != SEEKER_DYING) {
		Rectangle body = {-BODY_WIDTH, BODY_LENGTH, BODY_WIDTH, -BODY_LENGTH};
		Rectangle hitBox = Collision_transform_bounding_box(pl->position, body);

		PlayerDamageResult dmg = Enemy_check_player_damage(hitBox, pl->position);
		bool shielded = Defender_is_protecting(pl->position, dmg.ambush);
		bool hit = dmg.hit;
		if (hit && !shielded)
			s->hp -= dmg.damage + dmg.mine_damage;

		if (hit) {
			activate_spark(pl->position, shielded);
			if (shielded)
				Defender_notify_shield_hit(pl->position);
			else
				Audio_play_sample(&sampleHit);

			/* Getting shot immediately aggroes */
			if (s->aiState == SEEKER_IDLE) {
				s->aiState = SEEKER_STALKING;
			}
		}

		if (hit && s->hp <= 0.0) {
			s->alive = false;
			s->aiState = SEEKER_DYING;
			s->deathTimer = 0;
			s->killedByPlayer = true;
			Audio_play_sample(&sampleDeath);
			Enemy_on_player_kill(&dmg);
		}
	}

	/* --- State machine --- */
	switch (s->aiState) {
	case SEEKER_IDLE: {
		Enemy_move_toward(pl, s->wanderTarget, IDLE_DRIFT_SPEED, dt, WALL_CHECK_DIST);

		s->wanderTimer -= ticks;
		if (s->wanderTimer <= 0)
			pick_wander_target(s);

		/* Check aggro */
		Position shipPos = Ship_get_position();
		double dist = Enemy_distance_between(pl->position, shipPos);
		bool nearbyShot = Enemy_check_any_nearby(pl->position, NEAR_MISS_RADIUS);
		if (!Ship_is_destroyed() && !Sub_Stealth_is_stealthed() &&
			((dist < AGGRO_RANGE && Enemy_has_line_of_sight(pl->position, shipPos)) || nearbyShot)) {
			s->aiState = SEEKER_STALKING;
		}

		s->facing = Position_get_heading(pl->position, s->wanderTarget);
		break;
	}
	case SEEKER_STALKING: {
		Position shipPos = Ship_get_position();
		double dist = Enemy_distance_between(pl->position, shipPos);
		bool los = Enemy_has_line_of_sight(pl->position, shipPos);

		/* De-aggro */
		if (Ship_is_destroyed() || Sub_Stealth_is_stealthed() || dist > DEAGGRO_RANGE || !los) {
			s->aiState = SEEKER_IDLE;
			pick_wander_target(s);
			break;
		}

		/* Move toward player */
		Enemy_move_toward(pl, shipPos, STALK_SPEED, dt, WALL_CHECK_DIST);
		s->facing = Position_get_heading(pl->position, shipPos);

		/* Transition to orbiting when in strike range */
		if (dist < ORBIT_RADIUS * 1.2) {
			s->aiState = SEEKER_ORBITING;
			/* Calculate initial orbit angle from current position relative to ship */
			double dx = pl->position.x - shipPos.x;
			double dy = pl->position.y - shipPos.y;
			s->orbitAngle = atan2(dx, dy);
			s->orbitDirection = (rand() % 2) ? 1 : -1;
			s->orbitTimer = ORBIT_MIN_MS + (rand() % (ORBIT_MAX_MS - ORBIT_MIN_MS));
		}
		break;
	}
	case SEEKER_ORBITING: {
		Position shipPos = Ship_get_position();
		double dist = Enemy_distance_between(pl->position, shipPos);

		/* De-aggro */
		if (Ship_is_destroyed() || Sub_Stealth_is_stealthed() || dist > DEAGGRO_RANGE ||
			!Enemy_has_line_of_sight(pl->position, shipPos)) {
			s->aiState = SEEKER_IDLE;
			pick_wander_target(s);
			break;
		}

		/* Circle the player */
		double angularSpeed = ORBIT_SPEED / ORBIT_RADIUS;
		s->orbitAngle += s->orbitDirection * angularSpeed * dt;

		Position target;
		target.x = shipPos.x + sin(s->orbitAngle) * ORBIT_RADIUS;
		target.y = shipPos.y + cos(s->orbitAngle) * ORBIT_RADIUS;

		/* Move toward orbit position */
		Enemy_move_toward(pl, target, ORBIT_SPEED, dt, WALL_CHECK_DIST);
		s->facing = Position_get_heading(pl->position, shipPos);

		s->orbitTimer -= ticks;
		if (s->orbitTimer <= 0) {
			/* Transition to windup */
			s->aiState = SEEKER_WINDING_UP;
			s->windupTimer = 0;

			/* Lock dash direction toward player's CURRENT position */
			double dx = shipPos.x - pl->position.x;
			double dy = shipPos.y - pl->position.y;
			double len = sqrt(dx * dx + dy * dy);
			if (len > 0.001) {
				s->pendingDirX = dx / len;
				s->pendingDirY = dy / len;
			} else {
				s->pendingDirX = 0.0;
				s->pendingDirY = 1.0;
			}

		}
		break;
	}
	case SEEKER_WINDING_UP: {
		/* Stationary, flashing telegraph */
		s->windupTimer += ticks;

		/* Face dash direction */
		s->facing = atan2(s->pendingDirX, s->pendingDirY) * 180.0 / PI;

		if (s->windupTimer >= WINDUP_MS) {
			s->aiState = SEEKER_DASHING;
			s->dashStartPos = pl->position;
			SubDash_try_activate(&s->dashCore, &seekerDashCfg, s->pendingDirX, s->pendingDirY);
		}
		break;
	}
	case SEEKER_DASHING: {
		double moveX = s->dashCore.dirX * seekerDashCfg.speed * dt;
		double moveY = s->dashCore.dirY * seekerDashCfg.speed * dt;

		/* Check wall collision along dash path */
		double hx, hy;
		double newX = pl->position.x + moveX;
		double newY = pl->position.y + moveY;
		if (Map_line_test_hit(pl->position.x, pl->position.y, newX, newY, &hx, &hy)) {
			pl->position.x = hx - s->dashCore.dirX * 5.0;
			pl->position.y = hy - s->dashCore.dirY * 5.0;
			s->recoverVelX = s->dashCore.dirX * seekerDashCfg.speed * 0.1;
			s->recoverVelY = s->dashCore.dirY * seekerDashCfg.speed * 0.1;
			SubDash_end_early(&s->dashCore, &seekerDashCfg);
			s->aiState = SEEKER_RECOVERING;
			s->recoverTimer = 0;
			break;
		}

		pl->position.x = newX;
		pl->position.y = newY;
		s->facing = atan2(s->dashCore.dirX, s->dashCore.dirY) * 180.0 / PI;

		/* Check hit on player during dash */
		if (!Ship_is_destroyed() && !s->dashCore.hitThisDash) {
			Position shipPos = Ship_get_position();
			Rectangle shipBB = {-SHIP_BB_HALF_SIZE, SHIP_BB_HALF_SIZE, SHIP_BB_HALF_SIZE, -SHIP_BB_HALF_SIZE};
			Rectangle shipWorld = Collision_transform_bounding_box(shipPos, shipBB);
			Rectangle seekerBB = {-BODY_WIDTH, BODY_LENGTH, BODY_WIDTH, -BODY_LENGTH};
			Rectangle seekerWorld = Collision_transform_bounding_box(pl->position, seekerBB);

			if (Collision_aabb_test(seekerWorld, shipWorld)) {
				PlayerStats_damage(seekerDashCfg.damage);
				s->dashCore.hitThisDash = true;
			}
		}

		if (SubDash_update(&s->dashCore, &seekerDashCfg, ticks)) {
			s->recoverVelX = s->dashCore.dirX * seekerDashCfg.speed * 0.1;
			s->recoverVelY = s->dashCore.dirY * seekerDashCfg.speed * 0.1;
			s->aiState = SEEKER_RECOVERING;
			s->recoverTimer = 0;
		}
		break;
	}
	case SEEKER_RECOVERING: {
		s->recoverTimer += ticks;

		/* Decelerate from dash momentum (frame-rate independent) */
		double dampen = exp(-3.0 * dt);
		s->recoverVelX *= dampen;
		s->recoverVelY *= dampen;

		/* Wall check for recovery drift */
		double newX = pl->position.x + s->recoverVelX * dt;
		double newY = pl->position.y + s->recoverVelY * dt;
		double hx, hy;
		if (!Map_line_test_hit(pl->position.x, pl->position.y, newX, newY, &hx, &hy)) {
			pl->position.x = newX;
			pl->position.y = newY;
		}

		if (s->recoverTimer >= RECOVER_MS) {
			/* Back to stalking if player still in range, otherwise idle */
			if (!Ship_is_destroyed() && !Sub_Stealth_is_stealthed()) {
				Position shipPos = Ship_get_position();
				double dist = Enemy_distance_between(pl->position, shipPos);
				if (dist < DEAGGRO_RANGE && Enemy_has_line_of_sight(pl->position, shipPos)) {
					s->aiState = SEEKER_STALKING;
					break;
				}
			}
			s->aiState = SEEKER_IDLE;
			pick_wander_target(s);
		}
		break;
	}
	case SEEKER_DYING:
		s->deathTimer += ticks;
		if (s->deathTimer >= DEATH_FLASH_MS) {
			s->aiState = SEEKER_DEAD;
			s->respawnTimer = 0;

			/* Drop fragment */
			if (s->killedByPlayer)
				Enemy_drop_fragments(pl->position, seekerCarried, 1);
		}
		break;

	case SEEKER_DEAD:
		s->respawnTimer += ticks;
		if (s->respawnTimer >= RESPAWN_MS) {
			s->alive = true;
			s->hp = SEEKER_HP;
			s->aiState = SEEKER_IDLE;
			s->killedByPlayer = false;
			pl->position = s->spawnPoint;
			pick_wander_target(s);
			Audio_play_sample(&sampleRespawn);
		}
		break;
	}

	/* Gravity well pull (alive, not dying/dead, not mid-dash) */
	if (s->alive && s->aiState != SEEKER_DYING && s->aiState != SEEKER_DEAD
			&& s->aiState != SEEKER_DASHING)
		Enemy_apply_gravity(pl, dt);

	/* Spark decay (only from seeker index 0 to avoid N updates) */
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

/* Render an elongated diamond (needle) shape */
static void render_needle(Position pos, double facing, const ColorFloat *c)
{
	float rad = (float)(facing * PI / 180.0);
	float sr = sinf(rad);
	float cr = cosf(rad);

	/* Tip (forward) */
	float tipX = (float)pos.x + sr * (float)BODY_LENGTH;
	float tipY = (float)pos.y + cr * (float)BODY_LENGTH;

	/* Tail (backward) */
	float tailX = (float)pos.x - sr * (float)BODY_LENGTH;
	float tailY = (float)pos.y - cr * (float)BODY_LENGTH;

	/* Side points (perpendicular) */
	float sideX1 = (float)pos.x + cr * (float)BODY_WIDTH;
	float sideY1 = (float)pos.y - sr * (float)BODY_WIDTH;
	float sideX2 = (float)pos.x - cr * (float)BODY_WIDTH;
	float sideY2 = (float)pos.y + sr * (float)BODY_WIDTH;

	/* Draw as 4 thick lines forming a diamond */
	Render_thick_line(tipX, tipY, sideX1, sideY1, 2.0f,
		c->red, c->green, c->blue, c->alpha);
	Render_thick_line(sideX1, sideY1, tailX, tailY, 2.0f,
		c->red, c->green, c->blue, c->alpha);
	Render_thick_line(tailX, tailY, sideX2, sideY2, 2.0f,
		c->red, c->green, c->blue, c->alpha);
	Render_thick_line(sideX2, sideY2, tipX, tipY, 2.0f,
		c->red, c->green, c->blue, c->alpha);
}

void Seeker_render(const void *state, const PlaceableComponent *placeable)
{
	SeekerState *s = (SeekerState *)state;

	if (s->aiState == SEEKER_DEAD)
		return;

	/* Death flash */
	if (s->aiState == SEEKER_DYING) {
		Enemy_render_death_flash(placeable, (float)s->deathTimer, (float)DEATH_FLASH_MS);
		return;
	}

	/* Choose color based on state */
	const ColorFloat *bodyColor;
	switch (s->aiState) {
	case SEEKER_IDLE:       bodyColor = &colorIdle; break;
	case SEEKER_STALKING:   bodyColor = &colorStalk; break;
	case SEEKER_ORBITING:   bodyColor = &colorOrbit; break;
	case SEEKER_RECOVERING: bodyColor = &colorRecover; break;
	case SEEKER_WINDING_UP: {
		/* Rapid white flash telegraph */
		bool flashOn = (s->windupTimer / 50) % 2 == 0;
		static const ColorFloat white = {1.0f, 1.0f, 1.0f, 1.0f};
		bodyColor = flashOn ? &white : &colorOrbit;
		break;
	}
	case SEEKER_DASHING: {
		/* Bright white during dash */
		static const ColorFloat dashColor = {1.0f, 1.0f, 1.0f, 1.0f};
		bodyColor = &dashColor;
		break;
	}
	default: bodyColor = &colorIdle; break;
	}

	/* Dash motion trail */
	if (s->aiState == SEEKER_DASHING) {
		for (int g = 5; g >= 1; g--) {
			float t = (float)g / 6.0f;
			Position ghost;
			ghost.x = placeable->position.x - s->dashCore.dirX * BODY_LENGTH * 3.0 * t;
			ghost.y = placeable->position.y - s->dashCore.dirY * BODY_LENGTH * 3.0 * t;
			float alpha = (1.0f - t) * 0.5f;
			ColorFloat ghostColor = {1.0f, 1.0f, 1.0f, alpha};
			render_needle(ghost, s->facing, &ghostColor);
		}
	}

	render_needle(placeable->position, s->facing, bodyColor);

	/* Center dot */
	Render_point(&placeable->position, 3.0, bodyColor);

	/* Render hit sparks (only from index 0) */
	int idx = (int)((SeekerState *)state - seekers);
	if (idx == 0) {
		for (int si = 0; si < SPARK_POOL_SIZE; si++) {
			if (sparks[si].active) {
				Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
					SPARK_DURATION, SPARK_SIZE, sparks[si].shielded,
					0.0f, 1.0f, 0.2f);
			}
		}
	}
}

void Seeker_render_bloom_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		SeekerState *s = &seekers[i];
		PlaceableComponent *pl = &placeables[i];

		if (s->aiState == SEEKER_DEAD)
			continue;

		if (s->aiState == SEEKER_DYING) {
			Enemy_render_death_flash(pl, (float)s->deathTimer, (float)DEATH_FLASH_MS);
			continue;
		}

		/* Body glow */
		const ColorFloat *bodyColor;
		switch (s->aiState) {
		case SEEKER_IDLE:       bodyColor = &colorIdle; break;
		case SEEKER_STALKING:   bodyColor = &colorStalk; break;
		case SEEKER_RECOVERING: bodyColor = &colorRecover; break;
		case SEEKER_WINDING_UP: {
			static const ColorFloat white = {1.0f, 1.0f, 1.0f, 1.0f};
			bool flashOn = (s->windupTimer / 50) % 2 == 0;
			bodyColor = flashOn ? &white : &colorOrbit;
			break;
		}
		case SEEKER_DASHING: {
			static const ColorFloat dashColor = {1.0f, 1.0f, 1.0f, 1.0f};
			bodyColor = &dashColor;
			break;
		}
		default: bodyColor = &colorOrbit; break;
		}

		Render_point(&pl->position, 6.0, bodyColor);

		/* Dash trail bloom */
		if (s->aiState == SEEKER_DASHING) {
			for (int g = 3; g >= 1; g--) {
				float t = (float)g / 4.0f;
				Position ghost;
				ghost.x = pl->position.x - s->dashCore.dirX * BODY_LENGTH * 2.0 * t;
				ghost.y = pl->position.y - s->dashCore.dirY * BODY_LENGTH * 2.0 * t;
				float alpha = (1.0f - t) * 0.6f;
				ColorFloat gc = {1.0f, 1.0f, 1.0f, alpha};
				Render_point(&ghost, 4.0, &gc);
			}
		}
	}

	/* Spark bloom */
	for (int si = 0; si < SPARK_POOL_SIZE; si++) {
		if (sparks[si].active) {
			Enemy_render_spark(sparks[si].position, sparks[si].ticksLeft,
				SPARK_DURATION, SPARK_SIZE, sparks[si].shielded,
				0.0f, 1.0f, 0.2f);
		}
	}
}

void Seeker_render_light_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		SeekerState *s = &seekers[i];
		PlaceableComponent *pl = &placeables[i];

		if (s->aiState == SEEKER_DEAD || s->aiState == SEEKER_DYING)
			continue;

		if (s->aiState == SEEKER_WINDING_UP) {
			bool flashOn = (s->windupTimer / 50) % 2 == 0;
			if (flashOn) {
				Render_filled_circle(
					(float)pl->position.x, (float)pl->position.y,
					180.0f, 12, 0.2f, 1.0f, 0.3f, 0.6f);
			}
		} else if (s->aiState == SEEKER_DASHING) {
			Render_filled_circle(
				(float)pl->position.x, (float)pl->position.y,
				240.0f, 12, 0.2f, 1.0f, 0.3f, 0.8f);
		}
	}

	for (int si = 0; si < SPARK_POOL_SIZE; si++) {
		if (sparks[si].active) {
			Render_filled_circle(
				(float)sparks[si].position.x,
				(float)sparks[si].position.y,
				240.0f, 12, 0.2f, 1.0f, 0.3f, 0.5f);
		}
	}
}

void Seeker_deaggro_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		SeekerState *s = &seekers[i];
		if (s->aiState == SEEKER_STALKING || s->aiState == SEEKER_ORBITING ||
			s->aiState == SEEKER_WINDING_UP || s->aiState == SEEKER_DASHING ||
			s->aiState == SEEKER_RECOVERING) {
			s->aiState = SEEKER_IDLE;
			s->recoverVelX = 0.0;
			s->recoverVelY = 0.0;
			pick_wander_target(s);
		}
	}
}

void Seeker_reset_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		SeekerState *s = &seekers[i];
		s->alive = true;
		s->hp = SEEKER_HP;
		s->aiState = SEEKER_IDLE;
		s->killedByPlayer = false;
		s->orbitTimer = 0;
		s->pendingDirX = 0.0;
		s->pendingDirY = 0.0;
		SubDash_init(&s->dashCore);
		s->windupTimer = 0;
		s->recoverTimer = 0;
		s->recoverVelX = 0.0;
		s->recoverVelY = 0.0;
		s->deathTimer = 0;
		s->respawnTimer = 0;
		placeables[i].position = s->spawnPoint;
		pick_wander_target(s);
	}
	for (int i = 0; i < SPARK_POOL_SIZE; i++)
		sparks[i].active = false;
}

bool Seeker_find_wounded(Position from, double range, double hp_threshold, Position *out_pos, int *out_index)
{
	double bestDamage = 0.0;
	int bestIdx = -1;

	for (int i = 0; i < highestUsedIndex; i++) {
		SeekerState *s = &seekers[i];
		if (!s->alive || s->aiState == SEEKER_DYING || s->aiState == SEEKER_DEAD)
			continue;
		if (s->hp >= hp_threshold)
			continue;
		double missing = SEEKER_HP - s->hp;
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

bool Seeker_find_aggro(Position from, double range, Position *out_pos)
{
	double bestDist = range + 1.0;
	int bestIdx = -1;

	for (int i = 0; i < highestUsedIndex; i++) {
		SeekerState *s = &seekers[i];
		if (!s->alive)
			continue;
		if (s->aiState == SEEKER_IDLE || s->aiState == SEEKER_DYING || s->aiState == SEEKER_DEAD)
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

void Seeker_heal(int index, double amount)
{
	if (index < 0 || index >= highestUsedIndex)
		return;
	SeekerState *s = &seekers[index];
	if (!s->alive)
		return;
	s->hp += amount;
	if (s->hp > SEEKER_HP)
		s->hp = SEEKER_HP;
}
