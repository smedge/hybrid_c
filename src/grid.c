#include "grid.h"

const double GRID_SIZE = 100.0;
const double BIG_GRID_SIZE = 16.0;
const double GRID_MIN_LINE_SIZE = 1.0;
const double GRID_MIN_BIG_LINE_SIZE = 3.0;

void Grid_initialize(void)
{
}

void Grid_cleanup(void)
{

}

void Grid_render(const void *state, const PlaceableComponent *placeable)
{
	(void)state;
	(void)placeable;

	Render_grid_lines(GRID_SIZE, BIG_GRID_SIZE, GRID_MIN_LINE_SIZE, GRID_MIN_BIG_LINE_SIZE);
}
