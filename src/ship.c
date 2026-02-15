#include "ship.h"
#include "view.h"
#include "render.h"
#include "sub_pea.h"
#include "sub_mgun.h"
#include "sub_mine.h"
#include "sub_boost.h"
#include "sub_egress.h"
#include "sub_mend.h"
#include "sub_aegis.h"
#include "color.h"
#include "shipstate.h"
#include "audio.h"
#include "player_stats.h"
#include "hunter.h"
#include "seeker.h"
#include "defender.h"
#include "mine.h"
#include "savepoint.h"
#include "zone.h"
#include "fragment.h"
#include "progression.h"
#include "skillbar.h"

#include <string.h>

#include <SDL2/SDL_mixer.h>

static const double NORMAL_VELOCITY = 800.0;
static const double FAST_VELOCITY = 1600.0;
static const double SLOW_VELOCITY = 6400.0;  // warp speed right now, not slow

static const double ACCEL_RATE = 10.0;  /* how fast velocity ramps up (per second, multiplier) */
static const double FRICTION = 14.0;    /* how fast velocity bleeds off (per second, multiplier) */

static double vel_x = 0.0;
static double vel_y = 0.0;
static Position prevPosition = {0.0, 0.0};
static bool isBoosting = false;

#define TRAIL_GHOSTS 20
#define TRAIL_LENGTH 4.0

static Mix_Chunk *sample01 = 0;
static Mix_Chunk *sample02 = 0;
static Mix_Chunk *sample03 = 0;

static const ColorRGB COLOR = {255, 0, 0, 255};
static ColorFloat color;

static bool sparkActive;
static Position sparkPosition;
static int sparkTicksLeft;
#define SPARK_DURATION 80
#define SPARK_SIZE 30.0

static PlaceableComponent placeable = {{0.0, 0.0}, 0.0};
static RenderableComponent renderable = {Ship_render};
static UserUpdatableComponent updatable = {Ship_update};
static CollidableComponent collidable = {{-20.0, 20.0, 20.0, -20.0}, true,
	COLLISION_LAYER_PLAYER, COLLISION_LAYER_TERRAIN | COLLISION_LAYER_ENEMY,
	Ship_collide, Ship_resolve};

static ShipState shipState = {true, 0};
static bool godMode = false;

static double get_heading(bool n, bool s, bool e, bool w);

void Ship_initialize() 
{
	Entity entity = Entity_initialize_entity();
	entity.state = &shipState;
	entity.placeable = &placeable;
	entity.renderable = &renderable;
	entity.userUpdatable = &updatable;
	entity.collidable = &collidable;

	Entity *liveEntity = Entity_add_entity(entity);

	shipState.destroyed = true;
	shipState.ticksDestroyed = 0;
	sparkActive = false;

	color = Color_rgb_to_float(&COLOR);

	Audio_load_sample(&sample01, "resources/sounds/statue_rise.wav");
	Audio_load_sample(&sample02, "resources/sounds/samus_die.wav");

	Audio_load_sample(&sample03, "resources/sounds/bomb_explode.wav");

	Sub_Pea_initialize(liveEntity);
	Sub_Mgun_initialize(liveEntity);
	Sub_Mine_initialize();
	Sub_Boost_initialize();
	Sub_Egress_initialize();
	Sub_Mend_initialize();
	Sub_Aegis_initialize();
}

void Ship_cleanup()
{
	Sub_Pea_cleanup();
	Sub_Mgun_cleanup();
	Sub_Mine_cleanup();
	Sub_Boost_cleanup();
	Sub_Egress_cleanup();
	Sub_Mend_cleanup();
	Sub_Aegis_cleanup();

	placeable.position.x = 0.0;
	placeable.position.y = 0.0;
	placeable.heading = 0.0;

	Audio_unload_sample(&sample01);
	Audio_unload_sample(&sample02);
	Audio_unload_sample(&sample03);
}

Collision Ship_collide(const void *state, const PlaceableComponent *placeable, const Rectangle boundingBox) 
{
	Collision collision = {false, true};

	Position position = placeable->position;
	Rectangle thisBoundingBox = collidable.boundingBox;
	Rectangle transformedBoundingBox = Collision_transform_bounding_box(position, thisBoundingBox);

	if (Collision_aabb_test(transformedBoundingBox, boundingBox)) {
		collision.collisionDetected = true;
	}

	return collision;
}

void Ship_resolve(const void *state, const Collision collision)
{
	if (godMode)
		return;

	if (shipState.destroyed)
		return;

	if (collision.solid)
	{
		PlayerStats_force_kill();
	}
}

void Ship_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable)
{
	double ticksNormalized = ticks / 1000.0;

	prevPosition = placeable->position;

	if (shipState.destroyed) {
		isBoosting = false;
		shipState.ticksDestroyed += ticks;

		if (shipState.ticksDestroyed >= DEATH_TIMER) {
			shipState.destroyed = false;
			shipState.ticksDestroyed = 0;

			/* Respawn at checkpoint if valid and in current zone */
			const SaveCheckpoint *ckpt = Savepoint_get_checkpoint();
			const Zone *z = Zone_get();
			if (ckpt->valid && strcmp(ckpt->zone_path, z->filepath) == 0) {
				placeable->position = ckpt->position;
				Savepoint_suppress_by_id(ckpt->savepoint_id);
				for (int i = 0; i < FRAG_TYPE_COUNT; i++)
					Fragment_set_count(i, ckpt->fragment_counts[i]);
				Progression_restore(ckpt->unlocked, ckpt->discovered);
				Skillbar_restore(ckpt->skillbar);
			} else if (ckpt->valid) {
				/* Cross-zone respawn — flag for mode_gameplay to handle */
				Ship_set_pending_cross_zone_respawn(true);
				placeable->position.x = 0.0;
				placeable->position.y = 0.0;
			} else {
				/* No checkpoint — reset to default state */
				placeable->position.x = 0.0;
				placeable->position.y = 0.0;
				for (int i = 0; i < FRAG_TYPE_COUNT; i++)
					Fragment_set_count(i, 0);
				Progression_initialize();
				Skillbar_initialize();
			}

			placeable->heading = 0.0;
			vel_x = 0.0;
			vel_y = 0.0;
			prevPosition = placeable->position;
			Sub_Boost_initialize();
			Sub_Egress_initialize();
			Sub_Mend_initialize();
			Sub_Aegis_initialize();
			PlayerStats_reset();
			Hunter_reset_all();
			Seeker_reset_all();
			Defender_reset_all();
			Mine_reset_all();

			Audio_play_sample(&sample01);
		}
	}
	else {
		/* Death check — all damage sources funnel through integrity */
		if (!godMode && PlayerStats_is_dead()) {
			sparkActive = true;
			sparkPosition = placeable->position;
			sparkTicksLeft = SPARK_DURATION;
			shipState.destroyed = true;
			Audio_play_sample(&sample02);
			Audio_play_sample(&sample03);
			Hunter_deaggro_all();
			Seeker_deaggro_all();
			Defender_deaggro_all();
		}

		/* Update movement subs */
		Sub_Boost_update(userInput, ticks);
		Sub_Egress_update(userInput, ticks);

		isBoosting = Sub_Boost_is_boosting() || Sub_Egress_is_dashing();

		if (Sub_Egress_is_dashing()) {
			/* Dash overrides normal movement — fixed velocity in dash direction */
			vel_x = Sub_Egress_get_dash_vx();
			vel_y = Sub_Egress_get_dash_vy();

			placeable->position.x += vel_x * ticksNormalized;
			placeable->position.y += vel_y * ticksNormalized;
		} else {
			double maxSpeed;
			if (Sub_Boost_is_boosting())
				maxSpeed = FAST_VELOCITY;
			else if (userInput->keyLControl)
				maxSpeed = SLOW_VELOCITY;
			else
				maxSpeed = NORMAL_VELOCITY;

			/* Target velocity from input */
			double target_vx = 0.0, target_vy = 0.0;
			if (userInput->keyW) target_vy += 1.0;
			if (userInput->keyS) target_vy -= 1.0;
			if (userInput->keyD) target_vx += 1.0;
			if (userInput->keyA) target_vx -= 1.0;

			/* Normalize diagonal so it doesn't go faster */
			if (target_vx != 0.0 && target_vy != 0.0) {
				target_vx *= 0.7071;
				target_vy *= 0.7071;
			}

			target_vx *= maxSpeed;
			target_vy *= maxSpeed;

			/* Lerp toward target (acceleration) or toward zero (friction) */
			int hasInput = userInput->keyW || userInput->keyA ||
				userInput->keyS || userInput->keyD;

			double rate = hasInput ? ACCEL_RATE : FRICTION;
			double blend = rate * ticksNormalized;
			if (blend > 1.0) blend = 1.0;

			vel_x += (target_vx - vel_x) * blend;
			vel_y += (target_vy - vel_y) * blend;

			/* Apply velocity */
			placeable->position.x += vel_x * ticksNormalized;
			placeable->position.y += vel_y * ticksNormalized;

			if (hasInput) {
				placeable->heading = get_heading(userInput->keyW, userInput->keyS,
												userInput->keyD, userInput->keyA);
			}
		}
	}

	if (sparkActive) {
		sparkTicksLeft -= ticks;
		if (sparkTicksLeft <= 0)
			sparkActive = false;
	}

	Sub_Pea_update(userInput, ticks, placeable);
	Sub_Mgun_update(userInput, ticks, placeable);
	Sub_Mine_update(userInput, ticks);
	Sub_Mend_update(userInput, ticks);
	Sub_Aegis_update(userInput, ticks);
}

void Ship_render(const void *state, const PlaceableComponent *placeable)
{
	if (!shipState.destroyed) {
		/* Motion trail when boosting */
		if (isBoosting) {
			double dx = placeable->position.x - prevPosition.x;
			double dy = placeable->position.y - prevPosition.y;
			for (int i = TRAIL_GHOSTS; i >= 1; i--) {
				float t = (float)i / (float)(TRAIL_GHOSTS + 1);
				Position ghost;
				ghost.x = placeable->position.x - dx * TRAIL_LENGTH * t;
				ghost.y = placeable->position.y - dy * TRAIL_LENGTH * t;
				float alpha = (1.0f - t) * 0.4f;
				ColorFloat ghostColor = {color.red, color.green, color.blue, alpha};
				Render_triangle(&ghost, placeable->heading, &ghostColor);
			}
		}

		View view =  View_get_view();

		if (view.scale > 0.09)
			Render_triangle(&placeable->position, placeable->heading, &color);
		else
			Render_point(&placeable->position, 2.0, &color);

		//Render_bounding_box(&placeable->position, &collidable.boundingBox);
	}

	if (sparkActive) {
		float fade = (float)sparkTicksLeft / SPARK_DURATION;
		ColorFloat sparkColor = {1.0f, 1.0f, 1.0f, fade};
		Rectangle sparkRect = {-SPARK_SIZE, SPARK_SIZE, SPARK_SIZE, -SPARK_SIZE};
		Render_quad(&sparkPosition, 0.0, sparkRect, &sparkColor);
		Render_quad(&sparkPosition, 45.0, sparkRect, &sparkColor);
	}

	Sub_Pea_render();
	Sub_Mgun_render();
	Sub_Mine_render();
	Sub_Aegis_render();
}

void Ship_render_bloom_source(void)
{
	if (!shipState.destroyed) {
		if (isBoosting) {
			double dx = placeable.position.x - prevPosition.x;
			double dy = placeable.position.y - prevPosition.y;
			for (int i = TRAIL_GHOSTS; i >= 1; i--) {
				float t = (float)i / (float)(TRAIL_GHOSTS + 1);
				Position ghost;
				ghost.x = placeable.position.x - dx * TRAIL_LENGTH * t;
				ghost.y = placeable.position.y - dy * TRAIL_LENGTH * t;
				float alpha = (1.0f - t) * 0.4f;
				ColorFloat ghostColor = {color.red, color.green, color.blue, alpha};
				Render_triangle(&ghost, placeable.heading, &ghostColor);
			}
		}
		Render_triangle(&placeable.position, placeable.heading, &color);
	}

	if (sparkActive) {
		float fade = (float)sparkTicksLeft / SPARK_DURATION;
		ColorFloat sparkColor = {1.0f, 1.0f, 1.0f, fade};
		Rectangle sparkRect = {-SPARK_SIZE, SPARK_SIZE, SPARK_SIZE, -SPARK_SIZE};
		Render_quad(&sparkPosition, 0.0, sparkRect, &sparkColor);
		Render_quad(&sparkPosition, 45.0, sparkRect, &sparkColor);
	}

	Sub_Pea_render_bloom_source();
	Sub_Mgun_render_bloom_source();
	Sub_Mine_render_bloom_source();
}

Position Ship_get_position()
{
	return placeable.position;
}

double Ship_get_heading()
{
	return placeable.heading;
}

void Ship_force_spawn(Position pos)
{
	shipState.destroyed = false;
	shipState.ticksDestroyed = 0;
	placeable.position = pos;
	placeable.heading = 0.0;
	vel_x = 0.0;
	vel_y = 0.0;
	PlayerStats_reset();
	Audio_play_sample(&sample01);
}

void Ship_set_position(Position pos)
{
	placeable.position = pos;
}

void Ship_set_god_mode(bool enabled)
{
	godMode = enabled;
}

bool Ship_is_destroyed(void)
{
	return shipState.destroyed;
}

static bool pendingCrossZoneRespawn = false;

void Ship_set_pending_cross_zone_respawn(bool pending)
{
	pendingCrossZoneRespawn = pending;
}

bool Ship_has_pending_cross_zone_respawn(void)
{
	return pendingCrossZoneRespawn;
}

static double get_heading(const bool n, const bool s,
	const bool e, const bool w)
{
	if (n) {
		if (e)
			return 45.0;
		else if (w)
			return 315.0;
		else
			return 0.0;
	}
	if (s) {
		if (e)
			return 135.0;
		else if (w)
			return 225.0;
		else
			return 180.0;
	}
	if (e)
		return 90.0;
	if (w)
		return 270.0;

	return 0.0;
}
