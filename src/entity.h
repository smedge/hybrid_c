#ifndef ENTITY_H
#define ENTITY_H

#define ENTITY_COUNT 256

typedef enum {
	COMPONENT_NONE = 0,
	COMPONENT_PLACEABLE = 1 << 1,
	COMPONENT_RENDERABLE = 1 << 2,
	COMPONENT_COLLIDABLE = 1 << 3
} Component;

#endif