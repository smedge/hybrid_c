#include "map.h"

#include "entity.h"

static MapCell *map[MAP_SIZE][MAP_SIZE];

static MapCell emptyCell = {true, {0,0,0,0}, {0,0,0,0}};
static MapCell cell001 = {false, {16,0,16,255}, {128,0,128,255}};

static void initialize_map_data(void);
static void initialize_map_entity(void);

void Map_initialize(void)
{
	initialize_map_data();
	initialize_map_entity();
}

static void initialize_map_data(void)
{
	unsigned int i = 0, j = 0;
	for(i = 0; i < MAP_SIZE; i++) {
		for(i = 0; i < MAP_SIZE; i++) {
			map[i][j] = &emptyCell;
		}
	}

	map[64][64] = &cell001;
}

static void initialize_map_entity(void)
{
	int id = Entity_create_entity(COMPONENT_PLACEABLE | 
									COMPONENT_RENDERABLE);

	RenderableComponent renderable;
	renderable.render = Map_render;
	Entity_add_renderable(id, renderable);
}

void Map_render()
{

}