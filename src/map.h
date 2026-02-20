#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include "color.h"
#include "collision.h"
#include "entity.h"
#include "mat4.h"

#define MAP_SIZE 1024
#define HALF_MAP_SIZE (MAP_SIZE / 2)
#define MAP_CELL_SIZE 100.0
#define MAP_MIN_LINE_SIZE 1.0

typedef struct {
	bool empty;
	bool circuitPattern;
	ColorRGB primaryColor;
	ColorRGB outlineColor;
} MapCell;

void Map_initialize(void);
void Map_clear(void);
void Map_set_cell(int grid_x, int grid_y, const MapCell *cell);
void Map_clear_cell(int grid_x, int grid_y);
const MapCell *Map_get_cell(int grid_x, int grid_y);
void Map_set_boundary_cell(const MapCell *cell);
void Map_clear_boundary_cell(void);
Collision Map_collide(void *state, const PlaceableComponent *placeable, const Rectangle boundingBox);
void Map_render(const void *state, const PlaceableComponent *placeable);
void Map_render_bloom_source(void);
void Map_render_minimap(float center_x, float center_y,
	float screen_x, float screen_y, float size, float range);
bool Map_line_test_hit(double x0, double y0, double x1, double y1,
					   double *hit_x, double *hit_y);
void Map_render_stencil_mask(void);
void Map_render_stencil_mask_all(const Mat4 *proj, const Mat4 *view_mat);
void Map_set_circuit_traces(bool enabled);
bool Map_get_circuit_traces(void);
void Map_render_circuit_pattern_for_atlas(int cellX, int cellY,
	float ax, float ay, int adjN, int adjE, int adjS, int adjW);

#endif
