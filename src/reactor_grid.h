#ifndef REACTOR_GRID_H
#define REACTOR_GRID_H

#include "mat4.h"

/* Initialize the reactor grid centered at the boss arena */
void ReactorGrid_initialize(float center_x, float center_y);
void ReactorGrid_cleanup(void);

/* Set the scale factor (0.0 = invisible, 1.0 = full size) for grow/shrink animation */
void ReactorGrid_set_scale(float scale);

/* Render the grid as a midground layer (call between bg_bloom composite and grid render) */
void ReactorGrid_render(const Mat4 *projection, const Mat4 *view);

/* Write stencil=2 for grid squares (call before MapReflect_render for cloud reflections) */
void ReactorGrid_render_stencil(const Mat4 *projection, const Mat4 *view);

#endif
