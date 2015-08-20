#ifndef ENTITY_H
#define ENTITY_H

#define ENTITY_COUNT 255

typedef enum {
	COMPONENT_NONE = 0,
	COMPONENT_POSITION = 1 << 1,
	COMPONENT_RENDERABLE = 1 << 2
} Component;

#endif