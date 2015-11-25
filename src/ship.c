#include "ship.h"

#include "view.h"

static const double NORMAL_VELOCITY = 50.0;
static const double FAST_VELOCITY = 100.0;
static const double SLOW_VELOCITY = 10.0;

static Position position = {0.0, 0.0};
static double heading = 0.0;

static double get_heading(bool n, bool s, bool e, bool w);

void ship_initialize() {
	position.x = 0.0;
	position.y = 0.0;
	heading = 0;
}

void ship_update(const Input *input, const unsigned int ticks)
{
	double velocity;
	
	if (input->keyLShift)
		velocity = FAST_VELOCITY;
	else if (input->keyLControl)
		velocity = SLOW_VELOCITY;
	else
		velocity = NORMAL_VELOCITY;

	if (input->keyW)
		position.y += velocity;
	if (input->keyS)
		position.y -= velocity;
	if (input->keyD)
		position.x += velocity;
	if (input->keyA)
		position.x -= velocity;

	if (input->keyW || input->keyA || input->keyS || input->keyD)
		heading = get_heading(input->keyW, input->keyS, input->keyD, input->keyA);
	view_set_position(position);
}

void ship_render(void)
{
	Render_triangle(&position, heading, 255.0, 0.0, 0.0, 1.0);
}

static double get_heading(bool n, bool s, bool e, bool w)
{
	if (n) {
		if (e)
			return 45.0;
		else if (w)
			return 315.0;
		else
			return 0.0;
	}
	if (s) {
		if (e)
			return 135.0;
		else if (w)
			return 225.0;
		else
			return 180.0;
	}
	if (e)
		return 90.0;
	if (w)
		return 270.0;

	return 0.0;
}