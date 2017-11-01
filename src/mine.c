#include "mine.h"
#include "view.h"
#include "render.h"
#include "color.h"

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

	if (!sample01) {
		sample01 = Mix_LoadWAV("resources/sounds/bomb_set.wav");
		if (!sample01) {
			printf("FATAL ERROR: error loading sound for mine.\n");
			exit(-1);
		}
	}

	if (!sample02) {
		sample02 = Mix_LoadWAV("resources/sounds/bomb_explode.wav");
		if (!sample02) {
			printf("FATAL ERROR: error loading sound for mine.\n");
			exit(-1);
		}
	}

	if (!sample03) {
		sample03 = Mix_LoadWAV("resources/sounds/door.wav");
		if (!sample03) {
			printf("FATAL ERROR: error loading sound for mine.\n");
			exit(-1);
		}
	}

	color = Color_rgb_to_float(&COLOR);
	colorDark = Color_rgb_to_float(&COLOR_DARK);
	colorActive = Color_rgb_to_float(&COLOR_ACTIVE);
	colorWhite = Color_rgb_to_float(&COLOR_WHITE);
}

void Mine_cleanup()
{
	highestUsedIndex = 0;
	
	Mix_FreeChunk(sample01);
	sample01 = 0;

	Mix_FreeChunk(sample02);
	sample02 = 0;

	Mix_FreeChunk(sample03);
	sample03 = 0;
}

Collision Mine_collide(const void *entity, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	MineState* state = (MineState*)entity;

	Collision collision = {false, false};

	Position position = placeable->position;
	Rectangle thisBoundingBox = collidable.boundingBox;
	Rectangle transformedBoundingBox = {
		thisBoundingBox.aX + position.x,
		thisBoundingBox.aY + position.y,
		thisBoundingBox.bX + position.x,
		thisBoundingBox.bY + position.y,
	};

	if (Collision_aabb_test(transformedBoundingBox, boundingBox)) {
		collision.collisionDetected = true;
		if (state->exploding)
			collision.solid = true;
	}

	return collision;
}

void Mine_resolve(const void *entity, const Collision collision) 
{
	MineState* state = (MineState*)entity;

	if (state->destroyed || state->exploding)
		return;
	
	if (!state->active) {
		Mix_PlayChannel(-1, sample01, 0);

		state->active = true;
		state->ticksActive = 0;
	}
}

void Mine_update(const void *entity, const PlaceableComponent *placeable, const unsigned int ticks)
{
	MineState* state = (MineState*)entity;
	if (state->active)
	{
		state->ticksActive += ticks;
		if (state->ticksActive > TICKS_ACTIVE) {
			Mix_PlayChannel(-1, sample02, 0);
			state->active = false;
			state->exploding = true;
			state->ticksExploding = 0;
		}
		return;
	}
	
	if (state->exploding) {
		state->ticksExploding += ticks;
		if (state->ticksExploding > TICKS_EXPLODING) {
			state->exploding = false;
			state->destroyed = true;
			state->ticksDestroyed = 0;
		}
		return;
	}

	if (state->destroyed){
		state->ticksDestroyed += ticks;
		if (state->ticksDestroyed > TICKS_DESTROYED) {
			Mix_PlayChannel(-1, sample03, 0);
			state->active = false;
			state->exploding = false;
			state->destroyed = false;
		}
		return;
	}
}

void Mine_render(const void *entity, const PlaceableComponent *placeable) 
{
	MineState* state = (MineState*)entity;

	if (state->destroyed)
		return;

	if (state->exploding) {
		Rectangle explosion = {-250, 250, 250, -250};
		Render_quad(&placeable->position, 22.5, explosion, &colorWhite);
		Render_quad(&placeable->position, 67.5, explosion, &colorWhite);
		return;
	}

	Rectangle rectangle = {-10, 10, 10, -10};
	View view =  View_get_view();
	if (view.scale > 0.09)
		Render_quad(&placeable->position, 45.0, rectangle, &colorDark);

	if (state->active)
		Render_point(&placeable->position, 3.0, &colorActive);
	else
		Render_point(&placeable->position, 3.0, &color);

	//Render_bounding_box(&placeable->position, &collidable.boundingBox);
}
