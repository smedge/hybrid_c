#include "cursor.h"
#include "render.h"
#include "graphics.h"
#include <math.h>

static int x = 0;
static int y = 0;
static bool visible = false;

void cursor_update(const Input *input)
{
	visible = input->showMouse;
	x = input->mouseX;
	y = input->mouseY;
}

void cursor_render(const Mat4 *projection, const Mat4 *view)
{
	if (!visible)
		return;

	float s = Graphics_get_ui_scale();

	/* Invert blend: output = 1 - destination color.
	   Cursor is always visible regardless of background. */
	Render_set_blend_invert();

	float fx = (float)x;
	float fy = (float)y;

	/* Line width: scale but clamp to whole pixels, min 1px */
	float lw = floorf(1.0f * s + 0.5f);
	if (lw < 1.0f) lw = 1.0f;

	/* Center dot */
	float dot = floorf(1.0f * s + 0.5f);
	if (dot < 1.0f) dot = 1.0f;
	Render_quad_absolute(fx - dot, fy - dot, fx + dot, fy + dot,
		1.0f, 1.0f, 1.0f, 1.0f);

	/* Crosshair lines — scaled offsets, integer line width */
	float inner = 3.0f * s;
	float outer = 7.0f * s;
	Render_thick_line(fx, fy + outer, fx, fy + inner,
		lw, 1.0f, 1.0f, 1.0f, 1.0f);
	Render_thick_line(fx + outer, fy, fx + inner, fy,
		lw, 1.0f, 1.0f, 1.0f, 1.0f);
	Render_thick_line(fx, fy - inner, fx, fy - outer,
		lw, 1.0f, 1.0f, 1.0f, 1.0f);
	Render_thick_line(fx - outer, fy, fx - inner, fy,
		lw, 1.0f, 1.0f, 1.0f, 1.0f);

	Render_flush(projection, view);

	/* Restore normal alpha blending */
	Render_set_blend_normal();
}
