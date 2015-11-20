#include "ship.h"

static Position position = {0.0, 0.0};
static double heading = 0.0;
static double velocity = 0.0;

void ship_update(const Input *input, const unsigned int ticks)
{

}

void ship_render(void)
{
	render_triangle(&position, heading, 255.0, 0.0, 0.0, 1.0);
}