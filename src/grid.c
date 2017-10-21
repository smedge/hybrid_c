#include "grid.h"

#include <math.h>
#include <SDL2/SDL_opengl.h>

const double GRID_SIZE = 100.0;
const double BIG_GRID_SIZE = 16.0;
const double GRID_MIN_LINE_SIZE = 1.0;
const double GRID_MIN_BIG_LINE_SIZE = 3.0;

static RenderableComponent renderable = {Grid_render};

static int get_nearest_start_point(int x);

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
	const View view = View_get_view();
	const Screen screen = Graphics_get_screen();

	const double HALF_SCREEN_WIDTH = (screen.width / 2) / view.scale;
	const double HALF_SCREEN_HEIGHT = (screen.height / 2) / view.scale;

	glPushMatrix();

	// calculate line widths
	double bigLineWidth = GRID_MIN_BIG_LINE_SIZE  * view.scale;
	if (bigLineWidth < GRID_MIN_BIG_LINE_SIZE )
		bigLineWidth = GRID_MIN_BIG_LINE_SIZE;
	
	double lineWidth = GRID_MIN_LINE_SIZE * view.scale;
	if (lineWidth < GRID_MIN_LINE_SIZE)
		lineWidth = GRID_MIN_LINE_SIZE;

	// draw lines
	glLineWidth(lineWidth);
	glBegin(GL_LINES);
		
		// draw vert lines along x
		for (int i = get_nearest_start_point(view.position.x - HALF_SCREEN_WIDTH); 
				i<HALF_SCREEN_WIDTH + view.position.x;
				i+=GRID_SIZE) {
			if (fmod(i, GRID_SIZE * BIG_GRID_SIZE) == 0.0)
				glColor4f(0.0, 1.0, 0.0, 0.4);
			else
				glColor4f(0.0, 1.0, 0.0, 0.2);
				
			glVertex2f(i, view.position.y + HALF_SCREEN_HEIGHT);
			glVertex2f(i, view.position.y + (-HALF_SCREEN_HEIGHT));
		}
		
		// draw horz lines along y
		for (int i = get_nearest_start_point(view.position.y - HALF_SCREEN_HEIGHT); 
				i<HALF_SCREEN_HEIGHT + view.position.y;
				i+=GRID_SIZE) {
			if (fmod(i, GRID_SIZE * BIG_GRID_SIZE) == 0.0)
				glColor4f(0.0, 1.0, 0.0, 0.4);
			else
				glColor4f(0.0, 1.0, 0.0, 0.2);
				
			glVertex2f(view.position.x + HALF_SCREEN_WIDTH, i);
			glVertex2f(view.position.x + (-HALF_SCREEN_WIDTH), i);
		}

	glEnd();
	glPopMatrix();
}

static int get_nearest_start_point(int x)
{
	int a, b;

	if (x > 0) {
		a = x % 100;
		b = x - a;
		return b; 
	}
	else {
		x = -x;
		a = x % 100;
		b = 100 - a;
		return -x - b;	
	}
}
