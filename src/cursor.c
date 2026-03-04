#include "cursor.h"
#include "render.h"

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

	/* Invert blend: output = 1 - destination color.
	   Cursor is always visible regardless of background. */
	Render_set_blend_invert();

	float fx = (float)x;
	float fy = (float)y;

	/* Center dot */
	Render_quad_absolute(fx - 1.0f, fy - 1.0f, fx + 1.0f, fy + 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f);

	/* Crosshair lines */
	Render_thick_line(fx, fy + 7.0f, fx, fy + 3.0f,
		1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	Render_thick_line(fx + 7.0f, fy, fx + 3.0f, fy,
		1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	Render_thick_line(fx, fy - 3.0f, fx, fy - 7.0f,
		1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	Render_thick_line(fx - 7.0f, fy, fx - 3.0f, fy,
		1.0f, 1.0f, 1.0f, 1.0f, 1.0f);

	Render_flush(projection, view);

	/* Restore normal alpha blending */
	Render_set_blend_normal();
}
