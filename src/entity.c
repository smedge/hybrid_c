#include "entity.h"

static unsigned int highestIndex = 0;
static Entity entities[ENTITY_COUNT];
static unsigned int highestCollisionIndex = 0;
static ResolveCollisionCommand collisions[COLLISION_COUNT];

Entity Entity_initialize_entity() 
{
	Entity entity;
	entity.state = 0;
	entity.placeable = 0;
	entity.renderable = 0;
	entity.collidable = 0;
	entity.dynamics = 0;
	entity.userUpdatable = 0;
	entity.aiUpdatable = 0;
	return entity;
}

void Entity_add_entity(const Entity entity) 
{
	unsigned int entityId;
	for(entityId = 0; entityId < ENTITY_COUNT; ++entityId)
	{
		if(entities[entityId].empty)
			break;
	}

	if (highestIndex < entityId)
		highestIndex = entityId;

	entities[entityId].empty = false;

	if (entity.state)
		entities[entityId].state = entity.state;

	if (entity.placeable)
		entities[entityId].placeable = entity.placeable;
	
	if (entity.renderable)
		entities[entityId].renderable = entity.renderable;

	if (entity.collidable)
		entities[entityId].collidable = entity.collidable;

	if (entity.dynamics)
		entities[entityId].dynamics = entity.dynamics;

	if (entity.userUpdatable)
		entities[entityId].userUpdatable = entity.userUpdatable;

	if (entity.aiUpdatable)
		entities[entityId].aiUpdatable = entity.aiUpdatable;
}

void Entity_destroy_all(void)
{
	unsigned int entityId;
	for(entityId = 0; entityId < ENTITY_COUNT; ++entityId)
	{
		Entity_destroy(entityId);
	}
}

void Entity_destroy(const unsigned int entityId) 
{
	entities[entityId].empty = true;
}

void Entity_user_update_system(const Input *input, const unsigned int ticks)
{
	for(int i = 0; i <= highestIndex; i++) 
	{
		if (entities[i].empty || entities[i].userUpdatable == 0 ||
			entities[i].placeable == 0)
			continue;

		entities[i].userUpdatable->update(input, ticks, entities[i].placeable);
	}
}

void Entity_ai_update_system(const unsigned int ticks)
{
	for(int i = 0; i <= highestIndex; i++) 
	{
		if (entities[i].empty || entities[i].aiUpdatable == 0 ||
			entities[i].placeable == 0 || entities[i].state == 0)
			continue;

		entities[i].aiUpdatable->update(entities[i].state, entities[i].placeable, ticks);
	}
}

void Entity_render_system(void) 
{
	for(int i = 0; i <= highestIndex; i++) 
	{
		if (entities[i].empty || entities[i].renderable == 0)	
			continue;
		
		entities[i].renderable->render(entities[i].state, entities[i].placeable);
	}
}

void Entity_collision_system(void)
{
	highestCollisionIndex = 0;

	for(int i = 0; i <= highestIndex; i++) 
	{
		if (entities[i].empty || entities[i].collidable == 0 ||
			entities[i].placeable == 0)	
			continue;

		if (!entities[i].collidable->collidesWithOthers)
			continue;

		for (int j = 0; j <= highestIndex; j++)
		{
			if (entities[j].empty || entities[j].collidable == 0)	
				continue;

			if (i == j)
				continue;

			// create a transformed bounding box for i
			Position position = entities[i].placeable->position;
			Rectangle boundingBox = entities[i].collidable->boundingBox;
			Rectangle transformedBoundingBox = {
				boundingBox.aX + position.x,
				boundingBox.aY + position.y,
				boundingBox.bX + position.x,
				boundingBox.bY + position.y,
			};

			// call j's collide with i's transformed bounding box
			Collision collision = entities[j].collidable->collide(entities[j].state, entities[j].placeable, transformedBoundingBox);
			if (collision.collisionDetected)
			{
				//entities[i].collidable->resolve(entities[i].state, collision);
				collisions[highestCollisionIndex].collision = collision;
				collisions[highestCollisionIndex].entity = &entities[i];
				highestCollisionIndex++;
			}
		}
	}

	for (int i = 0; i < highestCollisionIndex; i++) 
	{
		collisions[i].entity->collidable->resolve(collisions[i].entity->state, collisions[i].collision); 
	}


}
