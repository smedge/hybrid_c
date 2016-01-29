#ifndef ENTITY_H
#define ENTITY_H

#include "input.h"
#include "position.h"

#define ENTITY_COUNT 256

#define USER_UPDATE_SYSTEM_MASK (COMPONENT_PLACEABLE | COMPONENT_PLAYER_UPDATABLE)
#define RENDER_SYSTEM_MASK (COMPONENT_RENDERABLE | COMPONENT_PLACEABLE)

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
 	void (*render)(const PlaceableComponent *placeable);
 } RenderableComponent;

 typedef struct { 
 	// bounding data
 } CollidableComponent;

 typedef struct {
 	void (*update)(const Input *input, const unsigned int ticks, 
 					PlaceableComponent *placeable);
 } UserUpdatableComponent;

int Entity_create_entity(int componentMask);
void Entity_destroy_entity(int entityId);
void Entity_add_placeable(int entityId, PlaceableComponent placeable);
void Entity_add_renderable(int entityId, RenderableComponent renderable);
void Entity_add_user_updatable(int entityId, UserUpdatableComponent updatable);
void Entity_add_collidable(int entityId);

void System_update_user_input(const Input *input, const unsigned int ticks);
void System_render_entities();

#endif