#include "view.h"

const double MAX_ZOOM = 4.0;
const double MIN_ZOOM = 0.01;
const double ZOOM_IN_RATE = 1.05;
const double ZOOM_OUT_RATE = 0.95;

static View view;

void View_initialize()
{
	view.position.x = 0.0;
	view.position.y = 0.0;
	view.scale = 0.5;
}

void View_update(const Input *input, const unsigned int ticks)
{
	if (view.scale <= MAX_ZOOM && input->mouseWheelUp)
		view.scale *= ZOOM_IN_RATE;
	if (view.scale >= MIN_ZOOM && input->mouseWheelDown)
		view.scale *= ZOOM_OUT_RATE;

	if (view.scale > MAX_ZOOM)
		view.scale = MAX_ZOOM;
	else if (view.scale < MIN_ZOOM)
		view.scale = MIN_ZOOM;
}

Mat4 View_get_transform(const Screen *screen)
{
	double x = (screen->width / 2.0) - (view.position.x * view.scale);
	double y = (screen->height / 2.0) - (view.position.y * view.scale);
	Mat4 t = Mat4_translate((float)x, (float)y, 0.0f);
	Mat4 s = Mat4_scale((float)view.scale, (float)view.scale, 1.0f);
	return Mat4_multiply(&t, &s);
}

Position View_get_world_position(const Screen *screen, const Position screenPosition)
{
	/* SDL mouse coords: y=0 at top. World projection: y=0 at bottom. Flip Y. */
	float flipped_y = (float)screen->height - (float)screenPosition.y;

	Mat4 view_mat = View_get_transform(screen);
	Mat4 inv = Mat4_inverse(&view_mat);

	float wx, wy;
	Mat4_transform_point(&inv, (float)screenPosition.x, flipped_y, &wx, &wy);

	Position worldPosition;
	worldPosition.x = wx;
	worldPosition.y = wy;
	return worldPosition;
}

const View View_get_view(void)
{
	return view;
}

void View_set_position(const Position position)
{
	view.position = position;
}
