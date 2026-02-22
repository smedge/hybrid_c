#include "entity.h"
#include "spatial_grid.h"

#include <stdio.h>

static unsigned int highestIndex = 0;
static Entity entities[ENTITY_COUNT];
static unsigned int highestCollisionIndex = 0;
static ResolveCollisionCommand collisions[COLLISION_COUNT];

Entity Entity_initialize_entity() 
{
	Entity entity;
	entity.empty = true;
	entity.disabled = false;
	entity.state = 0;
	entity.placeable = 0;
	entity.renderable = 0;
	entity.collidable = 0;
	entity.dynamics = 0;
	entity.userUpdatable = 0;
	entity.aiUpdatable = 0;
	return entity;
}

Entity* Entity_add_entity(const Entity entity)
{
	unsigned int entityId;
	for(entityId = 0; entityId < ENTITY_COUNT; ++entityId)
	{
		if(entities[entityId].empty)
			break;
	}

	if (entityId >= ENTITY_COUNT) {
		printf("WARNING: Entity pool full (%d)\n", ENTITY_COUNT);
		return NULL;
	}

	if (highestIndex < entityId)
		highestIndex = entityId;

	entities[entityId].empty = false;
	entities[entityId].disabled = entity.disabled;
	entities[entityId].state = entity.state;
	entities[entityId].placeable = entity.placeable;
	entities[entityId].renderable = entity.renderable;
	entities[entityId].collidable = entity.collidable;
	entities[entityId].dynamics = entity.dynamics;
	entities[entityId].userUpdatable = entity.userUpdatable;
	entities[entityId].aiUpdatable = entity.aiUpdatable;

	return &entities[entityId];
}

void Entity_destroy_all(void)
{
	for (unsigned int entityId = 0; entityId < ENTITY_COUNT; ++entityId)
		entities[entityId].empty = true;
	highestIndex = 0;
}

void Entity_destroy(const unsigned int entityId)
{
	if (entityId >= ENTITY_COUNT)
		return;

	entities[entityId].empty = true;

	if (entityId == highestIndex) {
		while (highestIndex > 0 && entities[highestIndex].empty)
			highestIndex--;
	}
}

void Entity_recalculate_highest_index(void)
{
	while (highestIndex > 0 && entities[highestIndex].empty)
		highestIndex--;
}

void Entity_user_update_system(const Input *input, const unsigned int ticks)
{
	for(int i = 0; i <= highestIndex; i++) 
	{
		if (entities[i].empty || entities[i].disabled || entities[i].userUpdatable == 0 ||
				entities[i].placeable == 0)
			continue;

		entities[i].userUpdatable->update(input, ticks, entities[i].placeable);
	}
}

void Entity_ai_update_system(const unsigned int ticks)
{
	for(int i = 0; i <= highestIndex; i++) 
	{
		if (entities[i].empty || entities[i].disabled || entities[i].aiUpdatable == 0 ||
			entities[i].placeable == 0 || entities[i].state == 0)
			continue;

		entities[i].aiUpdatable->update(entities[i].state, entities[i].placeable, ticks);
	}
}

void Entity_render_system(void) 
{
	for(int i = 0; i <= highestIndex; i++) 
	{
		if (entities[i].empty || entities[i].disabled || entities[i].renderable == 0)	
			continue;
		
		entities[i].renderable->render(entities[i].state, entities[i].placeable);
	}
}

void Entity_collision_system(void)
{
	highestCollisionIndex = 0;

	for(int i = 0; i <= highestIndex; i++)
	{
		if (entities[i].empty || entities[i].disabled || entities[i].collidable == 0 ||
			entities[i].placeable == 0)
			continue;

		if (!entities[i].collidable->collidesWithOthers)
			continue;

		if (!SpatialGrid_is_active(entities[i].placeable->position.x,
								   entities[i].placeable->position.y))
			continue;

		for (int j = 0; j <= highestIndex; j++)
		{
			if (entities[j].empty || entities[j].disabled || entities[j].collidable == 0 ||
				entities[j].placeable == 0)
				continue;

			if (i == j)
				continue;

			if (!SpatialGrid_is_active(entities[j].placeable->position.x,
									   entities[j].placeable->position.y))
				continue;

			if (!(entities[i].collidable->mask & entities[j].collidable->layer))
				continue;

			// create a transformed bounding box for i
			Position position = entities[i].placeable->position;
			Rectangle boundingBox = entities[i].collidable->boundingBox;
			Rectangle transformedBoundingBox = Collision_transform_bounding_box(position, boundingBox);

			// call j's collide with i's transformed bounding box
			Collision collision = entities[j].collidable->collide(entities[j].state, 
																	entities[j].placeable, 
																	transformedBoundingBox);
			
			// call i's collision resolver if there was a collision
			if (collision.collisionDetected)
				Entity_create_collision_command(entities[i].collidable->resolve, 
					entities[i].state, collision);
		}
	}

	for (int i = 0; i < highestCollisionIndex; i++) 
	{
		ResolveCollisionCommand collision = collisions[i];
		collision.resolve(collision.state, collision.collision); 
	}
}

void Entity_create_collision_command(void (*resolve)(void *state, const Collision collision),
	void *state, Collision collision)
{
	if (highestCollisionIndex >= COLLISION_COUNT) {
		printf("WARNING: Collision command buffer full (%d)\n", COLLISION_COUNT);
		return;
	}
	collisions[highestCollisionIndex].resolve = resolve;
	collisions[highestCollisionIndex].state = state;
	collisions[highestCollisionIndex].collision = collision;
	highestCollisionIndex++;
}
