#include "map.h"

#include <SDL2/SDL_opengl.h>
#include "entity.h"
#include "view.h"

static MapCell *map[MAP_SIZE][MAP_SIZE];

static MapCell emptyCell = {true, {0,0,0,0}, {0,0,0,0}};
static MapCell cell001 = {false, {20,0,20,255}, {128,0,128,255}};

static void initialize_map_data(void);
static void initialize_map_entity(void);
static void set_map_cell(int x, int y, MapCell *cell);
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

	set_map_cell(1, 1, &cell001);

	set_map_cell(1, -1, &cell001);

	set_map_cell(-1, 1, &cell001);
	
	set_map_cell(-1, -1, &cell001);

	set_map_cell(9, 9, &cell001);
	set_map_cell(9, 10, &cell001);
	set_map_cell(9, 11, &cell001);
	set_map_cell(10, 9, &cell001);
	set_map_cell(10, 10, &cell001);
	set_map_cell(10, 11, &cell001);
	set_map_cell(11, 9, &cell001);
	set_map_cell(11, 10, &cell001);
	set_map_cell(11, 11, &cell001);

}

static void initialize_map_entity(void)
{
	int id = Entity_create_entity(COMPONENT_PLACEABLE | 
									COMPONENT_RENDERABLE);

	RenderableComponent renderable;
	renderable.render = Map_render;
	Entity_add_renderable(id, renderable);
}

static void set_map_cell(int x, int y, MapCell *cell) {
	if (x==0 || y==0)
		return;

	if (x < 0)
		x--;
	if (y < 0)
		y--;

	map[x+HALF_MAP_SIZE][y+HALF_MAP_SIZE] = cell;
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
	MapCell mapCell = *map[x][y];
	if (mapCell.empty)
		return;

	View view = View_get_view();

	double lineWidth = MAP_MIN_LINE_SIZE * view.scale * 10.0;
	if (lineWidth < MAP_MIN_LINE_SIZE)
		lineWidth = MAP_MIN_LINE_SIZE;

	glLineWidth(lineWidth);

	float ax = (x - HALF_MAP_SIZE) * MAP_CELL_SIZE; // TODO USE
	float ay = (y - HALF_MAP_SIZE) * MAP_CELL_SIZE; // GL HERE INSTEAD 
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
	
	MapCell eastCell = *map[x+1][y];
	MapCell northCell = *map[x][y+1];
	MapCell westCell = *map[x-1][y];
	MapCell southCell = *map[x][y-1];

	if (northCell.empty)
	{
		glBegin(GL_LINE_STRIP);
		glVertex2f(ax, by);
	 	glVertex2f(bx, by);
		glEnd();
	}
	if (eastCell.empty)
	{
		glBegin(GL_LINE_STRIP);
		glVertex2f(bx, by);
	 	glVertex2f(bx, ay);
		glEnd();
	}
	if (southCell.empty)
	{
		glBegin(GL_LINE_STRIP);
		glVertex2f(bx, ay);;
	 	glVertex2f(ax, ay);
		glEnd();
	}
	if (westCell.empty)
	{
		glBegin(GL_LINE_STRIP);
		glVertex2f(ax, ay);
	 	glVertex2f(ax, by);
		glEnd();
	}
}