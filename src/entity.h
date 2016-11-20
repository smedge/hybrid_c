#ifndef ENTITY_H
#define ENTITY_H

#include "input.h"
#include "position.h"
#include "collision.h"

#define ENTITY_COUNT 1024

#define USER_UPDATE_SYSTEM_MASK (COMPONENT_PLACEABLE | COMPONENT_PLAYER_UPDATABLE)
#define AI_UPDATE_SYSTEM_MASK (COMPONENT_PLACEABLE | COMPONENT_AI_UPDATABLE)
#define RENDER_SYSTEM_MASK (COMPONENT_RENDERABLE | COMPONENT_PLACEABLE)
#define COLLISION_SYSTEM_MASK (COMPONENT_COLLIDABLE | COMPONENT_PLACEABLE)

typedef enum {
	COMPONENT_NONE = 0,
	COMPONENT_PLACEABLE = 1 << 1,
	COMPONENT_RENDERABLE = 1 << 2,
	COMPONENT_COLLIDABLE = 1 << 3,
	COMPONENT_PLAYER_UPDATABLE = 1 << 4,
	COMPONENT_AI_UPDATABLE = 1 << 5,
	COMPONENT_USER_INTERFACE = 1 << 6
} Component;

typedef struct {
	Position position;
	double heading;
} PlaceableComponent;

typedef struct {
	void (*render)(const void *entity, const PlaceableComponent *placeable);
} RenderableComponent;

typedef struct {
	Rectangle boundingBox;
	bool collidesWithOthers;
	Collision (*collide)(const void *entity, const PlaceableComponent *placeable, const Rectangle boundingBox);
	void (*resolve)(const void *entity, const Collision collision);
} CollidableComponent;

typedef struct {
	double mass;
} DynamicsComponent;

typedef struct {
	void (*update)(const Input *input, const unsigned int ticks, 
					PlaceableComponent *placeable);
} UserUpdatableComponent;

typedef struct {
	void (*update)(const void *entity, const PlaceableComponent *placeable, const unsigned int ticks);
} AIUpdatableComponent;

int Entity_create_entity(int componentMask);
void Entity_destroy_all(void);
void Entity_destroy(const unsigned int entityId);
void Entity_add_state(const unsigned int entityId, void *state);
void Entity_add_placeable(const unsigned int entityId, PlaceableComponent *placeable);
void Entity_add_renderable(const unsigned int entityId, RenderableComponent *renderable);
void Entity_add_user_updatable(const unsigned int entityId, UserUpdatableComponent *updatable);
void Entity_add_ai_updatable(const unsigned int entityId, AIUpdatableComponent *updatable);
void Entity_add_collidable(const unsigned int entityId, CollidableComponent *collidable);

void Entity_user_update_system(const Input *input, const unsigned int ticks);
void Entity_ai_update_system(const unsigned int ticks);
void Entity_render_system(void);
void Entity_collision_system(void);

#endif