#include "render.h"

#include <math.h>
#include <OpenGL/gl3.h>

static int get_nearest_grid_start_point(int x, const double GRID_SIZE);

void Render_point(const Position *position, const float size,
	const ColorFloat *color)
{
	BatchRenderer *batch = Graphics_get_batch();
	Batch_push_point_vertex(batch,
		(float)position->x, (float)position->y, size,
		color->red, color->green, color->blue, color->alpha);
}

void Render_line()
{
}

void Render_triangle(const Position *position, const double heading,
	const ColorFloat *color)
{
	/* Build model transform: translate then rotate */
	Mat4 t = Mat4_translate((float)position->x, (float)position->y, 0.0f);
	Mat4 r = Mat4_rotate_z((float)(heading * -1.0));
	Mat4 m = Mat4_multiply(&t, &r);

	/* Triangle vertices in local space */
	float lx[3] = { 0.0f, 10.0f, -10.0f };
	float ly[3] = { 20.0f, -10.0f, -10.0f };

	float wx[3], wy[3];
	for (int i = 0; i < 3; i++)
		Mat4_transform_point(&m, lx[i], ly[i], &wx[i], &wy[i]);

	BatchRenderer *batch = Graphics_get_batch();
	Batch_push_triangle_vertices(batch,
		wx[0], wy[0], wx[1], wy[1], wx[2], wy[2],
		color->red, color->green, color->blue, color->alpha);
}

void Render_quad(const Position *position, const double rotation,
	const Rectangle rectangle, const ColorFloat *color)
{
	Mat4 t = Mat4_translate((float)position->x, (float)position->y, 0.0f);
	Mat4 r = Mat4_rotate_z((float)(rotation * -1.0));
	Mat4 m = Mat4_multiply(&t, &r);

	/* Quad corners in local space */
	float lx[4] = { (float)rectangle.aX, (float)rectangle.aX,
	                 (float)rectangle.bX, (float)rectangle.bX };
	float ly[4] = { (float)rectangle.aY, (float)rectangle.bY,
	                 (float)rectangle.bY, (float)rectangle.aY };

	float wx[4], wy[4];
	for (int i = 0; i < 4; i++)
		Mat4_transform_point(&m, lx[i], ly[i], &wx[i], &wy[i]);

	/* Two triangles: 0-1-2 and 0-2-3 */
	BatchRenderer *batch = Graphics_get_batch();
	Batch_push_triangle_vertices(batch,
		wx[0], wy[0], wx[1], wy[1], wx[2], wy[2],
		color->red, color->green, color->blue, color->alpha);
	Batch_push_triangle_vertices(batch,
		wx[0], wy[0], wx[2], wy[2], wx[3], wy[3],
		color->red, color->green, color->blue, color->alpha);
}

void Render_convex_poly()
{
}

void Render_bounding_box(const Position *position,
	const Rectangle *boundingBox)
{
	float px = (float)position->x;
	float py = (float)position->y;

	float ax = px + (float)boundingBox->aX;
	float ay = py + (float)boundingBox->aY;
	float bx = px + (float)boundingBox->bX;
	float by = py + (float)boundingBox->bY;

	/* GL_LINE_LOOP → 4 GL_LINES segments */
	Render_line_segment(ax, ay, ax, by, 1.0f, 1.0f, 0.0f, 0.60f);
	Render_line_segment(ax, by, bx, by, 1.0f, 1.0f, 0.0f, 0.60f);
	Render_line_segment(bx, by, bx, ay, 1.0f, 1.0f, 0.0f, 0.60f);
	Render_line_segment(bx, ay, ax, ay, 1.0f, 1.0f, 0.0f, 0.60f);
}

void Render_grid_lines(const double gridSize, const double bigGridSize,
	const double minLineSize, const double minBigLineSize)
{
	const View view = View_get_view();
	const Screen screen = Graphics_get_screen();

	const double HALF_SCREEN_WIDTH = (screen.width / 2) / view.scale;
	const double HALF_SCREEN_HEIGHT = (screen.height / 2) / view.scale;

	(void)minBigLineSize;
	(void)minLineSize;

	BatchRenderer *batch = Graphics_get_batch();

	/* Vertical lines along x */
	for (int i = get_nearest_grid_start_point(
			(int)(view.position.x - HALF_SCREEN_WIDTH), gridSize);
			i < HALF_SCREEN_WIDTH + view.position.x;
			i += (int)gridSize) {
		float alpha;
		if (fmod(i, gridSize * bigGridSize) == 0.0)
			alpha = 0.3f;
		else
			alpha = 0.15f;

		Batch_push_line_vertices(batch,
			(float)i, (float)(view.position.y + HALF_SCREEN_HEIGHT),
			(float)i, (float)(view.position.y + (-HALF_SCREEN_HEIGHT)),
			0.0f, 1.0f, 0.0f, alpha);
	}

	/* Horizontal lines along y */
	for (int i = get_nearest_grid_start_point(
			(int)(view.position.y - HALF_SCREEN_HEIGHT), gridSize);
			i < HALF_SCREEN_HEIGHT + view.position.y;
			i += (int)gridSize) {
		float alpha;
		if (fmod(i, gridSize * bigGridSize) == 0.0)
			alpha = 0.3f;
		else
			alpha = 0.15f;

		Batch_push_line_vertices(batch,
			(float)(view.position.x + HALF_SCREEN_WIDTH), (float)i,
			(float)(view.position.x + (-HALF_SCREEN_WIDTH)), (float)i,
			0.0f, 1.0f, 0.0f, alpha);
	}
}

void Render_line_segment(float x0, float y0, float x1, float y1,
	float r, float g, float b, float a)
{
	BatchRenderer *batch = Graphics_get_batch();
	Batch_push_line_vertices(batch, x0, y0, x1, y1, r, g, b, a);
}

void Render_quad_absolute(float ax, float ay, float bx, float by,
	float r, float g, float b, float a)
{
	BatchRenderer *batch = Graphics_get_batch();
	/* Two triangles: (ax,ay)-(ax,by)-(bx,by) and (ax,ay)-(bx,by)-(bx,ay) */
	Batch_push_triangle_vertices(batch,
		ax, ay, ax, by, bx, by, r, g, b, a);
	Batch_push_triangle_vertices(batch,
		ax, ay, bx, by, bx, ay, r, g, b, a);
}

void Render_thick_line(float x0, float y0, float x1, float y1,
	float thickness, float r, float g, float b, float a)
{
	float dx = x1 - x0;
	float dy = y1 - y0;
	float len = sqrtf(dx * dx + dy * dy);
	if (len < 0.001f) return;

	float nx = (-dy / len) * thickness * 0.5f;
	float ny = (dx / len) * thickness * 0.5f;

	BatchRenderer *batch = Graphics_get_batch();
	Batch_push_triangle_vertices(batch,
		x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 - nx, y1 - ny,
		r, g, b, a);
	Batch_push_triangle_vertices(batch,
		x0 + nx, y0 + ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny,
		r, g, b, a);
}

void Render_filled_circle(float cx, float cy, float radius, int segments,
	float r, float g, float b, float a)
{
	BatchRenderer *batch = Graphics_get_batch();
	float step = 2.0f * (float)M_PI / (float)segments;

	for (int i = 0; i < segments; i++) {
		float a0 = (float)i * step;
		float a1 = (float)(i + 1) * step;
		Batch_push_triangle_vertices(batch,
			cx, cy,
			cx + radius * cosf(a0), cy + radius * sinf(a0),
			cx + radius * cosf(a1), cy + radius * sinf(a1),
			r, g, b, a);
	}
}

void Render_flush(const Mat4 *projection, const Mat4 *view)
{
	BatchRenderer *batch = Graphics_get_batch();
	Shaders *shaders = Graphics_get_shaders();
	Batch_flush(batch, shaders, projection, view);
}

void Render_flush_additive(const Mat4 *projection, const Mat4 *view)
{
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	BatchRenderer *batch = Graphics_get_batch();
	Shaders *shaders = Graphics_get_shaders();
	Batch_flush(batch, shaders, projection, view);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static int get_nearest_grid_start_point(int x, const double GRID_SIZE)
{
	int a, b;
	int gridSize = (int)GRID_SIZE;

	if (x > 0) {
		a = x % gridSize;
		b = x - a;
		return b;
	}
	else {
		x = -x;
		a = x % gridSize;
		b = gridSize - a;
		return -x - b;
	}
}
