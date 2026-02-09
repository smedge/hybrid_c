#include "map.h"

#include "view.h"
#include "render.h"
#include "color.h"

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
static int correctTruncation(int i);

void Map_initialize(void)
{
	initialize_map_entity();
	initialize_map_data();
}

static void initialize_map_entity(void)
{
	Entity entity = Entity_initialize_entity();
	entity.renderable = &renderable;
	entity.collidable = &collidable;

	Entity_add_entity(entity);
}

static void initialize_map_data(void)
{
	unsigned int i = 0, j = 0;
	for(i = 0; i < MAP_SIZE; i++) {
		for(j = 0; j < MAP_SIZE; j++) {
			map[i][j] = &emptyCell;
		}
	}

	set_map_cell(2, 2, &cell001);

	set_map_cell(2, -2, &cell002);

	set_map_cell(-2, 2, &cell001);

	set_map_cell(-2, -2, &cell001);



	set_map_cell(7, 7, &cell001);
	set_map_cell(7, 8, &cell001);
	set_map_cell(7, 9, &cell001);
	set_map_cell(7, 10, &cell001);

	set_map_cell(8, 7, &cell001);
	set_map_cell(8, 8, &cell001);
	set_map_cell(8, 9, &cell002);
	set_map_cell(8, 10, &cell001);

	set_map_cell(9, 7, &cell001);
	set_map_cell(9, 8, &cell001);
	set_map_cell(9, 9, &cell001);
	set_map_cell(9, 10, &cell001);

	set_map_cell(10, 7, &cell001);
	set_map_cell(10, 8, &cell001);
	set_map_cell(10, 9, &cell001);
	set_map_cell(10, 10, &cell001);



	set_map_cell(7, -7, &cell001);
	set_map_cell(7, -8, &cell001);
	set_map_cell(7, -9, &cell002);
	set_map_cell(7, -10, &cell001);

	set_map_cell(8, -7, &cell001);
	set_map_cell(8, -8, &cell001);
	set_map_cell(8, -9, &cell001);
	set_map_cell(8, -10, &cell001);

	set_map_cell(9, -7, &cell001);
	set_map_cell(9, -8, &cell001);
	set_map_cell(9, -9, &cell002);
	set_map_cell(9, -10, &cell001);

	set_map_cell(10, -7, &cell001);
	set_map_cell(10, -8, &cell001);
	set_map_cell(10, -9, &cell001);
	set_map_cell(10, -10, &cell001);



	set_map_cell(-7, 7, &cell001);
	set_map_cell(-7, 8, &cell001);
	set_map_cell(-7, 9, &cell001);
	set_map_cell(-7, 10, &cell001);

	set_map_cell(-8, 7, &cell002);
	set_map_cell(-8, 8, &cell001);
	set_map_cell(-8, 9, &cell001);
	set_map_cell(-8, 10, &cell001);

	set_map_cell(-9, 7, &cell001);
	set_map_cell(-9, 8, &cell001);
	set_map_cell(-9, 9, &cell001);
	set_map_cell(-9, 10, &cell001);

	set_map_cell(-10, 7, &cell001);
	set_map_cell(-10, 8, &cell002);
	set_map_cell(-10, 9, &cell002);
	set_map_cell(-10, 10, &cell001);



	set_map_cell(-7, -7, &cell002);
	set_map_cell(-7, -8, &cell001);
	set_map_cell(-7, -9, &cell001);
	set_map_cell(-7, -10, &cell001);

	set_map_cell(-8, -7, &cell001);
	set_map_cell(-8, -8, &cell001);
	set_map_cell(-8, -9, &cell001);
	set_map_cell(-8, -10, &cell001);

	set_map_cell(-9, -7, &cell001);
	set_map_cell(-9, -8, &cell002);
	set_map_cell(-9, -9, &cell001);
	set_map_cell(-9, -10, &cell001);

	set_map_cell(-10, -7, &cell001);
	set_map_cell(-10, -8, &cell001);
	set_map_cell(-10, -9, &cell001);
	set_map_cell(-10, -10, &cell001);
}

static void set_map_cell(int x, int y, MapCell *cell) {
	if (x==0 || y==0)
		return;

	if (x > 0)
		x--;
	if (y > 0)
		y--;

	map[x+HALF_MAP_SIZE][y+HALF_MAP_SIZE] = cell;
}

Collision Map_collide(const void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
{
	int corner1CellX = correctTruncation(boundingBox.aX / MAP_CELL_SIZE);
	int corner1CellY = correctTruncation(boundingBox.aY / MAP_CELL_SIZE);
	int corner3CellX = correctTruncation(boundingBox.bX / MAP_CELL_SIZE);
	int corner3CellY = correctTruncation(boundingBox.bY / MAP_CELL_SIZE);

	int map1X = corner1CellX + HALF_MAP_SIZE;
	int map1Y = corner1CellY + HALF_MAP_SIZE;
	int map3X = corner3CellX + HALF_MAP_SIZE;
	int map3Y = corner3CellY + HALF_MAP_SIZE;

	Collision collision;
	collision.collisionDetected = false;
	collision.solid = true;

	for (int x = map1X; x <= map3X; x++) {
		for (int y = map3Y; y <= map1Y; y++) {
			if (x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE) {
				if (!map[x][y]->empty) {
					collision.collisionDetected = true;
				}
			}
		}
	}

	return collision;
}

static int correctTruncation(int i)
{
	if (i < 0)
		return --i;
	else
		return i;
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

	float ax = (float)(x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	float ay = (float)(y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	float bx = ax + MAP_CELL_SIZE;
	float by = ay + MAP_CELL_SIZE;

	ColorFloat primaryColor = Color_rgb_to_float(&mapCell.primaryColor);
	Render_quad_absolute(ax, ay, bx, by,
		primaryColor.red, primaryColor.green, primaryColor.blue, primaryColor.alpha);

	/* Render outline edges where adjacent cell is empty */
	ColorFloat outlineColor = Color_rgb_to_float(&mapCell.outlineColor);
	float or_ = outlineColor.red, og = outlineColor.green;
	float ob = outlineColor.blue, oa = outlineColor.alpha;

	MapCell northCell = *map[x][y+1];
	MapCell eastCell = *map[x+1][y];
	MapCell southCell = *map[x][y-1];
	MapCell westCell = *map[x-1][y];

	if (northCell.empty)
		Render_line_segment(ax, by, bx, by, or_, og, ob, oa);
	if (eastCell.empty)
		Render_line_segment(bx, by, bx, ay, or_, og, ob, oa);
	if (southCell.empty)
		Render_line_segment(bx, ay, ax, ay, or_, og, ob, oa);
	if (westCell.empty)
		Render_line_segment(ax, ay, ax, by, or_, og, ob, oa);
}
