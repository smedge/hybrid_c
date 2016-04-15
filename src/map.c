#include "map.h"

#include <SDL2/SDL_opengl.h>
#include "entity.h"
#include "view.h"

static MapCell *map[MAP_SIZE][MAP_SIZE];

static MapCell emptyCell = {true, {0,0,0,0}, {0,0,0,0}};
static MapCell cell001 = {false, {20,0,20,255}, {128,0,128,255}};
static MapCell cell002 = {false, {10,20,20,255}, {64,128,128,255}};

static RenderableComponent renderable = {Map_render};
static CollidableComponent collidable = {{0.0, 0.0, 0.0, 0.0}, false, Map_collide};

static void initialize_map_data(void);
static void initialize_map_entity(void);
static void set_map_cell(int x, int y, MapCell *cell);
static void render_cell(const int x, const int y);
 
void Map_initialize(void)
{
	initialize_map_entity();
	initialize_map_data();
}

static void initialize_map_entity(void)
{
	int id = Entity_create_entity(COMPONENT_PLACEABLE | 
									COMPONENT_RENDERABLE|
									COMPONENT_COLLIDABLE);

	Entity_add_renderable(id, &renderable);
	Entity_add_collidable(id, &collidable);
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

	set_map_cell(1, -1, &cell002);

	set_map_cell(-1, 1, &cell001);

	set_map_cell(-1, -1, &cell001);



	set_map_cell(9, 9, &cell001);
	set_map_cell(9, 10, &cell001);
	set_map_cell(9, 11, &cell001);
	set_map_cell(9, 12, &cell001);

	set_map_cell(10, 9, &cell001);
	set_map_cell(10, 10, &cell001);
	set_map_cell(10, 11, &cell002);
	set_map_cell(10, 12, &cell001);

	set_map_cell(11, 9, &cell001);
	set_map_cell(11, 10, &cell001);
	set_map_cell(11, 11, &cell001);
	set_map_cell(11, 12, &cell001);

	set_map_cell(12, 9, &cell001);
	set_map_cell(12, 10, &cell001);
	set_map_cell(12, 11, &cell001);
	set_map_cell(12, 12, &cell001);



	set_map_cell(9, -9, &cell001);
	set_map_cell(9, -10, &cell001);
	set_map_cell(9, -11, &cell002);
	set_map_cell(9, -12, &cell001);

	set_map_cell(10, -9, &cell001);
	set_map_cell(10, -10, &cell001);
	set_map_cell(10, -11, &cell001);
	set_map_cell(10, -12, &cell001);

	set_map_cell(11, -9, &cell001);
	set_map_cell(11, -10, &cell001);
	set_map_cell(11, -11, &cell002);
	set_map_cell(11, -12, &cell001);

	set_map_cell(12, -9, &cell001);
	set_map_cell(12, -10, &cell001);
	set_map_cell(12, -11, &cell001);
	set_map_cell(12, -12, &cell001);



	set_map_cell(-9, 9, &cell001);
	set_map_cell(-9, 10, &cell001);
	set_map_cell(-9, 11, &cell001);
	set_map_cell(-9, 12, &cell001);

	set_map_cell(-10, 9, &cell002);
	set_map_cell(-10, 10, &cell001);
	set_map_cell(-10, 11, &cell001);
	set_map_cell(-10, 12, &cell001);

	set_map_cell(-11, 9, &cell001);
	set_map_cell(-11, 10, &cell001);
	set_map_cell(-11, 11, &cell001);
	set_map_cell(-11, 12, &cell001);

	set_map_cell(-12, 9, &cell001);
	set_map_cell(-12, 10, &cell002);
	set_map_cell(-12, 11, &cell002);
	set_map_cell(-12, 12, &cell001);



	set_map_cell(-9, -9, &cell002);
	set_map_cell(-9, -10, &cell001);
	set_map_cell(-9, -11, &cell001);
	set_map_cell(-9, -12, &cell001);

	set_map_cell(-10, -9, &cell001);
	set_map_cell(-10, -10, &cell001);
	set_map_cell(-10, -11, &cell001);
	set_map_cell(-10, -12, &cell001);

	set_map_cell(-11, -9, &cell001);
	set_map_cell(-11, -10, &cell002);
	set_map_cell(-11, -11, &cell001);
	set_map_cell(-11, -12, &cell001);

	set_map_cell(-12, -9, &cell001);
	set_map_cell(-12, -10, &cell001);
	set_map_cell(-12, -11, &cell001);
	set_map_cell(-12, -12, &cell001);

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

bool Map_collide(Rectangle boundingBox) 
{
	// use int truncation to calculate cell
	int cellX = boundingBox.aX / MAP_CELL_SIZE;
	int cellY = boundingBox.aY / MAP_CELL_SIZE;

	// correct for truncation
	if (boundingBox.aX < 0.0)
		cellX--;
	if (boundingBox.aY < 0.0)
		cellY--;

	// check for emptiness
	if (map[cellX + HALF_MAP_SIZE][cellY + HALF_MAP_SIZE]->empty)
		return false;
	else 
		return true;
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

	// render the inside of the cell
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

	// render the outside of the cell
	ColorFloat outlineColor = Color_rgb_to_float(&mapCell.outlineColor);
	glColor4f(outlineColor.red, outlineColor.green, outlineColor.blue, outlineColor.alpha);
	
	MapCell northCell = *map[x][y+1];
	MapCell eastCell = *map[x+1][y];
	MapCell southCell = *map[x][y-1];
	MapCell westCell = *map[x-1][y];

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