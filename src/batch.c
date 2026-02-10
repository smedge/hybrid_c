#include "batch.h"
#include <string.h>

static void init_batch(PrimitiveBatch *b)
{
	b->count = 0;

	glGenVertexArrays(1, &b->vao);
	glGenBuffers(1, &b->vbo);
	glBindVertexArray(b->vao);
	glBindBuffer(GL_ARRAY_BUFFER, b->vbo);

	/* position: location 0 */
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
		sizeof(ColorVertex), (void *)0);

	/* color: location 1 */
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
		sizeof(ColorVertex), (void *)(2 * sizeof(float)));

	/* point_size: location 2 */
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE,
		sizeof(ColorVertex), (void *)(6 * sizeof(float)));

	glBindVertexArray(0);
}

static void cleanup_batch(PrimitiveBatch *b)
{
	glDeleteBuffers(1, &b->vbo);
	glDeleteVertexArrays(1, &b->vao);
}

static void flush_batch(PrimitiveBatch *b, GLenum mode,
	const ShaderProgram *sp, const Mat4 *projection, const Mat4 *view)
{
	if (b->count == 0)
		return;

	Shader_set_matrices(sp, projection, view);

	glBindVertexArray(b->vao);
	glBindBuffer(GL_ARRAY_BUFFER, b->vbo);
	glBufferData(GL_ARRAY_BUFFER,
		(GLsizeiptr)(b->count * sizeof(ColorVertex)),
		b->vertices, GL_DYNAMIC_DRAW);
	glDrawArrays(mode, 0, b->count);
	glBindVertexArray(0);

	b->count = 0;
}

void Batch_initialize(BatchRenderer *batch)
{
	memset(batch, 0, sizeof(*batch));
	init_batch(&batch->triangles);
	init_batch(&batch->lines);
	init_batch(&batch->points);
}

void Batch_cleanup(BatchRenderer *batch)
{
	cleanup_batch(&batch->triangles);
	cleanup_batch(&batch->lines);
	cleanup_batch(&batch->points);
}

void Batch_push_triangle_vertices(BatchRenderer *batch,
	float x0, float y0, float x1, float y1, float x2, float y2,
	float r, float g, float b, float a)
{
	PrimitiveBatch *pb = &batch->triangles;
	if (pb->count + 3 > BATCH_MAX_VERTICES)
		return;

	ColorVertex *v = &pb->vertices[pb->count];
	v[0] = (ColorVertex){x0, y0, r, g, b, a, 1.0f};
	v[1] = (ColorVertex){x1, y1, r, g, b, a, 1.0f};
	v[2] = (ColorVertex){x2, y2, r, g, b, a, 1.0f};
	pb->count += 3;
}

void Batch_push_line_vertices(BatchRenderer *batch,
	float x0, float y0, float x1, float y1,
	float r, float g, float b, float a)
{
	PrimitiveBatch *pb = &batch->lines;
	if (pb->count + 2 > BATCH_MAX_VERTICES)
		return;

	ColorVertex *v = &pb->vertices[pb->count];
	v[0] = (ColorVertex){x0, y0, r, g, b, a, 1.0f};
	v[1] = (ColorVertex){x1, y1, r, g, b, a, 1.0f};
	pb->count += 2;
}

void Batch_push_point_vertex(BatchRenderer *batch,
	float x, float y, float size,
	float r, float g, float b, float a)
{
	PrimitiveBatch *pb = &batch->points;
	if (pb->count + 1 > BATCH_MAX_VERTICES)
		return;

	pb->vertices[pb->count] = (ColorVertex){x, y, r, g, b, a, size};
	pb->count++;
}

void Batch_flush(BatchRenderer *batch, const Shaders *shaders,
	const Mat4 *projection, const Mat4 *view)
{
	flush_batch(&batch->lines, GL_LINES,
		&shaders->color_shader, projection, view);
	flush_batch(&batch->triangles, GL_TRIANGLES,
		&shaders->color_shader, projection, view);
	flush_batch(&batch->points, GL_POINTS,
		&shaders->color_shader, projection, view);
}

void Batch_clear(BatchRenderer *batch)
{
	batch->triangles.count = 0;
	batch->lines.count = 0;
	batch->points.count = 0;
}
