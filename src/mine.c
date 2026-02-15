#include "mine.h"
#include "sub_pea.h"
#include "sub_mgun.h"
#include "sub_mine.h"
#include "fragment.h"
#include "progression.h"
#include "view.h"
#include "render.h"
#include "color.h"
#include "audio.h"

#include <stdlib.h>
#include <SDL2/SDL_mixer.h>

#define MINE_COUNT 128
#define MINE_ROTATION 0.0
#define TICKS_ACTIVE 500
#define TICKS_EXPLODING 100
#define TICKS_DESTROYED 10000

static RenderableComponent renderable = {Mine_render};
static CollidableComponent collidable = {{-250.0, 250.0, 250.0, -250.0},
											true,
											COLLISION_LAYER_ENEMY,
											COLLISION_LAYER_PLAYER,
											Mine_collide, Mine_resolve};
static AIUpdatableComponent updatable = {Mine_update};

static const ColorRGB COLOR = {45, 45, 45, 255};
static const ColorRGB COLOR_DARK = {50, 50, 50, 255};
static const ColorRGB COLOR_ACTIVE = {255, 0, 0, 255};
static const ColorRGB COLOR_WHITE = {255, 255, 255, 255};

static ColorFloat color, colorDark, colorActive, colorWhite;

typedef struct {
	bool active;
	unsigned int ticksActive;
	bool destroyed;
	unsigned int ticksDestroyed;
	bool exploding;
	unsigned int ticksExploding;
	unsigned int blinkTimer;
	bool killedByPlayer;
} MineState;

static MineState mines[MINE_COUNT];
static PlaceableComponent placeables[MINE_COUNT];
static Entity *entityRefs[MINE_COUNT];
static int highestUsedIndex = 0;

static Mix_Chunk *sample01 = 0;
static Mix_Chunk *sample02 = 0;
static Mix_Chunk *sample03 = 0;

void Mine_initialize(Position position)
{
	if (highestUsedIndex == MINE_COUNT) {
		printf("FATAL ERROR: Too many mine entities.\n");
		exit(-1);
	}

	mines[highestUsedIndex].active = false;
	mines[highestUsedIndex].exploding = false;
	mines[highestUsedIndex].destroyed = false;
	mines[highestUsedIndex].ticksActive = 0;
	mines[highestUsedIndex].ticksExploding = 0;
	mines[highestUsedIndex].ticksDestroyed = 0;
	mines[highestUsedIndex].blinkTimer = rand() % 1000;
	mines[highestUsedIndex].killedByPlayer = false;

	placeables[highestUsedIndex].position = position;
	placeables[highestUsedIndex].heading = MINE_ROTATION;

	Entity entity = Entity_initialize_entity();
	entity.state = &mines[highestUsedIndex];
	entity.placeable = &placeables[highestUsedIndex];
	entity.renderable = &renderable;
	entity.collidable = &collidable;
	entity.aiUpdatable = &updatable;
	
	entityRefs[highestUsedIndex] = Entity_add_entity(entity);

	highestUsedIndex++;

	/* Load audio and colors once, not per-entity */
	if (!sample01) {
		Audio_load_sample(&sample01, "resources/sounds/bomb_set.wav");
		Audio_load_sample(&sample02, "resources/sounds/bomb_explode.wav");
		Audio_load_sample(&sample03, "resources/sounds/door.wav");

		color = Color_rgb_to_float(&COLOR);
		colorDark = Color_rgb_to_float(&COLOR_DARK);
		colorActive = Color_rgb_to_float(&COLOR_ACTIVE);
		colorWhite = Color_rgb_to_float(&COLOR_WHITE);
	}
}

void Mine_cleanup()
{
	for (int i = 0; i < highestUsedIndex; i++) {
		if (entityRefs[i]) {
			entityRefs[i]->empty = true;
			entityRefs[i] = NULL;
		}
	}
	highestUsedIndex = 0;

	Audio_unload_sample(&sample01);
	Audio_unload_sample(&sample02);
	Audio_unload_sample(&sample03);
}

Collision Mine_collide(const void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	MineState* mineState = (MineState*)state;

	Collision collision = {false, false};

	Position position = placeable->position;
	Rectangle thisBoundingBox = collidable.boundingBox;
	Rectangle transformedBoundingBox = Collision_transform_bounding_box(position, thisBoundingBox);

	if (Collision_aabb_test(transformedBoundingBox, boundingBox)) {
		collision.collisionDetected = true;
		if (mineState->exploding)
			collision.solid = true;
	}

	return collision;
}

void Mine_resolve(const void *state, const Collision collision)
{
	MineState* mineState = (MineState*)state;

	if (mineState->active || mineState->exploding || mineState->destroyed)
		return;

	Audio_play_sample(&sample01);
	mineState->active = true;
	mineState->ticksActive = 0;
}

void Mine_update(const void *state, const PlaceableComponent *placeable, const unsigned int ticks)
{
	MineState* mineState = (MineState*)state;

	if (!mineState->destroyed && !mineState->exploding) {
		Rectangle body = {-10.0, 10.0, 10.0, -10.0};
		Rectangle mineBody = Collision_transform_bounding_box(placeable->position, body);
		if (Sub_Pea_check_hit(mineBody) || Sub_Mgun_check_hit(mineBody) || Sub_Mine_check_hit(mineBody)) {
			Audio_play_sample(&sample02);
			mineState->active = false;
			mineState->exploding = true;
			mineState->ticksExploding = 0;
			mineState->killedByPlayer = true;
		}
	}

	if (mineState->active)
	{
		mineState->ticksActive += ticks;
		if (mineState->ticksActive > TICKS_ACTIVE) {
			Audio_play_sample(&sample02);
			mineState->active = false;
			mineState->exploding = true;
			mineState->ticksExploding = 0;
		}
		return;
	}
	
	if (mineState->exploding) {
		mineState->ticksExploding += ticks;
		if (mineState->ticksExploding > TICKS_EXPLODING) {
			mineState->exploding = false;
			mineState->destroyed = true;
			mineState->ticksDestroyed = 0;
			if (mineState->killedByPlayer && !Progression_is_unlocked(SUB_ID_MINE))
				Fragment_spawn(placeable->position, FRAG_TYPE_MINE);
		}
		return;
	}

	if (mineState->destroyed){
		mineState->ticksDestroyed += ticks;
		if (mineState->ticksDestroyed > TICKS_DESTROYED) {
			Audio_play_sample(&sample03);
			mineState->active = false;
			mineState->exploding = false;
			mineState->destroyed = false;
			mineState->killedByPlayer = false;
		}
		return;
	}

	mineState->blinkTimer += ticks;
	if (mineState->blinkTimer >= 1000)
		mineState->blinkTimer -= 1000;
}

void Mine_render(const void *state, const PlaceableComponent *placeable) 
{
	MineState* mineState = (MineState*)state;

	if (mineState->destroyed)
		return;

	if (mineState->exploding) {
		Rectangle explosion = {-250, 250, 250, -250};
		Render_quad(&placeable->position, 22.5, explosion, &colorWhite);
		Render_quad(&placeable->position, 67.5, explosion, &colorWhite);
		return;
	}

	Rectangle rectangle = {-10, 10, 10, -10};
	View view =  View_get_view();
	if (view.scale > 0.09)
		Render_quad(&placeable->position, 45.0, rectangle, &colorDark);

	float dh = 3.0f;
	if (view.scale > 0.001f && 0.5f / (float)view.scale > dh)
		dh = 0.5f / (float)view.scale;
	Rectangle dot = {-dh, dh, dh, -dh};
	if (mineState->active)
		Render_quad(&placeable->position, 45.0, dot, &colorActive);
	else if (mineState->blinkTimer < 100)
		Render_quad(&placeable->position, 45.0, dot, &colorActive);

	//Render_bounding_box(&placeable->position, &collidable.boundingBox);
}

void Mine_render_bloom_source(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		MineState *ms = &mines[i];
		PlaceableComponent *pl = &placeables[i];

		if (ms->destroyed)
			continue;

		if (ms->exploding) {
			Rectangle explosion = {-250, 250, 250, -250};
			Render_quad(&pl->position, 22.5, explosion, &colorWhite);
			Render_quad(&pl->position, 67.5, explosion, &colorWhite);
			continue;
		}

		View view = View_get_view();
		float dh = 3.0f;
		if (view.scale > 0.001f && 1.0f / (float)view.scale > dh)
			dh = 1.0f / (float)view.scale;
		Rectangle dot = {-dh, dh, dh, -dh};
		if (ms->active || ms->blinkTimer < 100)
			Render_quad(&pl->position, 45.0, dot, &colorActive);
	}
}

void Mine_reset_all(void)
{
	for (int i = 0; i < highestUsedIndex; i++) {
		mines[i].active = false;
		mines[i].exploding = false;
		mines[i].destroyed = false;
		mines[i].ticksActive = 0;
		mines[i].ticksExploding = 0;
		mines[i].ticksDestroyed = 0;
		mines[i].blinkTimer = rand() % 1000;
		mines[i].killedByPlayer = false;
	}
}
