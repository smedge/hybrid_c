#include "seeker.h"
#include "sub_pea.h"
#include "sub_mgun.h"
#include "sub_mine.h"
#include "fragment.h"
#include "progression.h"
#include "player_stats.h"
#include "ship.h"
#include "view.h"
#include "render.h"
#include "color.h"
#include "audio.h"
#include "map.h"

#include <math.h>
#include <stdlib.h>
#include <SDL2/SDL_mixer.h>

#define SEEKER_COUNT 64

#define SEEKER_HP 60.0
#define STALK_SPEED 300.0
#define ORBIT_SPEED 500.0
#define AGGRO_RANGE 1000.0
#define DEAGGRO_RANGE 3200.0
#define ORBIT_RADIUS 750.0
#define IDLE_DRIFT_RADIUS 500.0
#define IDLE_DRIFT_SPEED 80.0
#define IDLE_WANDER_INTERVAL 2000

#define ORBIT_MIN_MS 1000
#define ORBIT_MAX_MS 2000
#define WINDUP_MS 300
#define DASH_SPEED 5000.0
#define DASH_DURATION_MS 150
#define DASH_DAMAGE 80.0
#define RECOVER_MS 2000

#define DEATH_FLASH_MS 200
#define RESPAWN_MS 30000

#define BODY_LENGTH 18.0
#define BODY_WIDTH 8.0
#define WALL_CHECK_DIST 50.0
#define NEAR_MISS_RADIUS 200.0

#define PI 3.14159265358979323846

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

	/* Windup */
	double dashDirX;
	double dashDirY;

	/* Dash */
	int dashTimer;
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
static int highestUsedIndex = 0;

/* Death spark */
static bool sparkActive = false;
static Position sparkPosition;
static int sparkTicksLeft = 0;
#define SPARK_DURATION 80
#define SPARK_SIZE 15.0

/* Audio */
static Mix_Chunk *sampleWindup = 0;
static Mix_Chunk *sampleDash = 0;
static Mix_Chunk *sampleDeath = 0;
static Mix_Chunk *sampleRespawn = 0;
static Mix_Chunk *sampleHit = 0;

/* Helpers */
static double distance_between(Position a, Position b)
{
	double dx = b.x - a.x;
	double dy = b.y - a.y;
	return sqrt(dx * dx + dy * dy);
}

static void pick_wander_target(SeekerState *s)
{
	double angle = (rand() % 360) * PI / 180.0;
	double dist = (rand() % (int)IDLE_DRIFT_RADIUS);
	s->wanderTarget.x = s->spawnPoint.x + cos(angle) * dist;
	s->wanderTarget.y = s->spawnPoint.y + sin(angle) * dist;
	s->wanderTimer = IDLE_WANDER_INTERVAL + (rand() % 1000);
}

static bool has_line_of_sight(Position from, Position to)
{
	double hx, hy;
	return !Map_line_test_hit(from.x, from.y, to.x, to.y, &hx, &hy);
}

static void move_toward(PlaceableComponent *pl, Position target, double speed, double dt)
{
	double dx = target.x - pl->position.x;
	double dy = target.y - pl->position.y;
	double dist = sqrt(dx * dx + dy * dy);
	if (dist < 1.0)
		return;

	double nx = dx / dist;
	double ny = dy / dist;

	/* Wall avoidance */
	double checkX = pl->position.x + nx * WALL_CHECK_DIST;
	double checkY = pl->position.y + ny * WALL_CHECK_DIST;
	double hx, hy;
	if (Map_line_test_hit(pl->position.x, pl->position.y, checkX, checkY, &hx, &hy))
		return;

	double move = speed * dt;
	if (move > dist)
		move = dist;

	pl->position.x += nx * move;
	pl->position.y += ny * move;
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
	s->dashDirX = 0.0;
	s->dashDirY = 0.0;
	s->dashTimer = 0;
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

	Entity_add_entity(entity);

	highestUsedIndex++;

	Audio_load_sample(&sampleWindup, "resources/sounds/enemy_aggro.wav");
	Audio_load_sample(&sampleDash, "resources/sounds/ricochet.wav");
	Audio_load_sample(&sampleDeath, "resources/sounds/bomb_explode.wav");
	Audio_load_sample(&sampleRespawn, "resources/sounds/door.wav");
	Audio_load_sample(&sampleHit, "resources/sounds/samus_hurt.wav");
}

void Seeker_cleanup(void)
{
	highestUsedIndex = 0;
	sparkActive = false;

	Audio_unload_sample(&sampleWindup);
	Audio_unload_sample(&sampleDash);
	Audio_unload_sample(&sampleDeath);
	Audio_unload_sample(&sampleRespawn);
	Audio_unload_sample(&sampleHit);
}

Collision Seeker_collide(const void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	SeekerState *s = (SeekerState *)state;
	Collision collision = {false, false};

	if (!s->alive)
		return collision;

	Rectangle thisBB = collidable.boundingBox;
	Rectangle transformed = Collision_transform_bounding_box(placeable->position, thisBB);

	if (Collision_aabb_test(transformed, boundingBox)) {
		collision.collisionDetected = true;
		/* Solid contact only during dash (deals damage via update check) */
		if (s->aiState == SEEKER_DASHING)
			collision.solid = true;
	}

	return collision;
}

void Seeker_resolve(const void *state, const Collision collision)
{
	(void)state;
	(void)collision;
}

void Seeker_update(const void *state, const PlaceableComponent *placeable, unsigned int ticks)
{
	SeekerState *s = (SeekerState *)state;
	int idx = (int)(s - seekers);
	PlaceableComponent *pl = &placeables[idx];
	double dt = ticks / 1000.0;

	/* --- Check for incoming player projectiles (if alive and not dying) --- */
	if (s->alive && s->aiState != SEEKER_DYING) {
		Rectangle body = {-BODY_WIDTH, BODY_LENGTH, BODY_WIDTH, -BODY_LENGTH};
		Rectangle hitBox = Collision_transform_bounding_box(pl->position, body);

		bool hit = false;
		if (Sub_Pea_check_hit(hitBox)) {
			s->hp -= 50.0;
			hit = true;
		}
		if (Sub_Mgun_check_hit(hitBox)) {
			s->hp -= 20.0;
			hit = true;
		}
		if (Sub_Mine_check_hit(hitBox)) {
			s->hp -= 100.0;
			hit = true;
		}

		if (hit) {
			sparkActive = true;
			sparkPosition = pl->position;
			sparkTicksLeft = SPARK_DURATION;
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
		}
	}

	/* --- State machine --- */
	switch (s->aiState) {
	case SEEKER_IDLE: {
		move_toward(pl, s->wanderTarget, IDLE_DRIFT_SPEED, dt);

		s->wanderTimer -= ticks;
		if (s->wanderTimer <= 0)
			pick_wander_target(s);

		/* Check aggro */
		Position shipPos = Ship_get_position();
		double dist = distance_between(pl->position, shipPos);
		bool nearbyShot = Sub_Pea_check_nearby(pl->position, NEAR_MISS_RADIUS)
						|| Sub_Mgun_check_nearby(pl->position, NEAR_MISS_RADIUS);
		if (!Ship_is_destroyed() &&
			((dist < AGGRO_RANGE && has_line_of_sight(pl->position, shipPos)) || nearbyShot)) {
			s->aiState = SEEKER_STALKING;
		}

		s->facing = Position_get_heading(pl->position, s->wanderTarget);
		break;
	}
	case SEEKER_STALKING: {
		Position shipPos = Ship_get_position();
		double dist = distance_between(pl->position, shipPos);
		bool los = has_line_of_sight(pl->position, shipPos);

		/* De-aggro */
		if (Ship_is_destroyed() || dist > DEAGGRO_RANGE || !los) {
			s->aiState = SEEKER_IDLE;
			pick_wander_target(s);
			break;
		}

		/* Move toward player */
		move_toward(pl, shipPos, STALK_SPEED, dt);
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
		double dist = distance_between(pl->position, shipPos);

		/* De-aggro */
		if (Ship_is_destroyed() || dist > DEAGGRO_RANGE ||
			!has_line_of_sight(pl->position, shipPos)) {
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
		move_toward(pl, target, ORBIT_SPEED, dt);
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
				s->dashDirX = dx / len;
				s->dashDirY = dy / len;
			} else {
				s->dashDirX = 0.0;
				s->dashDirY = 1.0;
			}

			Audio_play_sample(&sampleWindup);
		}
		break;
	}
	case SEEKER_WINDING_UP: {
		/* Stationary, flashing telegraph */
		s->windupTimer += ticks;

		/* Face dash direction */
		s->facing = atan2(s->dashDirX, s->dashDirY) * 180.0 / PI;

		if (s->windupTimer >= WINDUP_MS) {
			s->aiState = SEEKER_DASHING;
			s->dashTimer = 0;
			s->dashStartPos = pl->position;
			Audio_play_sample(&sampleDash);
		}
		break;
	}
	case SEEKER_DASHING: {
		s->dashTimer += ticks;

		double moveX = s->dashDirX * DASH_SPEED * dt;
		double moveY = s->dashDirY * DASH_SPEED * dt;

		/* Check wall collision along dash path */
		double hx, hy;
		double newX = pl->position.x + moveX;
		double newY = pl->position.y + moveY;
		if (Map_line_test_hit(pl->position.x, pl->position.y, newX, newY, &hx, &hy)) {
			/* Stop at wall */
			pl->position.x = hx - s->dashDirX * 5.0; /* back off slightly */
			pl->position.y = hy - s->dashDirY * 5.0;
			/* End dash early */
			s->aiState = SEEKER_RECOVERING;
			s->recoverTimer = 0;
			s->recoverVelX = s->dashDirX * DASH_SPEED * 0.1;
			s->recoverVelY = s->dashDirY * DASH_SPEED * 0.1;
			break;
		}

		pl->position.x = newX;
		pl->position.y = newY;
		s->facing = atan2(s->dashDirX, s->dashDirY) * 180.0 / PI;

		/* Check hit on player during dash */
		if (!Ship_is_destroyed()) {
			Position shipPos = Ship_get_position();
			Rectangle shipBB = {-20.0, 20.0, 20.0, -20.0};
			Rectangle shipWorld = Collision_transform_bounding_box(shipPos, shipBB);
			Rectangle seekerBB = {-BODY_WIDTH, BODY_LENGTH, BODY_WIDTH, -BODY_LENGTH};
			Rectangle seekerWorld = Collision_transform_bounding_box(pl->position, seekerBB);

			if (Collision_aabb_test(seekerWorld, shipWorld)) {
				PlayerStats_damage(DASH_DAMAGE);
			}
		}

		if (s->dashTimer >= DASH_DURATION_MS) {
			s->aiState = SEEKER_RECOVERING;
			s->recoverTimer = 0;
			/* Carry some momentum into recovery */
			s->recoverVelX = s->dashDirX * DASH_SPEED * 0.1;
			s->recoverVelY = s->dashDirY * DASH_SPEED * 0.1;
		}
		break;
	}
	case SEEKER_RECOVERING: {
		s->recoverTimer += ticks;

		/* Decelerate from dash momentum */
		double dampen = 1.0 - 3.0 * dt;
		if (dampen < 0.0) dampen = 0.0;
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
			if (!Ship_is_destroyed()) {
				Position shipPos = Ship_get_position();
				double dist = distance_between(pl->position, shipPos);
				if (dist < DEAGGRO_RANGE && has_line_of_sight(pl->position, shipPos)) {
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
			if (s->killedByPlayer && !Progression_is_unlocked(SUB_ID_EGRESS))
				Fragment_spawn(pl->position, FRAG_TYPE_SEEKER);
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

	/* Spark decay (only from seeker index 0 to avoid N updates) */
	if (idx == 0) {
		if (sparkActive) {
			sparkTicksLeft -= ticks;
			if (sparkTicksLeft <= 0)
				sparkActive = false;
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
		float fade = 1.0f - (float)s->deathTimer / DEATH_FLASH_MS;
		ColorFloat flash = {1.0f, 1.0f, 1.0f, fade};
		Rectangle sparkRect = {-20.0, 20.0, 20.0, -20.0};
		Render_quad(&placeable->position, 0.0, sparkRect, &flash);
		Render_quad(&placeable->position, 45.0, sparkRect, &flash);
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
			ghost.x = placeable->position.x - s->dashDirX * BODY_LENGTH * 3.0 * t;
			ghost.y = placeable->position.y - s->dashDirY * BODY_LENGTH * 3.0 * t;
			float alpha = (1.0f - t) * 0.5f;
			ColorFloat ghostColor = {1.0f, 1.0f, 1.0f, alpha};
			render_needle(ghost, s->facing, &ghostColor);
		}
	}

	render_needle(placeable->position, s->facing, bodyColor);

	/* Center dot */
	Render_point(&placeable->position, 3.0, bodyColor);

	/* Render hit spark (only from index 0) */
	int idx = (int)((SeekerState *)state - seekers);
	if (idx == 0 && sparkActive) {
		float fade = (float)sparkTicksLeft / SPARK_DURATION;
		ColorFloat sparkColor = {0.0f, 1.0f, 0.2f, fade};
		Rectangle sparkRect = {-SPARK_SIZE, SPARK_SIZE, SPARK_SIZE, -SPARK_SIZE};
		Render_quad(&sparkPosition, 0.0, sparkRect, &sparkColor);
		Render_quad(&sparkPosition, 45.0, sparkRect, &sparkColor);
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
			float fade = 1.0f - (float)s->deathTimer / DEATH_FLASH_MS;
			ColorFloat flash = {1.0f, 1.0f, 1.0f, fade};
			Rectangle sparkRect = {-20.0, 20.0, 20.0, -20.0};
			Render_quad(&pl->position, 0.0, sparkRect, &flash);
			Render_quad(&pl->position, 45.0, sparkRect, &flash);
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
				ghost.x = pl->position.x - s->dashDirX * BODY_LENGTH * 2.0 * t;
				ghost.y = pl->position.y - s->dashDirY * BODY_LENGTH * 2.0 * t;
				float alpha = (1.0f - t) * 0.6f;
				ColorFloat gc = {1.0f, 1.0f, 1.0f, alpha};
				Render_point(&ghost, 4.0, &gc);
			}
		}
	}

	/* Spark bloom */
	if (sparkActive) {
		float fade = (float)sparkTicksLeft / SPARK_DURATION;
		ColorFloat sparkColor = {0.0f, 1.0f, 0.2f, fade};
		Rectangle sparkRect = {-SPARK_SIZE, SPARK_SIZE, SPARK_SIZE, -SPARK_SIZE};
		Render_quad(&sparkPosition, 0.0, sparkRect, &sparkColor);
		Render_quad(&sparkPosition, 45.0, sparkRect, &sparkColor);
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
		s->deathTimer = 0;
		s->respawnTimer = 0;
		s->recoverVelX = 0.0;
		s->recoverVelY = 0.0;
		placeables[i].position = s->spawnPoint;
		pick_wander_target(s);
	}
	sparkActive = false;
}
