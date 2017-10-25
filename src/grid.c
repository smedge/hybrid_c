#include "grid.h"

#include <math.h>
#include <SDL2/SDL_opengl.h>

const double GRID_SIZE = 100.0;
const double BIG_GRID_SIZE = 16.0;
const double GRID_MIN_LINE_SIZE = 1.0;
const double GRID_MIN_BIG_LINE_SIZE = 3.0;

static RenderableComponent renderable = {Grid_render};

void Grid_initialize()
{
	Entity entity = Entity_initialize_entity();
	entity.renderable = &renderable;
	Entity_add_entity(entity);
}

void Grid_cleanup()
{
	
}

void Grid_render()
{
	Render_grid_lines(GRID_SIZE, BIG_GRID_SIZE, GRID_MIN_LINE_SIZE, GRID_MIN_BIG_LINE_SIZE);
}
