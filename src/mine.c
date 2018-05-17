#include "mine.h"
#include "view.h"
#include "render.h"
#include "color.h"
#include "audio.h"

#include <stdlib.h>
#include <SDL2/SDL_mixer.h>

#define MINE_COUNT 64
#define MINE_ROTATION 0.0
#define TICKS_ACTIVE 500
#define TICKS_EXPLODING 100
#define TICKS_DESTROYED 10000

static RenderableComponent renderable = {Mine_render};
static CollidableComponent collidable = {{-250.0, 250.0, 250.0, -250.0},
											true, Mine_collide, Mine_resolve};
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
} MineState;

static MineState mines[MINE_COUNT];
static PlaceableComponent placeables[MINE_COUNT];
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

	placeables[highestUsedIndex].position = position;
	placeables[highestUsedIndex].heading = MINE_ROTATION;

	Entity entity = Entity_initialize_entity();
	entity.state = &mines[highestUsedIndex];
	entity.placeable = &placeables[highestUsedIndex];
	entity.renderable = &renderable;
	entity.collidable = &collidable;
	entity.aiUpdatable = &updatable;
	
	Entity_add_entity(entity);
	
	highestUsedIndex++;

	Audio_load_sample(&sample01, "resources/sounds/bomb_set.wav");
	Audio_load_sample(&sample02, "resources/sounds/bomb_explode.wav");
	Audio_load_sample(&sample03, "resources/sounds/door.wav");

	color = Color_rgb_to_float(&COLOR);
	colorDark = Color_rgb_to_float(&COLOR_DARK);
	colorActive = Color_rgb_to_float(&COLOR_ACTIVE);
	colorWhite = Color_rgb_to_float(&COLOR_WHITE);
}

void Mine_cleanup()
{
	highestUsedIndex = 0;

	Audio_unload_sample(&sample01);
	Audio_unload_sample(&sample02);
	Audio_unload_sample(&sample03);
}

void Mine_collide(const void *state, const PlaceableComponent *placeable, const Rectangle boundingBox,
	void (*resolve)(const void *state, const Collision collision), void *stateOther)
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
		Entity_create_collision_command(resolve, stateOther, collision);
	}
}

void Mine_resolve(const void *state, const Collision collision) 
{
	MineState* mineState = (MineState*)state;

	if (mineState->destroyed || mineState->exploding)
		return;
	
	if (!mineState->active) {
		Audio_play_sample(&sample01);

		mineState->active = true;
		mineState->ticksActive = 0;
	}
}

void Mine_update(const void *state, const PlaceableComponent *placeable, const unsigned int ticks)
{
	MineState* mineState = (MineState*)state;
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
		}
		return;
	}
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

	if (mineState->active)
		Render_point(&placeable->position, 3.0, &colorActive);
	else
		Render_point(&placeable->position, 3.0, &color);

	//Render_bounding_box(&placeable->position, &collidable.boundingBox);
}
