#include "view.h"
#include <math.h>

const double MAX_ZOOM = 4.0;
const double DEFAULT_minZoom = 0.25;
const double ZOOM_IN_RATE = 1.05;
const double ZOOM_OUT_RATE = 0.95;

static double minZoom = 0.25;
static bool pixelSnapping = true;
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
	if (view.scale >= minZoom && input->mouseWheelDown)
		view.scale *= ZOOM_OUT_RATE;

	if (view.scale > MAX_ZOOM)
		view.scale = MAX_ZOOM;
	else if (view.scale < minZoom)
		view.scale = minZoom;
}

Mat4 View_get_transform(const Screen *screen)
{
	double x = (screen->width / 2.0) - (view.position.x * view.scale);
	double y = (screen->height / 2.0) - (view.position.y * view.scale);

	/* Snap to pixel grid to prevent subpixel jitter on thin geometry */
	if (pixelSnapping) {
		x = floor(x + 0.5);
		y = floor(y + 0.5);
	}

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

void View_set_scale(double scale)
{
	view.scale = scale;
}

void View_set_min_zoom(double zoom)
{
	minZoom = zoom;
}

void View_set_pixel_snapping(bool enabled)
{
	pixelSnapping = enabled;
}

bool View_get_pixel_snapping(void)
{
	return pixelSnapping;
}
