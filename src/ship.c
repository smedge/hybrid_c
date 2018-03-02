#include "ship.h"
#include "view.h"
#include "render.h"
#include "sub_pea.h"
#include "color.h"
#include "shipstate.h"
#include "audio.h"

#include <SDL2/SDL_mixer.h>

static const double NORMAL_VELOCITY = 800.0;
static const double FAST_VELOCITY = 1600.0;
static const double SLOW_VELOCITY = 6400.0;  // warp speed right now, not slow

static Mix_Chunk *sample01 = 0;
static Mix_Chunk *sample02 = 0;

static const ColorRGB COLOR = {255, 0, 0, 255};
static ColorFloat color;

static PlaceableComponent placeable = {{0.0, 0.0}, 0.0};
static RenderableComponent renderable = {Ship_render};
static UserUpdatableComponent updatable = {Ship_update};
static CollidableComponent collidable = {{-20.0, 20.0, 20.0, -20.0}, true, Ship_collide, Ship_resolve};

static ShipState shipState = {true, 0};

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

	color = Color_rgb_to_float(&COLOR);

	Audio_load_sample(&sample01, "resources/sounds/statue_rise.wav");
	Audio_load_sample(&sample02, "resources/sounds/samus_die.wav");

	Sub_Pea_initialize(liveEntity);
}

void Ship_cleanup() 
{
	Sub_Pea_cleanup();

	placeable.position.x = 0.0;
	placeable.position.y = 0.0;
	placeable.heading = 0.0;

	Audio_unload_sample(&sample01);
	Audio_unload_sample(&sample02);
}

Collision Ship_collide(const void *entity, const PlaceableComponent *placeable, const Rectangle boundingBox) 
{
	Collision collision = {false, true};

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
	}

	return collision;
}

void Ship_resolve(const void *entity, const Collision collision)
{
	if (shipState.destroyed)
		return;

	if (collision.solid)
	{
		shipState.destroyed = true;
		Audio_play_sample(&sample02);
	}
}

void Ship_update(const Input *userInput, const unsigned int ticks, PlaceableComponent *placeable)
{
	double ticksNormalized = ticks / 1000.0;

	if (shipState.destroyed) {
		shipState.ticksDestroyed += ticks;
		
		if (shipState.ticksDestroyed >= DEATH_TIMER) {
			shipState.destroyed = false;
			shipState.ticksDestroyed = 0;
			placeable->position.x = 0.0;
			placeable->position.y = 0.0;
			placeable->heading = 0.0;

			Audio_play_sample(&sample01);
		}
	}
	else {
		double velocity;
		if (userInput->keyLShift)
			velocity = FAST_VELOCITY;
		else if (userInput->keyLControl)
			velocity = SLOW_VELOCITY;
		else
			velocity = NORMAL_VELOCITY;
	
		if (userInput->keyW)
			placeable->position.y += velocity * ticksNormalized;
		if (userInput->keyS)
			placeable->position.y -= velocity * ticksNormalized;
		if (userInput->keyD)
			placeable->position.x += velocity * ticksNormalized;
		if (userInput->keyA)
			placeable->position.x -= velocity * ticksNormalized;
	
		if (userInput->keyW || userInput->keyA || 
			userInput->keyS || userInput->keyD)
		{
			placeable->heading = get_heading(userInput->keyW, userInput->keyS, 
											userInput->keyD, userInput->keyA);
		}
	}

	Sub_Pea_update(userInput, ticks, placeable);
}

void Ship_render(const void *entity, const PlaceableComponent *placeable)
{
	if (!shipState.destroyed) {
		View view =  View_get_view();

		if (view.scale > 0.09)
			Render_triangle(&placeable->position, placeable->heading, &color);
		else
			Render_point(&placeable->position, 2.0, &color);
		
		//Render_bounding_box(&placeable->position, &collidable.boundingBox);
	}

	Sub_Pea_render();

}

Position Ship_get_position()
{
	return placeable.position;
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
