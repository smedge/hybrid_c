#include <stdlib.h>
#include <stdio.h>

#include "mine.h"
#include "view.h"
#include "render.h"
#include "color.h"

#include <SDL2/SDL_mixer.h>

#define MINE_COUNT 16
#define MINE_ROTATION 0.0
#define TICKS_ACTIVE 1000

static RenderableComponent renderable = {Mine_render};
static CollidableComponent collidable = {{-150.0, 150.0, 150.0, -150.0}, true, Mine_collide, Mine_resolve};
static AIUpdatableComponent updatable = {Mine_update};

static ColorRGB color = {120, 120, 120, 255};
static ColorRGB colorActive = {255, 0, 0, 255};

typedef struct {
	bool active;
	unsigned int ticksActive;
} MineState;

static MineState mines[MINE_COUNT];
static PlaceableComponent placeables[MINE_COUNT];
static int highestUsedIndex = 0;

static Mix_Chunk *sample01 = 0;
static Mix_Chunk *sample02 = 0;

void Mine_initialize(Position position)
{
	int id = Entity_create_entity(COMPONENT_PLACEABLE | 
									COMPONENT_RENDERABLE |
									COMPONENT_COLLIDABLE |
									COMPONENT_AI_UPDATABLE);

	if (highestUsedIndex == MINE_COUNT) {
		printf("FATAL ERROR: Too many mine entities.\n");
		exit(-1);
	}

	mines[highestUsedIndex].active = false;
	mines[highestUsedIndex].ticksActive = 0;

	placeables[highestUsedIndex].position = position;
	placeables[highestUsedIndex].heading = MINE_ROTATION;
	
	Entity_add_state(id, &mines[highestUsedIndex]);
	Entity_add_placeable(id, &placeables[highestUsedIndex]);
	Entity_add_renderable(id, &renderable);
	Entity_add_collidable(id, &collidable);
	Entity_add_ai_updatable(id, &updatable);
	
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

}

void Mine_cleanup()
{
	highestUsedIndex = 0;
	
	Mix_FreeChunk(sample01);
	sample02 = 0;

	Mix_FreeChunk(sample02);
	sample02 = 0;
}

Collision Mine_collide(const void *entity, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	Position position = placeable->position;
	Rectangle thisBoundingBox = collidable.boundingBox;
	Rectangle transformedBoundingBox = {
		thisBoundingBox.aX + position.x,
		thisBoundingBox.aY + position.y,
		thisBoundingBox.bX + position.x,
		thisBoundingBox.bY + position.y,
	};

	Collision collision = {false, false};

	if (Collision_aabb_test(transformedBoundingBox, boundingBox)) {
		collision.collisionDetected = true;
	}

	return collision;
}

void Mine_resolve(const void *entity, const Collision collision) 
{
	MineState* state = (MineState*)entity;
	
	if (!state->active)
		Mix_PlayChannel(-1, sample01, 0);

	state->active = true;
	state->ticksActive = 0;
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
		}
	}
}

void Mine_render(const void *entity, const PlaceableComponent *placeable) 
{
	MineState* state = (MineState*)entity;

	ColorFloat colorFloat;
	if (state->active)
		colorFloat= Color_rgb_to_float(&colorActive);
	else
		colorFloat= Color_rgb_to_float(&color);

	Render_point(&placeable->position, colorFloat.red, colorFloat.green, 
			colorFloat.blue, colorFloat.alpha);

	Render_bounding_box(&placeable->position, &collidable.boundingBox);
}