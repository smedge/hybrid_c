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
static void render_cell(const int x, const int y, const float outlineThickness);
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

bool Map_line_test_hit(double x0, double y0, double x1, double y1,
					   double *hit_x, double *hit_y)
{
	double minX = x0 < x1 ? x0 : x1;
	double maxX = x0 > x1 ? x0 : x1;
	double minY = y0 < y1 ? y0 : y1;
	double maxY = y0 > y1 ? y0 : y1;

	int cellMinX = correctTruncation((int)(minX / MAP_CELL_SIZE));
	int cellMaxX = correctTruncation((int)(maxX / MAP_CELL_SIZE));
	int cellMinY = correctTruncation((int)(minY / MAP_CELL_SIZE));
	int cellMaxY = correctTruncation((int)(maxY / MAP_CELL_SIZE));

	double best_t = 2.0;
	bool hit = false;

	for (int cx = cellMinX; cx <= cellMaxX; cx++) {
		for (int cy = cellMinY; cy <= cellMaxY; cy++) {
			int mx = cx + HALF_MAP_SIZE;
			int my = cy + HALF_MAP_SIZE;
			if (mx < 0 || mx >= MAP_SIZE || my < 0 || my >= MAP_SIZE)
				continue;
			if (map[mx][my]->empty)
				continue;

			Rectangle cellRect = {
				cx * MAP_CELL_SIZE,
				(cy + 1) * MAP_CELL_SIZE,
				(cx + 1) * MAP_CELL_SIZE,
				cy * MAP_CELL_SIZE
			};
			double t;
			if (Collision_line_aabb_test(x0, y0, x1, y1, cellRect, &t)) {
				if (t < best_t) {
					best_t = t;
					hit = true;
					if (best_t <= 0.0)
						goto done;
				}
			}
		}
	}

done:
	if (hit) {
		*hit_x = x0 + (x1 - x0) * best_t;
		*hit_y = y0 + (y1 - y0) * best_t;
	}
	return hit;
}

void Map_render()
{
	View view = View_get_view();
	float outlineThickness = 2.0f / (float)view.scale;
	if (outlineThickness < 2.0f) outlineThickness = 2.0f;

	unsigned int x = 0, y = 0;
	for(x = 0; x < MAP_SIZE; x++) {
		for(y = 0; y < MAP_SIZE; y++) {
			render_cell(x, y, outlineThickness);
		}
	}
}

static void render_cell(const int x, const int y, const float outlineThickness)
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

	float t = outlineThickness;
	if (northCell.empty)
		Render_quad_absolute(ax, by - t, bx, by, or_, og, ob, oa);
	if (eastCell.empty)
		Render_quad_absolute(bx - t, ay, bx, by, or_, og, ob, oa);
	if (southCell.empty)
		Render_quad_absolute(ax, ay, bx, ay + t, or_, og, ob, oa);
	if (westCell.empty)
		Render_quad_absolute(ax, ay, ax + t, by, or_, og, ob, oa);
}
