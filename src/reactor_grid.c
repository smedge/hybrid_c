#include "reactor_grid.h"
#include "render.h"
#include "view.h"
#include "graphics.h"
#include "screen.h"
#include "map_reflect.h"

#include <stdbool.h>
#include <math.h>
#include <OpenGL/gl3.h>

/* ----- Tuning ----- */

/* Grid layout */
static const float SQUARE_SIZE     = 800.0f;   /* side length of each square */
static const float SQUARE_GAP      = 160.0f;   /* gap between squares */

/* Colors */
static const float FILL_R          = 0.06f;    /* dark metallic fill */
static const float FILL_G          = 0.06f;
static const float FILL_B          = 0.08f;
static const float FILL_A          = 0.9f;

static const float BORDER_R        = 0.12f;    /* slightly lighter border */
static const float BORDER_G        = 0.12f;
static const float BORDER_B        = 0.16f;
static const float BORDER_A        = 0.9f;
static const float BORDER_WIDTH    = 3.0f;     /* border thickness in world units */

/* Parallax — lower = more depth separation from foreground */
static const float PARALLAX_RATE   = 0.4f;

/* ----- State ----- */

static float gridCenterX;
static float gridCenterY;
static float gridScale = 0.0f;
static bool initialized = false;

void ReactorGrid_initialize(float center_x, float center_y)
{
	gridCenterX = center_x;
	gridCenterY = center_y;
	gridScale = 0.0f;
	initialized = true;
}

void ReactorGrid_cleanup(void)
{
	initialized = false;
	gridScale = 0.0f;
}

void ReactorGrid_set_scale(float scale)
{
	if (scale < 0.0f) scale = 0.0f;
	if (scale > 1.0f) scale = 1.0f;
	gridScale = scale;
}

void ReactorGrid_render(const Mat4 *projection, const Mat4 *view)
{
	(void)projection;
	(void)view;

	if (!initialized || gridScale < 0.001f) return;

	float sz = SQUARE_SIZE * gridScale;
	float half_sz = sz * 0.5f;
	float stride = SQUARE_SIZE + SQUARE_GAP;

	View v = View_get_view();
	float camX = (float)v.position.x;
	float camY = (float)v.position.y;

	/*
	 * Grid cell [col,row] has its center at world position:
	 *   wx = gridCenterX + col * stride - camX * (1 - PARALLAX_RATE)
	 *   wy = gridCenterY + row * stride - camY * (1 - PARALLAX_RATE)
	 *
	 * The (1 - PARALLAX_RATE) term makes the grid scroll slower than
	 * the camera, creating depth. We precompute the parallax shift.
	 */
	float shiftX = -camX * (1.0f - PARALLAX_RATE);
	float shiftY = -camY * (1.0f - PARALLAX_RATE);

	/* Visible range in world coords (account for camera zoom) */
	Screen screen = Graphics_get_screen();
	float viewScale = (float)v.scale;
	if (viewScale < 0.01f) viewScale = 1.0f;
	float halfW = (screen.norm_w * 0.5f) / viewScale + stride;
	float halfH = (screen.norm_h * 0.5f) / viewScale + stride;

	/*
	 * A cell is visible when its world position is within [camX-halfW, camX+halfW].
	 * Solve for col:  camX - halfW < gridCenterX + col*stride - shiftX < camX + halfW
	 *                 col > (camX - halfW - gridCenterX + shiftX) / stride
	 */
	int colStart = (int)floorf((camX - halfW - gridCenterX + shiftX) / stride) - 2;
	int colEnd   = (int)ceilf((camX + halfW - gridCenterX + shiftX) / stride) + 2;
	int rowStart = (int)floorf((camY - halfH - gridCenterY + shiftY) / stride) - 2;
	int rowEnd   = (int)ceilf((camY + halfH - gridCenterY + shiftY) / stride) + 2;

	float alpha = gridScale * gridScale;
	float bw = BORDER_WIDTH;
	float ba = BORDER_A * alpha;
	float fa = FILL_A * alpha;

	for (int row = rowStart; row <= rowEnd; row++) {
		float cy = gridCenterY + row * stride - shiftY;

		for (int col = colStart; col <= colEnd; col++) {
			float cx = gridCenterX + col * stride - shiftX;

			/* Fill quad */
			Render_quad_absolute(
				cx - half_sz, cy + half_sz,
				cx + half_sz, cy - half_sz,
				FILL_R, FILL_G, FILL_B, fa);

			/* Border (4 edges) */
			/* Top */
			Render_quad_absolute(
				cx - half_sz, cy + half_sz,
				cx + half_sz, cy + half_sz - bw,
				BORDER_R, BORDER_G, BORDER_B, ba);
			/* Bottom */
			Render_quad_absolute(
				cx - half_sz, cy - half_sz + bw,
				cx + half_sz, cy - half_sz,
				BORDER_R, BORDER_G, BORDER_B, ba);
			/* Left */
			Render_quad_absolute(
				cx - half_sz, cy + half_sz - bw,
				cx - half_sz + bw, cy - half_sz + bw,
				BORDER_R, BORDER_G, BORDER_B, ba);
			/* Right */
			Render_quad_absolute(
				cx + half_sz - bw, cy + half_sz - bw,
				cx + half_sz, cy - half_sz + bw,
				BORDER_R, BORDER_G, BORDER_B, ba);
		}
	}
}

void ReactorGrid_render_stencil(const Mat4 *projection, const Mat4 *view)
{
	if (!initialized || gridScale < 0.001f) return;
	if (!MapReflect_get_enabled()) return;

	float sz = SQUARE_SIZE * gridScale;
	float half_sz = sz * 0.5f;
	float stride = SQUARE_SIZE + SQUARE_GAP;

	View v = View_get_view();
	float camX = (float)v.position.x;
	float camY = (float)v.position.y;
	float shiftX = -camX * (1.0f - PARALLAX_RATE);
	float shiftY = -camY * (1.0f - PARALLAX_RATE);

	Screen screen = Graphics_get_screen();
	float viewScale = (float)v.scale;
	if (viewScale < 0.01f) viewScale = 1.0f;
	float halfW = (screen.norm_w * 0.5f) / viewScale + stride;
	float halfH = (screen.norm_h * 0.5f) / viewScale + stride;

	int colStart = (int)floorf((camX - halfW - gridCenterX + shiftX) / stride) - 2;
	int colEnd   = (int)ceilf((camX + halfW - gridCenterX + shiftX) / stride) + 2;
	int rowStart = (int)floorf((camY - halfH - gridCenterY + shiftY) / stride) - 2;
	int rowEnd   = (int)ceilf((camY + halfH - gridCenterY + shiftY) / stride) + 2;

	/* Write stencil=2, no color output */
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS, 2, 0xFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glStencilMask(0xFF);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	for (int row = rowStart; row <= rowEnd; row++) {
		float cy = gridCenterY + row * stride - shiftY;
		for (int col = colStart; col <= colEnd; col++) {
			float cx = gridCenterX + col * stride - shiftX;
			Render_quad_absolute(
				cx - half_sz, cy + half_sz,
				cx + half_sz, cy - half_sz,
				1.0f, 1.0f, 1.0f, 1.0f); /* color doesn't matter, masked off */
		}
	}

	Render_flush(projection, view);

	/* Restore */
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glStencilMask(0xFF);
	glDisable(GL_STENCIL_TEST);
}
