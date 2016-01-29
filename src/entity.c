#include "entity.h"

static int entities[ENTITY_COUNT];
static PlaceableComponent placeables[ENTITY_COUNT];
static RenderableComponent renderables[ENTITY_COUNT];
static CollidableComponent collidables[ENTITY_COUNT];
static UserUpdatableComponent user_updatables[ENTITY_COUNT];

int Entity_create_entity(int componentMask)
{
	int i = 0;
	entities[i] = componentMask;
	return i;
}

void Entity_destroy_entity(int entityId) {}

void Entity_add_placeable(int entityId, PlaceableComponent placeable) 
{
	placeables[entityId] = placeable;
}

void Entity_add_renderable(int entityId, RenderableComponent renderable)
{
	renderables[entityId] = renderable;
}

void Entity_add_collidable(int entityId, CollidableComponent collidable) 
{
	collidables[entityId] = collidable;
}

void Entity_add_user_updatable(int entityId, UserUpdatableComponent updatable) {
	user_updatables[entityId] = updatable;
}

void System_update_user_input(const Input *input, const unsigned int ticks)
{
	for(int i = 0; i <= 0; i++) 
	{
		if ((entities[i] & USER_UPDATE_SYSTEM_MASK) != USER_UPDATE_SYSTEM_MASK)
			continue;
		user_updatables[i].update(input, ticks, &placeables[i]);
	}
}

void System_render_entities() 
{
	for(int i = 0; i <= 0; i++) 
	{
		if ((entities[i] & RENDER_SYSTEM_MASK) != RENDER_SYSTEM_MASK)
			continue;
		renderables[i].render(&placeables[i]);
	}
}