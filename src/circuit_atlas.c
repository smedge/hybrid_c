#include "circuit_atlas.h"

#include <math.h>
#include <string.h>
#include <OpenGL/gl3.h>

#include "map.h"
#include "view.h"
#include "graphics.h"
#include "render.h"
#include "color.h"

/* --- Atlas layout --- */

#define TILE_SIZE    512
#define ATLAS_COLS   5
#define ATLAS_ROWS   4
#define TILE_COUNT   20
#define ATLAS_WIDTH  (TILE_SIZE * ATLAS_COLS)   /* 2560 */
#define ATLAS_HEIGHT (TILE_SIZE * ATLAS_ROWS)   /* 2048 */

/* Connectivity classes:
   0 = island (0-edge)      4 base patterns
   1 = 1-edge (N)           4 base patterns
   2 = 2-edge adjacent (NE) 3 base patterns
   3 = 2-edge opposite (NS) 3 base patterns
   4 = 3-edge (NES)         3 base patterns
   5 = 4-edge (NESW)        3 base patterns */

static const int class_start[] = {0, 4, 8, 11, 14, 17};
static const int class_count[] = {4, 4, 3,  3,  3,  3};

/* Adjacency flags for each base tile (canonical orientation) */
static const struct { int adjN, adjE, adjS, adjW; } tile_adj[TILE_COUNT] = {
	/* Class 0: island */
	{0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0},
	/* Class 1: 1-edge N */
	{1,0,0,0}, {1,0,0,0}, {1,0,0,0}, {1,0,0,0},
	/* Class 2a: adjacent NE */
	{1,1,0,0}, {1,1,0,0}, {1,1,0,0},
	/* Class 2b: opposite NS */
	{1,0,1,0}, {1,0,1,0}, {1,0,1,0},
	/* Class 3: 3-edge NES */
	{1,1,1,0}, {1,1,1,0}, {1,1,1,0},
	/* Class 4: surrounded */
	{1,1,1,1}, {1,1,1,1}, {1,1,1,1},
};

/* conn_mask (NESW bits) → {class_index, base_rotation} */
typedef struct {
	int class_idx;
	int base_rot;
} ConnLookup;

static const ConnLookup conn_table[16] = {
	/* 0000 */ {0, 0},  /* island */
	/* 0001 */ {1, 3},  /* W only → 1-edge rot 270 */
	/* 0010 */ {1, 2},  /* S only → 1-edge rot 180 */
	/* 0011 */ {2, 2},  /* SW adj → 2a rot 180 */
	/* 0100 */ {1, 1},  /* E only → 1-edge rot 90 */
	/* 0101 */ {3, 1},  /* EW opp → 2b rot 90 */
	/* 0110 */ {2, 1},  /* ES adj → 2a rot 90 */
	/* 0111 */ {4, 1},  /* ESW → 3-edge rot 90 */
	/* 1000 */ {1, 0},  /* N only → 1-edge rot 0 */
	/* 1001 */ {2, 3},  /* NW adj → 2a rot 270 */
	/* 1010 */ {3, 0},  /* NS opp → 2b rot 0 */
	/* 1011 */ {4, 2},  /* NSW → 3-edge rot 180 */
	/* 1100 */ {2, 0},  /* NE adj → 2a rot 0 */
	/* 1101 */ {4, 3},  /* NEW → 3-edge rot 270 */
	/* 1110 */ {4, 0},  /* NES → 3-edge rot 0 */
	/* 1111 */ {5, 0},  /* surrounded */
};

/* --- Vertex format (matches TextVertex in text.c) --- */

typedef struct {
	float x, y;
	float r, g, b, a;
	float u, v;
} CircuitVertex;

#define CIRCUIT_MAX_VERTS 131072

/* --- Module state --- */

static GLuint s_atlas_tex;
static GLuint s_vao;
static GLuint s_vbo;
static CircuitVertex s_verts[CIRCUIT_MAX_VERTS];
static int s_vert_count;

/* --- UV helpers --- */

static const float TILE_U_SIZE = 1.0f / (float)ATLAS_COLS;  /* 0.2  */
static const float TILE_V_SIZE = 1.0f / (float)ATLAS_ROWS;  /* 0.25 */

static void transform_uv(float lu, float lv, int rotation, int mirror,
	float u0, float v0, float *out_u, float *out_v)
{
	/* Mirror: flip lu horizontally */
	if (mirror) lu = 1.0f - lu;

	/* Rotation (CW): transform local UV within tile */
	float flu, flv;
	switch (rotation & 3) {
	case 0: flu = lu;        flv = lv;        break;
	case 1: flu = lv;        flv = 1.0f - lu; break;
	case 2: flu = 1.0f - lu; flv = 1.0f - lv; break;
	case 3: flu = 1.0f - lv; flv = lu;        break;
	default: flu = lu; flv = lv; break;
	}

	*out_u = u0 + flu * TILE_U_SIZE;
	*out_v = v0 + flv * TILE_V_SIZE;
}

static inline void push_vertex(float x, float y,
	float r, float g, float b, float a, float u, float v)
{
	if (s_vert_count >= CIRCUIT_MAX_VERTS) return;
	s_verts[s_vert_count++] = (CircuitVertex){x, y, r, g, b, a, u, v};
}

/* Push a textured triangle with UV computed from world position */
static void push_tri(float x0, float y0, float x1, float y1,
	float x2, float y2, float ax, float ay,
	float r, float g, float b, float a,
	float u0, float v0, int rotation, int mirror)
{
	float inv = 1.0f / (float)MAP_CELL_SIZE;
	float lu0 = (x0 - ax) * inv, lv0 = (y0 - ay) * inv;
	float lu1 = (x1 - ax) * inv, lv1 = (y1 - ay) * inv;
	float lu2 = (x2 - ax) * inv, lv2 = (y2 - ay) * inv;

	float ou0, ov0, ou1, ov1, ou2, ov2;
	transform_uv(lu0, lv0, rotation, mirror, u0, v0, &ou0, &ov0);
	transform_uv(lu1, lv1, rotation, mirror, u0, v0, &ou1, &ov1);
	transform_uv(lu2, lv2, rotation, mirror, u0, v0, &ou2, &ov2);

	push_vertex(x0, y0, r, g, b, a, ou0, ov0);
	push_vertex(x1, y1, r, g, b, a, ou1, ov1);
	push_vertex(x2, y2, r, g, b, a, ou2, ov2);
}

/* --- Atlas generation --- */

void CircuitAtlas_initialize(void)
{
	/* Create atlas texture */
	glGenTextures(1, &s_atlas_tex);
	glBindTexture(GL_TEXTURE_2D, s_atlas_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
		ATLAS_WIDTH, ATLAS_HEIGHT, 0,
		GL_RED, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Create temporary FBO */
	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, s_atlas_tex, 0);

	/* Clear entire atlas to black */
	glViewport(0, 0, ATLAS_WIDTH, ATLAS_HEIGHT);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	/* Render each tile into its sub-region */
	Mat4 identity = Mat4_identity();
	Mat4 proj = Mat4_ortho(0.0f, (float)MAP_CELL_SIZE,
		0.0f, (float)MAP_CELL_SIZE, -1.0f, 1.0f);

	for (int i = 0; i < TILE_COUNT; i++) {
		int col = i % ATLAS_COLS;
		int row = i / ATLAS_COLS;
		glViewport(col * TILE_SIZE, row * TILE_SIZE,
			TILE_SIZE, TILE_SIZE);

		/* Use tile index as seed source (cellX) */
		Map_render_circuit_pattern_for_atlas(
			i + 1, i * 7 + 3,
			0.0f, 0.0f,
			tile_adj[i].adjN, tile_adj[i].adjE,
			tile_adj[i].adjS, tile_adj[i].adjW);

		Render_flush(&proj, &identity);
	}

	/* Cleanup FBO, restore state */
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	/* Restore viewport */
	int draw_w, draw_h;
	Graphics_get_drawable_size(&draw_w, &draw_h);
	glViewport(0, 0, draw_w, draw_h);

	/* Create VAO/VBO for circuit quad rendering */
	glGenVertexArrays(1, &s_vao);
	glGenBuffers(1, &s_vbo);
	glBindVertexArray(s_vao);
	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);

	/* position: location 0 */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
		sizeof(CircuitVertex), (void *)0);

	/* color: location 1 */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
		sizeof(CircuitVertex), (void *)(2 * sizeof(float)));

	/* texcoord: location 2 */
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
		sizeof(CircuitVertex), (void *)(6 * sizeof(float)));

	glBindVertexArray(0);
}

void CircuitAtlas_cleanup(void)
{
	glDeleteTextures(1, &s_atlas_tex);
	glDeleteBuffers(1, &s_vbo);
	glDeleteVertexArrays(1, &s_vao);
	s_atlas_tex = 0;
	s_vao = 0;
	s_vbo = 0;
}

/* --- Per-frame rendering --- */

/* Floor-toward-negative-infinity integer truncation (matches map.c) */
static int floor_int(double v)
{
	int i = (int)v;
	if (v < 0.0 && v != (double)i)
		i--;
	return i;
}

void CircuitAtlas_render(void)
{
	if (!Map_get_circuit_traces()) return;

	View view = View_get_view();
	if (view.scale < 0.15) return;

	Screen screen = Graphics_get_screen();
	float half_w = (float)screen.width * 0.5f / (float)view.scale;
	float half_h = (float)screen.height * 0.5f / (float)view.scale;
	float cx = (float)view.position.x;
	float cy = (float)view.position.y;

	int minX = floor_int((cx - half_w) / MAP_CELL_SIZE) + HALF_MAP_SIZE - 1;
	int maxX = floor_int((cx + half_w) / MAP_CELL_SIZE) + HALF_MAP_SIZE + 1;
	int minY = floor_int((cy - half_h) / MAP_CELL_SIZE) + HALF_MAP_SIZE - 1;
	int maxY = floor_int((cy + half_h) / MAP_CELL_SIZE) + HALF_MAP_SIZE + 1;

	if (minX < 0) minX = 0;
	if (minY < 0) minY = 0;
	if (maxX >= MAP_SIZE) maxX = MAP_SIZE - 1;
	if (maxY >= MAP_SIZE) maxY = MAP_SIZE - 1;

	s_vert_count = 0;

	for (int x = minX; x <= maxX; x++) {
		for (int y = minY; y <= maxY; y++) {
			const MapCell *me = Map_get_cell(x, y);
			if (me->empty || !me->circuitPattern)
				continue;

			/* World-space cell corners */
			float ax = (float)(x - HALF_MAP_SIZE) * MAP_CELL_SIZE;
			float ay = (float)(y - HALF_MAP_SIZE) * MAP_CELL_SIZE;
			float bx = ax + (float)MAP_CELL_SIZE;
			float by = ay + (float)MAP_CELL_SIZE;

			/* Neighbor lookup */
			const MapCell *nPtr = Map_get_cell(x, y + 1);
			const MapCell *ePtr = Map_get_cell(x + 1, y);
			const MapCell *sPtr = Map_get_cell(x, y - 1);
			const MapCell *wPtr = Map_get_cell(x - 1, y);

			/* Connectivity mask: NESW bits */
			int adjN = !nPtr->empty;
			int adjE = !ePtr->empty;
			int adjS = !sPtr->empty;
			int adjW = !wPtr->empty;
			int conn = (adjN << 3) | (adjE << 2) | (adjS << 1) | adjW;

			ConnLookup cl = conn_table[conn];

			/* Spatial hash → pattern selection + rotation + mirror */
			unsigned int hash = (unsigned int)(x * 73856093u)
				^ (unsigned int)(y * 19349663u);
			int pattern_idx = (int)(hash % (unsigned int)class_count[cl.class_idx]);

			/* Extra rotation for rotationally symmetric classes */
			int extra_rot = 0;
			if (cl.class_idx == 0 || cl.class_idx == 5) {
				/* Island / surrounded: full rotational symmetry */
				extra_rot = (int)((hash >> 8) & 3u);
			} else if (cl.class_idx == 3) {
				/* Opposite edges: 180° symmetry */
				extra_rot = (int)((hash >> 8) & 1u) * 2;
			}
			int rotation = (cl.base_rot + extra_rot) & 3;

			/* Mirror only when E/W edges match (flip preserves connectivity) */
			int mirror = (adjE == adjW) ? (int)((hash >> 16) & 1u) : 0;

			/* Atlas tile index and UV origin */
			int tile = class_start[cl.class_idx] + pattern_idx;
			int tile_col = tile % ATLAS_COLS;
			int tile_row = tile / ATLAS_COLS;
			float u0 = (float)tile_col * TILE_U_SIZE;
			float v0 = (float)tile_row * TILE_V_SIZE;

			/* Trace color from cell outline */
			ColorFloat traceColor = Color_rgb_to_float(&me->outlineColor);
			float tr = traceColor.red, tg = traceColor.green;
			float tb = traceColor.blue, ta = traceColor.alpha;

			/* Chamfer (same logic as render_cell in map.c) */
			float chamf = MAP_CELL_SIZE * 0.17f;
			int chamfer_ne = nPtr->empty && ePtr->empty;
			int chamfer_sw = sPtr->empty && wPtr->empty;

			if (chamfer_ne || chamfer_sw) {
				/* Build chamfered polygon (same as render_cell) */
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

				/* Fan triangulation from vertex 0 */
				for (int i = 1; i < n - 1; i++) {
					push_tri(vx[0], vy[0], vx[i], vy[i],
						vx[i+1], vy[i+1], ax, ay,
						tr, tg, tb, ta,
						u0, v0, rotation, mirror);
				}
			} else {
				/* Simple quad: 2 triangles */
				push_tri(ax, ay, bx, ay, bx, by, ax, ay,
					tr, tg, tb, ta, u0, v0, rotation, mirror);
				push_tri(ax, ay, bx, by, ax, by, ax, ay,
					tr, tg, tb, ta, u0, v0, rotation, mirror);
			}
		}
	}

	if (s_vert_count == 0) return;

	/* Draw using text shader (same vertex layout) */
	Shaders *shaders = Graphics_get_shaders();
	Screen scr = Graphics_get_screen();
	Mat4 proj = Graphics_get_world_projection();
	Mat4 view_mat = View_get_transform(&scr);

	Shader_set_matrices(&shaders->text_shader, &proj, &view_mat);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_atlas_tex);
	glUniform1i(shaders->text_u_texture, 0);

	glBindVertexArray(s_vao);
	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
	glBufferData(GL_ARRAY_BUFFER,
		(GLsizeiptr)(s_vert_count * (int)sizeof(CircuitVertex)),
		s_verts, GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, s_vert_count);
	glBindVertexArray(0);
}
