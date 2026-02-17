#include "map.h"

#include <string.h>
#include "view.h"
#include "render.h"
#include "color.h"

static MapCell *map[MAP_SIZE][MAP_SIZE];

static MapCell emptyCell = {true, false, {0,0,0,0}, {0,0,0,0}};
static MapCell boundaryCell = {true, false, {0,0,0,0}, {0,0,0,0}};

/* Pool of dynamically-set cells for zone loader */
#define CELL_POOL_SIZE (MAP_SIZE * MAP_SIZE)
static MapCell cellPool[CELL_POOL_SIZE];
static int cellPoolCount = 0;

static RenderableComponent renderable = {Map_render};
static CollidableComponent collidable = {{0.0, 0.0, 0.0, 0.0}, false,
	COLLISION_LAYER_TERRAIN, 0,
	Map_collide};

static void initialize_map_entity(void);
static void render_cell(int x, int y, float outlineThickness);
static int correctTruncation(double v);

static inline const MapCell* get_cell_fast(int x, int y) {
	if (x < 0 || x >= MAP_SIZE || y < 0 || y >= MAP_SIZE)
		return &boundaryCell;
	return map[x][y];
}

void Map_initialize(void)
{
	initialize_map_entity();
	Map_clear();
}

/* --- Zone loader API --- */

void Map_clear(void)
{
	for (int i = 0; i < MAP_SIZE; i++)
		for (int j = 0; j < MAP_SIZE; j++)
			map[i][j] = &emptyCell;
	cellPoolCount = 0;
}

void Map_set_cell(int grid_x, int grid_y, const MapCell *cell)
{
	if (grid_x < 0 || grid_x >= MAP_SIZE || grid_y < 0 || grid_y >= MAP_SIZE)
		return;

	/* Check if this coordinate already has a pool cell — overwrite in place */
	MapCell *existing = map[grid_x][grid_y];
	if (existing >= cellPool && existing < cellPool + CELL_POOL_SIZE) {
		*existing = *cell;
		return;
	}

	if (cellPoolCount >= CELL_POOL_SIZE)
		return;
	cellPool[cellPoolCount] = *cell;
	map[grid_x][grid_y] = &cellPool[cellPoolCount];
	cellPoolCount++;
}

void Map_clear_cell(int grid_x, int grid_y)
{
	if (grid_x < 0 || grid_x >= MAP_SIZE || grid_y < 0 || grid_y >= MAP_SIZE)
		return;
	map[grid_x][grid_y] = &emptyCell;
}

const MapCell *Map_get_cell(int grid_x, int grid_y)
{
	if (grid_x < 0 || grid_x >= MAP_SIZE || grid_y < 0 || grid_y >= MAP_SIZE)
		return &boundaryCell;
	return map[grid_x][grid_y];
}

void Map_set_boundary_cell(const MapCell *cell)
{
	boundaryCell = *cell;
	boundaryCell.empty = false;
}

void Map_clear_boundary_cell(void)
{
	boundaryCell = (MapCell){true, false, {0,0,0,0}, {0,0,0,0}};
}

static void initialize_map_entity(void)
{
	Entity entity = Entity_initialize_entity();
	entity.renderable = &renderable;
	entity.collidable = &collidable;

	Entity_add_entity(entity);
}

Collision Map_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox)
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
			} else if (!boundaryCell.empty) {
				collision.collisionDetected = true;
			}
		}
	}

	return collision;
}

static int correctTruncation(double v)
{
	int i = (int)v;
	if (v < 0.0 && v != (double)i)
		return i - 1;
	return i;
}

bool Map_line_test_hit(double x0, double y0, double x1, double y1,
					   double *hit_x, double *hit_y)
{
	double minX = x0 < x1 ? x0 : x1;
	double maxX = x0 > x1 ? x0 : x1;
	double minY = y0 < y1 ? y0 : y1;
	double maxY = y0 > y1 ? y0 : y1;

	int cellMinX = correctTruncation(minX / MAP_CELL_SIZE);
	int cellMaxX = correctTruncation(maxX / MAP_CELL_SIZE);
	int cellMinY = correctTruncation(minY / MAP_CELL_SIZE);
	int cellMaxY = correctTruncation(maxY / MAP_CELL_SIZE);

	double best_t = 2.0;
	bool hit = false;

	for (int cx = cellMinX; cx <= cellMaxX; cx++) {
		for (int cy = cellMinY; cy <= cellMaxY; cy++) {
			int mx = cx + HALF_MAP_SIZE;
			int my = cy + HALF_MAP_SIZE;
			if (mx < 0 || mx >= MAP_SIZE || my < 0 || my >= MAP_SIZE) {
				if (boundaryCell.empty)
					continue;
			} else if (map[mx][my]->empty) {
				continue;
			}

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

static bool cells_match_visual(const MapCell *a, const MapCell *b)
{
	return a->circuitPattern == b->circuitPattern &&
		memcmp(&a->primaryColor, &b->primaryColor, sizeof(ColorRGB)) == 0 &&
		memcmp(&a->outlineColor, &b->outlineColor, sizeof(ColorRGB)) == 0;
}

void Map_render(const void *state, const PlaceableComponent *placeable)
{
	(void)state;
	(void)placeable;

	View view = View_get_view();
	float outlineThickness = 2.0f / (float)view.scale;
	if (outlineThickness < 2.0f) outlineThickness = 2.0f;

	/* View frustum culling — only render cells visible on screen */
	Screen screen = Graphics_get_screen();
	float half_w = (float)screen.width * 0.5f / (float)view.scale;
	float half_h = (float)screen.height * 0.5f / (float)view.scale;
	float cx = (float)view.position.x;
	float cy = (float)view.position.y;

	/* Visible cell range (unclamped) */
	int minX = correctTruncation((cx - half_w) / MAP_CELL_SIZE) + HALF_MAP_SIZE - 1;
	int maxX = correctTruncation((cx + half_w) / MAP_CELL_SIZE) + HALF_MAP_SIZE + 1;
	int minY = correctTruncation((cy - half_h) / MAP_CELL_SIZE) + HALF_MAP_SIZE - 1;
	int maxY = correctTruncation((cy + half_h) / MAP_CELL_SIZE) + HALF_MAP_SIZE + 1;

	/* Render boundary fill and edge outlines for out-of-bounds area */
	if (!boundaryCell.empty) {
		float map_left = (float)(-HALF_MAP_SIZE) * MAP_CELL_SIZE;
		float map_right = (float)(MAP_SIZE - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		float map_bottom = map_left;
		float map_top = map_right;

		float vl = cx - half_w, vr = cx + half_w;
		float vb = cy - half_h, vt = cy + half_h;

		ColorFloat bc = Color_rgb_to_float(&boundaryCell.primaryColor);
		float br = bc.red, bg = bc.green, bb = bc.blue, ba = bc.alpha;

		/* Fill quads — 4 strips framing the map */
		if (vt > map_top)
			Render_quad_absolute(vl, map_top > vb ? map_top : vb,
				vr, vt, br, bg, bb, ba);
		if (vb < map_bottom)
			Render_quad_absolute(vl, vb,
				vr, map_bottom < vt ? map_bottom : vt, br, bg, bb, ba);
		if (vl < map_left) {
			float y0 = vb > map_bottom ? vb : map_bottom;
			float y1 = vt < map_top ? vt : map_top;
			if (y1 > y0)
				Render_quad_absolute(vl, y0, map_left, y1, br, bg, bb, ba);
		}
		if (vr > map_right) {
			float y0 = vb > map_bottom ? vb : map_bottom;
			float y1 = vt < map_top ? vt : map_top;
			if (y1 > y0)
				Render_quad_absolute(map_right, y0, vr, y1, br, bg, bb, ba);
		}

		/* Edge outlines — boundary's inner border facing empty/non-matching cells */
		ColorFloat boc = Color_rgb_to_float(&boundaryCell.outlineColor);
		float bor = boc.red, bog = boc.green, bob = boc.blue, boa = boc.alpha;
		float t = outlineThickness;

		int eyMin = minY < 0 ? 0 : minY;
		int eyMax = maxY >= MAP_SIZE ? MAP_SIZE - 1 : maxY;
		int exMin = minX < 0 ? 0 : minX;
		int exMax = maxX >= MAP_SIZE ? MAP_SIZE - 1 : maxX;

		#define BOUNDARY_OUTLINE(gx, gy) \
			(map[gx][gy]->empty || !cells_match_visual(map[gx][gy], &boundaryCell))

		if (maxX >= MAP_SIZE) {
			for (int y = eyMin; y <= eyMax; y++) {
				if (BOUNDARY_OUTLINE(MAP_SIZE - 1, y)) {
					float ay = (float)(y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
					Render_quad_absolute(map_right, ay,
						map_right + t, ay + MAP_CELL_SIZE, bor, bog, bob, boa);
					if (y > 0 && !BOUNDARY_OUTLINE(MAP_SIZE - 1, y - 1))
						Render_quad_absolute(map_right, ay - t,
							map_right + t, ay, bor, bog, bob, boa);
					if (y < MAP_SIZE - 1 && !BOUNDARY_OUTLINE(MAP_SIZE - 1, y + 1))
						Render_quad_absolute(map_right, ay + MAP_CELL_SIZE,
							map_right + t, ay + MAP_CELL_SIZE + t, bor, bog, bob, boa);
				}
			}
		}
		if (minX < 0) {
			for (int y = eyMin; y <= eyMax; y++) {
				if (BOUNDARY_OUTLINE(0, y)) {
					float ay = (float)(y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
					Render_quad_absolute(map_left - t, ay,
						map_left, ay + MAP_CELL_SIZE, bor, bog, bob, boa);
					if (y > 0 && !BOUNDARY_OUTLINE(0, y - 1))
						Render_quad_absolute(map_left - t, ay - t,
							map_left, ay, bor, bog, bob, boa);
					if (y < MAP_SIZE - 1 && !BOUNDARY_OUTLINE(0, y + 1))
						Render_quad_absolute(map_left - t, ay + MAP_CELL_SIZE,
							map_left, ay + MAP_CELL_SIZE + t, bor, bog, bob, boa);
				}
			}
		}
		if (maxY >= MAP_SIZE) {
			for (int x = exMin; x <= exMax; x++) {
				if (BOUNDARY_OUTLINE(x, MAP_SIZE - 1)) {
					float ax = (float)(x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
					Render_quad_absolute(ax, map_top,
						ax + MAP_CELL_SIZE, map_top + t, bor, bog, bob, boa);
					if (x > 0 && !BOUNDARY_OUTLINE(x - 1, MAP_SIZE - 1))
						Render_quad_absolute(ax - t, map_top,
							ax, map_top + t, bor, bog, bob, boa);
					if (x < MAP_SIZE - 1 && !BOUNDARY_OUTLINE(x + 1, MAP_SIZE - 1))
						Render_quad_absolute(ax + MAP_CELL_SIZE, map_top,
							ax + MAP_CELL_SIZE + t, map_top + t, bor, bog, bob, boa);
				}
			}
		}
		if (minY < 0) {
			for (int x = exMin; x <= exMax; x++) {
				if (BOUNDARY_OUTLINE(x, 0)) {
					float ax = (float)(x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
					Render_quad_absolute(ax, map_bottom - t,
						ax + MAP_CELL_SIZE, map_bottom, bor, bog, bob, boa);
					if (x > 0 && !BOUNDARY_OUTLINE(x - 1, 0))
						Render_quad_absolute(ax - t, map_bottom - t,
							ax, map_bottom, bor, bog, bob, boa);
					if (x < MAP_SIZE - 1 && !BOUNDARY_OUTLINE(x + 1, 0))
						Render_quad_absolute(ax + MAP_CELL_SIZE, map_bottom - t,
							ax + MAP_CELL_SIZE + t, map_bottom, bor, bog, bob, boa);
				}
			}
		}

		/* Corner fills — patch t*t gaps where perpendicular outlines meet */
		if (maxX >= MAP_SIZE && maxY >= MAP_SIZE &&
			BOUNDARY_OUTLINE(MAP_SIZE - 1, MAP_SIZE - 1))
			Render_quad_absolute(map_right, map_top,
				map_right + t, map_top + t, bor, bog, bob, boa);
		if (minX < 0 && maxY >= MAP_SIZE &&
			BOUNDARY_OUTLINE(0, MAP_SIZE - 1))
			Render_quad_absolute(map_left - t, map_top,
				map_left, map_top + t, bor, bog, bob, boa);
		if (maxX >= MAP_SIZE && minY < 0 &&
			BOUNDARY_OUTLINE(MAP_SIZE - 1, 0))
			Render_quad_absolute(map_right, map_bottom - t,
				map_right + t, map_bottom, bor, bog, bob, boa);
		if (minX < 0 && minY < 0 &&
			BOUNDARY_OUTLINE(0, 0))
			Render_quad_absolute(map_left - t, map_bottom - t,
				map_left, map_bottom, bor, bog, bob, boa);

		#undef BOUNDARY_OUTLINE
	}

	/* Clamp to map bounds for per-cell iteration */
	if (minX < 0) minX = 0;
	if (minY < 0) minY = 0;
	if (maxX >= MAP_SIZE) maxX = MAP_SIZE - 1;
	if (maxY >= MAP_SIZE) maxY = MAP_SIZE - 1;

	for (int x = minX; x <= maxX; x++)
		for (int y = minY; y <= maxY; y++)
			render_cell(x, y, outlineThickness);
}

void Map_render_minimap(float center_x, float center_y,
	float screen_x, float screen_y, float size, float range)
{
	float half_range = range * 0.5f;
	float scale = size / range;

	/* World bounds visible on minimap */
	float world_min_x = center_x - half_range;
	float world_max_x = center_x + half_range;
	float world_min_y = center_y - half_range;
	float world_max_y = center_y + half_range;

	/* Convert to map indices */
	int cell_min_x = correctTruncation(world_min_x / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	int cell_max_x = correctTruncation(world_max_x / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	int cell_min_y = correctTruncation(world_min_y / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	int cell_max_y = correctTruncation(world_max_y / MAP_CELL_SIZE) + HALF_MAP_SIZE;

	/* Render boundary (out-of-bounds) fill on minimap */
	if (!boundaryCell.empty) {
		float map_left   = (float)(-HALF_MAP_SIZE) * MAP_CELL_SIZE;
		float map_right  = (float)(MAP_SIZE - HALF_MAP_SIZE) * MAP_CELL_SIZE;
		float map_bottom = map_left;
		float map_top    = map_right;

		/* Map edges in minimap screen coords */
		float ml_sx = screen_x + (map_left - world_min_x) * scale;
		float mr_sx = screen_x + (map_right - world_min_x) * scale;
		float mt_sy = screen_y + size - (map_top - world_min_y) * scale;
		float mb_sy = screen_y + size - (map_bottom - world_min_y) * scale;

		/* Minimap bounds */
		float mm_l = screen_x, mm_r = screen_x + size;
		float mm_t = screen_y, mm_b = screen_y + size;

		/* Clamp map edges to minimap */
		if (ml_sx < mm_l) ml_sx = mm_l;
		if (mr_sx > mm_r) mr_sx = mm_r;
		if (mt_sy < mm_t) mt_sy = mm_t;
		if (mb_sy > mm_b) mb_sy = mm_b;

		ColorFloat bc = Color_rgb_to_float(&boundaryCell.primaryColor);
		float br = bc.red * 8.0f;  if (br > 1.0f) br = 1.0f;
		float bg = bc.green * 8.0f; if (bg > 1.0f) bg = 1.0f;
		float bb = bc.blue * 8.0f;  if (bb > 1.0f) bb = 1.0f;

		/* Top strip (above map) */
		if (mt_sy > mm_t)
			Render_quad_absolute(mm_l, mm_t, mm_r, mt_sy, br, bg, bb, 1.0f);
		/* Bottom strip (below map) */
		if (mb_sy < mm_b)
			Render_quad_absolute(mm_l, mb_sy, mm_r, mm_b, br, bg, bb, 1.0f);
		/* Left strip */
		if (ml_sx > mm_l)
			Render_quad_absolute(mm_l, mt_sy, ml_sx, mb_sy, br, bg, bb, 1.0f);
		/* Right strip */
		if (mr_sx < mm_r)
			Render_quad_absolute(mr_sx, mt_sy, mm_r, mb_sy, br, bg, bb, 1.0f);
	}

	if (cell_min_x < 0) cell_min_x = 0;
	if (cell_min_y < 0) cell_min_y = 0;
	if (cell_max_x >= MAP_SIZE) cell_max_x = MAP_SIZE - 1;
	if (cell_max_y >= MAP_SIZE) cell_max_y = MAP_SIZE - 1;

	for (int x = cell_min_x; x <= cell_max_x; x++) {
		for (int y = cell_min_y; y <= cell_max_y; y++) {
			if (map[x][y]->empty)
				continue;

			/* World position of cell center */
			float wx = (float)(x - HALF_MAP_SIZE) * MAP_CELL_SIZE + MAP_CELL_SIZE * 0.5f;
			float wy = (float)(y - HALF_MAP_SIZE) * MAP_CELL_SIZE + MAP_CELL_SIZE * 0.5f;

			/* Screen position on minimap (UI coords: y-down) */
			float sx = screen_x + (wx - world_min_x) * scale;
			float sy = screen_y + size - (wy - world_min_y) * scale;

			float cell_px = MAP_CELL_SIZE * scale;
			float half_px = cell_px * 0.5f;

			ColorFloat c = Color_rgb_to_float(&map[x][y]->primaryColor);
			/* Brighten for minimap visibility */
			float br = c.red * 8.0f;  if (br > 1.0f) br = 1.0f;
			float bg = c.green * 8.0f; if (bg > 1.0f) bg = 1.0f;
			float bb = c.blue * 8.0f;  if (bb > 1.0f) bb = 1.0f;
			Render_quad_absolute(
				sx - half_px, sy - half_px,
				sx + half_px, sy + half_px,
				br, bg, bb, 1.0f);
		}
	}
}

/* --- Circuit board pattern generation --- */

static unsigned int circuit_rand(unsigned int *state)
{
	unsigned int s = *state;
	s ^= s << 13;
	s ^= s >> 17;
	s ^= s << 5;
	*state = s;
	return s;
}

static float circuit_rand_range(unsigned int *state, float min, float max)
{
	unsigned int r = circuit_rand(state);
	return min + (float)(r % 10000) / 10000.0f * (max - min);
}

/* Occupancy grid prevents traces from crossing */
#define CGRID 50

static unsigned char g_occupy[CGRID][CGRID];

static int grid_coord(float world, float origin)
{
	int g = (int)((world - origin) * 0.5f);
	if (g < 0) return 0;
	if (g >= CGRID) return CGRID - 1;
	return g;
}

/* Walk a line on the grid; mark=1 writes, mark=0 checks for conflict */
static int grid_walk(float wx0, float wy0, float wx1, float wy1,
	float ox, float oy, int margin, int do_mark)
{
	int x0 = grid_coord(wx0, ox), y0 = grid_coord(wy0, oy);
	int x1 = grid_coord(wx1, ox), y1 = grid_coord(wy1, oy);
	int dx = x1 > x0 ? x1 - x0 : x0 - x1;
	int dy = y1 > y0 ? y1 - y0 : y0 - y1;
	int steps = dx > dy ? dx : dy;
	if (steps == 0) steps = 1;

	for (int s = 0; s <= steps; s++) {
		int gx = x0 + (x1 - x0) * s / steps;
		int gy = y0 + (y1 - y0) * s / steps;
		for (int mx = -margin; mx <= margin; mx++) {
			for (int my = -margin; my <= margin; my++) {
				int cx = gx + mx, cy = gy + my;
				if (cx < 0 || cx >= CGRID || cy < 0 || cy >= CGRID)
					continue;
				if (do_mark)
					g_occupy[cx][cy] = 1;
				else if (g_occupy[cx][cy])
					return 1;
			}
		}
	}
	return 0;
}

static int path_conflicts(float *xs, float *ys, int n, float ox, float oy)
{
	for (int i = 0; i < n - 1; i++)
		if (grid_walk(xs[i], ys[i], xs[i+1], ys[i+1], ox, oy, 1, 0))
			return 1;
	return 0;
}

static void path_mark(float *xs, float *ys, int n, float ox, float oy)
{
	for (int i = 0; i < n - 1; i++)
		grid_walk(xs[i], ys[i], xs[i+1], ys[i+1], ox, oy, 2, 1);
}

static void render_circuit_pattern(int cellX, int cellY, float ax, float ay,
	const ColorFloat *traceColor, const ColorFloat *fillColor,
	int adjN, int adjE, int adjS, int adjW)
{
	const float TW = 1.5f;
	const float PAD_R = 2.5f;
	const float VIA_OUT = 3.5f;
	const float VIA_IN = 1.8f;
	const int CSEGS = 10;
	const float EPAD = 4.0f;
	const float CF = 5.0f; /* chamfer length */

	float ux0 = ax + EPAD, uy0 = ay + EPAD;
	float ux1 = ax + MAP_CELL_SIZE - EPAD;
	float uy1 = ay + MAP_CELL_SIZE - EPAD;
	float uw = ux1 - ux0, uh = uy1 - uy0;

	float tr = traceColor->red, tg = traceColor->green;
	float tb = traceColor->blue, ta = traceColor->alpha;
	float fr = fillColor->red, fg = fillColor->green;
	float fb = fillColor->blue, fa = fillColor->alpha;

	unsigned int seed = (unsigned int)(cellX * 73856093u) ^ (unsigned int)(cellY * 19349663u);
	if (seed == 0) seed = 1;

	memset(g_occupy, 0, sizeof(g_occupy));

	int numAttempts = 24 + (int)(circuit_rand(&seed) % 5);

	for (int i = 0; i < numAttempts; i++) {
		/* Consume consistent random values per iteration */
		int typeRoll = circuit_rand(&seed) % 10;
		float r1 = circuit_rand_range(&seed, 0.08f, 0.92f);
		float r2 = circuit_rand_range(&seed, 0.0f, 0.3f);
		float r3 = circuit_rand_range(&seed, 0.0f, 0.3f);
		float r4 = circuit_rand_range(&seed, 0.08f, 0.92f);
		float r5 = circuit_rand_range(&seed, 0.3f, 0.7f);
		int extStart = (circuit_rand(&seed) % 5 <= 2); /* 60% */
		int extEnd = (circuit_rand(&seed) % 5 <= 2);
		int busRoll = circuit_rand(&seed) % 10;
		float busSp = circuit_rand_range(&seed, 5.0f, 7.0f);
		int startMk = circuit_rand(&seed) % 5;
		int endMk = circuit_rand(&seed) % 5;

		int busCount = busRoll < 3 ? 1 : (busRoll < 7 ? 2 : 3);
		int startPad = 1, endPad = 1;

		float bxs[3][5], bys[3][5];
		int bn[3];

		if (typeRoll <= 1) {
			/* --- Horizontal straight / bus --- */
			float baseY = uy0 + r1 * uh;
			float x0 = ux0 + r2 * uw;
			float x1 = ux1 - r3 * uw;
			if (x1 - x0 < 15.0f) continue;

			if (extStart && adjW) { x0 = ax; startPad = 0; }
			if (extEnd && adjE) { x1 = ax + MAP_CELL_SIZE; endPad = 0; }

			for (int b = 0; b < busCount; b++) {
				float y = baseY + (float)b * busSp;
				if (y < uy0 || y > uy1) { busCount = b; break; }
				bxs[b][0] = x0; bys[b][0] = y;
				bxs[b][1] = x1; bys[b][1] = y;
				bn[b] = 2;
			}
		}
		else if (typeRoll <= 3) {
			/* --- Vertical straight / bus --- */
			float baseX = ux0 + r1 * uw;
			float y0 = uy0 + r2 * uh;
			float y1 = uy1 - r3 * uh;
			if (y1 - y0 < 15.0f) continue;

			if (extStart && adjS) { y0 = ay; startPad = 0; }
			if (extEnd && adjN) { y1 = ay + MAP_CELL_SIZE; endPad = 0; }

			for (int b = 0; b < busCount; b++) {
				float x = baseX + (float)b * busSp;
				if (x < ux0 || x > ux1) { busCount = b; break; }
				bxs[b][0] = x; bys[b][0] = y0;
				bxs[b][1] = x; bys[b][1] = y1;
				bn[b] = 2;
			}
		}
		else if (typeRoll <= 6) {
			/* --- L-shape: H then V, always 45° chamfer --- */
			float hY = uy0 + r1 * uh;
			float startX = ux0 + r2 * uw;
			float turnX = ux0 + r5 * uw;
			float endY = uy0 + r4 * uh;
			float vLen = endY - hY;
			float absVLen = vLen > 0 ? vLen : -vLen;
			if (turnX - startX < 12.0f || absVLen < 15.0f) continue;

			float vDir = vLen > 0 ? 1.0f : -1.0f;
			if (extStart && adjW) { startX = ax; startPad = 0; }
			if (vDir > 0 && extEnd && adjN)
				{ endY = ay + MAP_CELL_SIZE; endPad = 0; }
			if (vDir < 0 && extEnd && adjS)
				{ endY = ay; endPad = 0; }

			for (int b = 0; b < busCount; b++) {
				float offY = (float)b * busSp * vDir;
				float offT = -(float)b * busSp;
				float bHY = hY + offY;
				float bTX = turnX + offT;
				float bVStart = bHY + CF * vDir;

				if (bHY < uy0 || bHY > uy1 || bTX < ux0
					|| bTX - CF - startX < 5.0f
					|| (endY - bVStart) * vDir < 5.0f) {
					busCount = b; break;
				}

				bxs[b][0] = startX;   bys[b][0] = bHY;
				bxs[b][1] = bTX - CF; bys[b][1] = bHY;
				bxs[b][2] = bTX;      bys[b][2] = bHY + CF * vDir;
				bxs[b][3] = bTX;      bys[b][3] = endY;
				bn[b] = 4;
			}
		}
		else {
			/* --- L-shape: V then H, always 45° chamfer --- */
			float vX = ux0 + r1 * uw;
			float startY = uy0 + r2 * uh;
			float turnY = uy0 + r5 * uh;
			float endX = ux0 + r4 * uw;
			float hLen = endX - vX;
			float absHLen = hLen > 0 ? hLen : -hLen;
			float vLen = turnY - startY;
			if (vLen < 12.0f || absHLen < 15.0f) continue;

			float hDir = hLen > 0 ? 1.0f : -1.0f;
			if (extStart && adjS) { startY = ay; startPad = 0; }
			if (hDir > 0 && extEnd && adjE)
				{ endX = ax + MAP_CELL_SIZE; endPad = 0; }
			if (hDir < 0 && extEnd && adjW)
				{ endX = ax; endPad = 0; }

			for (int b = 0; b < busCount; b++) {
				float offX = -(float)b * busSp * hDir;
				float offT = -(float)b * busSp;
				float bVX = vX + offX;
				float bTY = turnY + offT;
				float bHStart = bVX + CF * hDir;

				if (bVX < ux0 || bVX > ux1 || bTY < uy0
					|| bTY - CF - startY < 5.0f
					|| (endX - bHStart) * hDir < 5.0f) {
					busCount = b; break;
				}

				bxs[b][0] = bVX;            bys[b][0] = startY;
				bxs[b][1] = bVX;            bys[b][1] = bTY - CF;
				bxs[b][2] = bVX + CF*hDir;  bys[b][2] = bTY;
				bxs[b][3] = endX;           bys[b][3] = bTY;
				bn[b] = 4;
			}
		}

		if (busCount < 1) continue;

		/* Check all bus traces against occupancy grid */
		int conflict = 0;
		for (int b = 0; b < busCount; b++) {
			if (path_conflicts(bxs[b], bys[b], bn[b], ax, ay))
				{ conflict = 1; break; }
		}
		if (conflict) continue;

		/* Mark and render all bus traces */
		for (int b = 0; b < busCount; b++) {
			path_mark(bxs[b], bys[b], bn[b], ax, ay);
			for (int j = 0; j < bn[b] - 1; j++)
				Render_thick_line(bxs[b][j], bys[b][j],
					bxs[b][j+1], bys[b][j+1], TW, tr, tg, tb, ta);
			for (int j = 1; j < bn[b] - 1; j++)
				Render_filled_circle(bxs[b][j], bys[b][j],
					TW * 0.6f, 6, tr, tg, tb, ta);
		}

		/* Endpoint markers */
		for (int b = 0; b < busCount; b++) {
			if (startPad) {
				if (busCount == 1 && startMk >= 2) {
					Render_filled_circle(bxs[b][0], bys[b][0],
						VIA_OUT, CSEGS, tr, tg, tb, ta);
					Render_filled_circle(bxs[b][0], bys[b][0],
						VIA_IN, CSEGS, fr, fg, fb, fa);
				} else {
					Render_filled_circle(bxs[b][0], bys[b][0],
						PAD_R, CSEGS, tr, tg, tb, ta);
				}
			}
			if (endPad) {
				int last = bn[b] - 1;
				if (busCount == 1 && endMk >= 2) {
					Render_filled_circle(bxs[b][last], bys[b][last],
						VIA_OUT, CSEGS, tr, tg, tb, ta);
					Render_filled_circle(bxs[b][last], bys[b][last],
						VIA_IN, CSEGS, fr, fg, fb, fa);
				} else {
					Render_filled_circle(bxs[b][last], bys[b][last],
						PAD_R, CSEGS, tr, tg, tb, ta);
				}
			}
		}
	}
}

/* --- Cell rendering --- */

static void render_cell(int x, int y, float outlineThickness)
{
	const MapCell *me = get_cell_fast(x, y);
	MapCell mapCell = *me;
	if (mapCell.empty)
		return;

	float ax = (float)(x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	float ay = (float)(y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	float bx = ax + MAP_CELL_SIZE;
	float by = ay + MAP_CELL_SIZE;

	const MapCell *nPtr = get_cell_fast(x, y + 1);
	const MapCell *ePtr = get_cell_fast(x + 1, y);
	const MapCell *sPtr = get_cell_fast(x, y - 1);
	const MapCell *wPtr = get_cell_fast(x - 1, y);

	/* Chamfer NE and SW corners of circuit cells when both edges face empty */
	float chamf = MAP_CELL_SIZE * 0.17f;
	int chamfer_ne = mapCell.circuitPattern && nPtr->empty && ePtr->empty;
	int chamfer_sw = mapCell.circuitPattern && sPtr->empty && wPtr->empty;

	/* Cell fill */
	ColorFloat primaryColor = Color_rgb_to_float(&mapCell.primaryColor);
	float pr = primaryColor.red, pg = primaryColor.green;
	float pb = primaryColor.blue, pa = primaryColor.alpha;

	if (chamfer_ne || chamfer_sw) {
		float vx[6], vy[6];
		int n = 0;
		vx[n] = bx; vy[n] = ay; n++;
		if (chamfer_ne) {
			vx[n] = bx;         vy[n] = by - chamf; n++;
			vx[n] = bx - chamf; vy[n] = by;         n++;
		} else {
			vx[n] = bx; vy[n] = by; n++;
		}
		vx[n] = ax; vy[n] = by; n++;
		if (chamfer_sw) {
			vx[n] = ax;         vy[n] = ay + chamf; n++;
			vx[n] = ax + chamf; vy[n] = ay;         n++;
		} else {
			vx[n] = ax; vy[n] = ay; n++;
		}
		BatchRenderer *batch = Graphics_get_batch();
		for (int i = 1; i < n - 1; i++)
			Batch_push_triangle_vertices(batch,
				vx[0], vy[0], vx[i], vy[i], vx[i+1], vy[i+1],
				pr, pg, pb, pa);
	} else {
		Render_quad_absolute(ax, ay, bx, by, pr, pg, pb, pa);
	}

	/* Circuit board pattern */
	if (mapCell.circuitPattern) {
		View view = View_get_view();
		if (view.scale >= 0.15) {
			int adjN = !nPtr->empty;
			int adjE = !ePtr->empty;
			int adjS = !sPtr->empty;
			int adjW = !wPtr->empty;
			ColorFloat outlineColorF = Color_rgb_to_float(&mapCell.outlineColor);
			render_circuit_pattern(x - HALF_MAP_SIZE, y - HALF_MAP_SIZE,
				ax, ay, &outlineColorF, &primaryColor,
				adjN, adjE, adjS, adjW);
		}
	}

	/* Outline edges */
	ColorFloat outlineColor = Color_rgb_to_float(&mapCell.outlineColor);
	float or_ = outlineColor.red, og = outlineColor.green;
	float ob = outlineColor.blue, oa = outlineColor.alpha;

	float t = outlineThickness;

	/* Shorten edges at chamfered corners */
	float n_bx = chamfer_ne ? bx - chamf : bx;
	float e_by = chamfer_ne ? by - chamf : by;
	float s_ax = chamfer_sw ? ax + chamf : ax;
	float w_ay = chamfer_sw ? ay + chamf : ay;

	/* Draw outline on empty-adjacent edges (own color).
	   Each cell draws its own border in its own color.
	   Circuit cells skip borders on edges touching solid cells
	   (the solid cell draws its own border on that edge instead). */

#define CELLS_MATCH(a, b) \
	((a)->circuitPattern == (b)->circuitPattern && \
	 memcmp(&(a)->primaryColor, &(b)->primaryColor, sizeof(ColorRGB)) == 0 && \
	 memcmp(&(a)->outlineColor, &(b)->outlineColor, sizeof(ColorRGB)) == 0)

#define EDGE_DRAW(ptr, QAX, QAY, QBX, QBY) \
	if ((ptr)->empty) { \
		Render_quad_absolute(QAX, QAY, QBX, QBY, or_, og, ob, oa); \
	} else if (!CELLS_MATCH(ptr, me)) { \
		if (!mapCell.circuitPattern || (ptr)->circuitPattern) { \
			Render_quad_absolute(QAX, QAY, QBX, QBY, or_, og, ob, oa); \
		} \
	}

	EDGE_DRAW(nPtr, ax, by - t, n_bx, by)
	EDGE_DRAW(ePtr, bx - t, ay, bx, e_by)
	EDGE_DRAW(sPtr, s_ax, ay, bx, ay + t)
	EDGE_DRAW(wPtr, ax, w_ay, ax + t, by)

	/* Concave corner fills — patch t×t gaps at inner L-corners where
	   both cardinal neighbors match (suppressing their edge) but the
	   diagonal cell is empty or (for non-circuit cells) a different type.
	   Circuit cells skip borders against solids, so they only need fills
	   when the diagonal is truly empty. */

#define CORNER_GAP(diag) \
	((diag)->empty || (!mapCell.circuitPattern && !CELLS_MATCH((diag), me)))

	if (!nPtr->empty && CELLS_MATCH(nPtr, me) &&
		!ePtr->empty && CELLS_MATCH(ePtr, me) &&
		CORNER_GAP(get_cell_fast(x + 1, y + 1)))
		Render_quad_absolute(bx - t, by - t, bx, by, or_, og, ob, oa);

	if (!nPtr->empty && CELLS_MATCH(nPtr, me) &&
		!wPtr->empty && CELLS_MATCH(wPtr, me) &&
		CORNER_GAP(get_cell_fast(x - 1, y + 1)))
		Render_quad_absolute(ax, by - t, ax + t, by, or_, og, ob, oa);

	if (!sPtr->empty && CELLS_MATCH(sPtr, me) &&
		!ePtr->empty && CELLS_MATCH(ePtr, me) &&
		CORNER_GAP(get_cell_fast(x + 1, y - 1)))
		Render_quad_absolute(bx - t, ay, bx, ay + t, or_, og, ob, oa);

	if (!sPtr->empty && CELLS_MATCH(sPtr, me) &&
		!wPtr->empty && CELLS_MATCH(wPtr, me) &&
		CORNER_GAP(get_cell_fast(x - 1, y - 1)))
		Render_quad_absolute(ax, ay, ax + t, ay + t, or_, og, ob, oa);

#undef CORNER_GAP

#undef EDGE_DRAW
#undef CELLS_MATCH

	/* Chamfer diagonal outlines — quads that join flush with edge outlines */
	if (chamfer_ne) {
		BatchRenderer *batch = Graphics_get_batch();
		Batch_push_triangle_vertices(batch,
			bx - chamf, by, bx, by - chamf,
			bx - t, by - chamf, or_, og, ob, oa);
		Batch_push_triangle_vertices(batch,
			bx - chamf, by, bx - t, by - chamf,
			bx - chamf, by - t, or_, og, ob, oa);
	}
	if (chamfer_sw) {
		BatchRenderer *batch = Graphics_get_batch();
		Batch_push_triangle_vertices(batch,
			ax, ay + chamf, ax + chamf, ay,
			ax + chamf, ay + t, or_, og, ob, oa);
		Batch_push_triangle_vertices(batch,
			ax, ay + chamf, ax + chamf, ay + t,
			ax + t, ay + chamf, or_, og, ob, oa);
	}
}
