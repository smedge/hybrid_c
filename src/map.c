#include "map.h"

#include <string.h>
#include "view.h"
#include "render.h"
#include "color.h"

static MapCell *map[MAP_SIZE][MAP_SIZE];

static MapCell emptyCell = {true, false, {0,0,0,0}, {0,0,0,0}};
static MapCell cell001 = {false, false, {20,0,20,255}, {128,0,128,255}};
static MapCell cell002 = {false, true, {10,20,20,255}, {64,128,128,255}};

static RenderableComponent renderable = {Map_render};
static CollidableComponent collidable = {{0.0, 0.0, 0.0, 0.0}, false, Map_collide};

static void initialize_map_data(void);
static void initialize_map_entity(void);
static void set_map_cell(int x, int y, MapCell *cell);
static void render_cell(int x, int y, float outlineThickness);
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
	for (x = 0; x < MAP_SIZE; x++)
		for (y = 0; y < MAP_SIZE; y++)
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
	int cell_min_x = (int)(world_min_x / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	int cell_max_x = (int)(world_max_x / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	int cell_min_y = (int)(world_min_y / MAP_CELL_SIZE) + HALF_MAP_SIZE;
	int cell_max_y = (int)(world_max_y / MAP_CELL_SIZE) + HALF_MAP_SIZE;

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
	MapCell mapCell = *map[x][y];
	if (mapCell.empty)
		return;

	float ax = (float)(x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	float ay = (float)(y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
	float bx = ax + MAP_CELL_SIZE;
	float by = ay + MAP_CELL_SIZE;

	MapCell *nPtr = map[x][y+1], *ePtr = map[x+1][y];
	MapCell *sPtr = map[x][y-1], *wPtr = map[x-1][y];

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
	   On edges adjacent to a different non-empty type, only the
	   circuitPattern cell draws (using the non-circuit cell's color).
	   This keeps corners covered within one cell and avoids double-draw. */

#define EDGE_DRAW(ptr, QAX, QAY, QBX, QBY) \
	if ((ptr)->empty) { \
		Render_quad_absolute(QAX, QAY, QBX, QBY, or_, og, ob, oa); \
	} else if ((ptr) != map[x][y]) { \
		if (mapCell.circuitPattern) { \
			ColorFloat bc = Color_rgb_to_float(&(ptr)->outlineColor); \
			Render_quad_absolute(QAX, QAY, QBX, QBY, \
				bc.red, bc.green, bc.blue, bc.alpha); \
		} else if (!(ptr)->circuitPattern) { \
			Render_quad_absolute(QAX, QAY, QBX, QBY, or_, og, ob, oa); \
		} \
	}

	EDGE_DRAW(nPtr, ax, by - t, n_bx, by)
	EDGE_DRAW(ePtr, bx - t, ay, bx, e_by)
	EDGE_DRAW(sPtr, s_ax, ay, bx, ay + t)
	EDGE_DRAW(wPtr, ax, w_ay, ax + t, by)

#undef EDGE_DRAW

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
