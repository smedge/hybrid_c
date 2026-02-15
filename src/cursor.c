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

void cursor_render(void)
{
	if (!visible)
		return;

	/* Red center dot */
	Position pos = {x, y};
	ColorFloat red = {0.9f, 0.0f, 0.0f, 0.70f};
	Render_point(&pos, 2.0f, &red);

	/* White crosshair lines */
	float fx = (float)x;
	float fy = (float)y;
	Render_thick_line(fx, fy + 7.0f, fx, fy + 3.0f,
		1.0f, 0.9f, 0.9f, 0.9f, 0.50f);
	Render_thick_line(fx + 7.0f, fy, fx + 3.0f, fy,
		1.0f, 0.9f, 0.9f, 0.9f, 0.50f);
	Render_thick_line(fx, fy - 3.0f, fx, fy - 7.0f,
		1.0f, 0.9f, 0.9f, 0.9f, 0.50f);
	Render_thick_line(fx - 7.0f, fy, fx - 3.0f, fy,
		1.0f, 0.9f, 0.9f, 0.9f, 0.50f);
}
