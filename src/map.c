#include "map.h"

#include <SDL2/SDL_opengl.h>
#include "entity.h"
#include "view.h"

static MapCell *map[MAP_SIZE][MAP_SIZE];

static MapCell emptyCell = {true, {0,0,0,0}, {0,0,0,0}};
static MapCell cell001 = {false, {20,0,20,255}, {128,0,128,255}};

static void initialize_map_data(void);
static void initialize_map_entity(void);
static void render_cell(const int x, const int y);
 
void Map_initialize(void)
{
	initialize_map_data();
	initialize_map_entity();
}

static void initialize_map_data(void)
{
	unsigned int i = 0, j = 0;
	for(i = 0; i < MAP_SIZE; i++) {
		for(j = 0; j < MAP_SIZE; j++) {
			map[i][j] = &emptyCell;
		}
	}

	map[65][65] = &cell001;
	map[65][66] = &cell001;
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
	unsigned int x = 0, y = 0;
	for(x = 0; x < MAP_SIZE; x++) {
		for(y = 0; y < MAP_SIZE; y++) {
			render_cell(x,y);
		}
	}
}

static void render_cell(const int x, const int y)
{
	View view = View_get_view();

	glLineWidth(8.0 * view.scale);

	MapCell mapCell = *map[x][y];

	if (mapCell.empty)
		return;

	float ax = (x - 64.0) * MAP_CELL_SIZE; // TODO USE
	float ay = (y - 64.0) * MAP_CELL_SIZE; // GL HERE INSTEAD 
	float bx = ax + MAP_CELL_SIZE;
	float by = ay + MAP_CELL_SIZE;
	
	ColorFloat primaryColor = Color_rgb_to_float(&mapCell.primaryColor);
	glColor4f(primaryColor.red, primaryColor.green, primaryColor.blue, primaryColor.alpha);
	glBegin(GL_QUADS);
		glVertex2f(ax, ay);
		glVertex2f(ax, by);
		glVertex2f(bx, by);
		glVertex2f(bx, ay);
	glEnd();

	ColorFloat outlineColor = Color_rgb_to_float(&mapCell.outlineColor);
	glColor4f(outlineColor.red, outlineColor.green, outlineColor.blue, outlineColor.alpha);
	glBegin(GL_LINE_LOOP);
		glVertex2f(ax, ay);
		glVertex2f(ax, by);
		glVertex2f(bx, by);
		glVertex2f(bx, ay);
	glEnd();
}