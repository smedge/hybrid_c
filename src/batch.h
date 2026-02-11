#ifndef BATCH_H
#define BATCH_H

#include <OpenGL/gl3.h>
#include "shader.h"
#include "mat4.h"

#define BATCH_MAX_VERTICES 65536

typedef struct {
	float x, y;
	float r, g, b, a;
	float point_size;
} ColorVertex;

typedef struct {
	ColorVertex vertices[BATCH_MAX_VERTICES];
	int count;
	GLuint vao;
	GLuint vbo;
} PrimitiveBatch;

typedef struct {
	PrimitiveBatch triangles;
	PrimitiveBatch lines;
	PrimitiveBatch points;
} BatchRenderer;

void Batch_initialize(BatchRenderer *batch);
void Batch_cleanup(BatchRenderer *batch);

void Batch_push_triangle_vertices(BatchRenderer *batch,
	float x0, float y0, float x1, float y1, float x2, float y2,
	float r, float g, float b, float a);

void Batch_push_line_vertices(BatchRenderer *batch,
	float x0, float y0, float x1, float y1,
	float r, float g, float b, float a);

void Batch_push_point_vertex(BatchRenderer *batch,
	float x, float y, float size,
	float r, float g, float b, float a);

void Batch_flush(BatchRenderer *batch, const Shaders *shaders,
	const Mat4 *projection, const Mat4 *view);

void Batch_flush_keep(BatchRenderer *batch, const Shaders *shaders,
	const Mat4 *projection, const Mat4 *view);

void Batch_redraw(BatchRenderer *batch, const Shaders *shaders,
	const Mat4 *projection, const Mat4 *view);

void Batch_clear(BatchRenderer *batch);

#endif
