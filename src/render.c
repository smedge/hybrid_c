#include "render.h"

#include <SDL2/SDL_opengl.h>

static int get_nearest_start_point(int x, const double GRID_SIZE);

void Render_point(const Position *position, const float size, 
	const ColorFloat *color)
{
	glPushMatrix();
	glPointSize(size);
	glColor4f(color->red, color->green, color->blue, color->alpha);
	glTranslatef(position->x, position->y, 0.0);
	glBegin(GL_POINTS);
		glVertex2f(0.0, 0.0);
	glEnd();
	glPopMatrix();
}

void Render_line()
{
	
}

void Render_triangle(const Position *position, const double heading, 
	const ColorFloat *color) 
{
	glPushMatrix();
	glTranslatef(position->x, position->y, 0.0);
	glRotatef(heading*-1, 0, 0, 1);
	glBegin(GL_TRIANGLES);
	    glColor4f(color->red, color->green, color->blue, color->alpha);
		glVertex2f(0.0, 20.0);
		glVertex2f(10.0, -10.0);
		glVertex2f(-10.0, -10.0);
	glEnd();
	glPopMatrix();
}

void Render_quad(const Position *position, const double rotation, 
	const Rectangle rectangle, const ColorFloat *color)
{
	glPushMatrix();
	glColor4f(color->red, color->green, color->blue, color->alpha);
	glTranslatef(position->x, position->y, 0.0);
	glRotatef(rotation*-1, 0, 0, 1);
	glBegin(GL_QUADS);
		glVertex2f(rectangle.aX, rectangle.aY);
		glVertex2f(rectangle.aX, rectangle.bY);
		glVertex2f(rectangle.bX, rectangle.bY);
		glVertex2f(rectangle.bX, rectangle.aY);
	glEnd();
	glPopMatrix();
}

void Render_convex_poly()
{
	
}

void Render_bounding_box(const Position *position, 
		const Rectangle *boundingBox) 
{
	glPushMatrix();
	glTranslatef(position->x, position->y, 0.0);

	glLineWidth(1.0);
	glColor4f(1.0, 1.0, 0.0, 0.60);
	glBegin(GL_LINE_LOOP);
	glVertex2f(boundingBox->aX, boundingBox->aY);
	glVertex2f(boundingBox->aX, boundingBox->bY);
	glVertex2f(boundingBox->bX, boundingBox->bY);
	glVertex2f(boundingBox->bX, boundingBox->aY);
	glEnd();

	glPopMatrix();
}

void Render_grid_lines(const double gridSize, const double bigGridSize, 
		const double minLineSize, const double minBigLineSize)
{
	const View view = View_get_view();
	const Screen screen = Graphics_get_screen();

	const double HALF_SCREEN_WIDTH = (screen.width / 2) / view.scale;
	const double HALF_SCREEN_HEIGHT = (screen.height / 2) / view.scale;

	// calculate line widths
	double bigLineWidth = minBigLineSize  * view.scale;
	if (bigLineWidth < minBigLineSize )
		bigLineWidth = minBigLineSize;
	
	double lineWidth = minLineSize * view.scale;
	if (lineWidth < minLineSize)
		lineWidth = minLineSize;


	glPushMatrix();
	
	// draw lines
	glLineWidth(lineWidth);
	glBegin(GL_LINES);

		// draw vert lines along x
		for (int i = get_nearest_start_point(view.position.x - HALF_SCREEN_WIDTH, gridSize); 
				i<HALF_SCREEN_WIDTH + view.position.x;
				i+=gridSize) {
			if (fmod(i, gridSize * bigGridSize) == 0.0)
				glColor4f(0.0, 1.0, 0.0, 0.4);
			else
				glColor4f(0.0, 1.0, 0.0, 0.2);
				
			glVertex2f(i, view.position.y + HALF_SCREEN_HEIGHT);
			glVertex2f(i, view.position.y + (-HALF_SCREEN_HEIGHT));
		}
		
		// draw horz lines along y
		for (int i = get_nearest_start_point(view.position.y - HALF_SCREEN_HEIGHT, gridSize); 
				i<HALF_SCREEN_HEIGHT + view.position.y;
				i+=gridSize) {
			if (fmod(i, gridSize * bigGridSize) == 0.0)
				glColor4f(0.0, 1.0, 0.0, 0.4);
			else
				glColor4f(0.0, 1.0, 0.0, 0.2);
				
			glVertex2f(view.position.x + HALF_SCREEN_WIDTH, i);
			glVertex2f(view.position.x + (-HALF_SCREEN_WIDTH), i);
		}

	glEnd();
	glPopMatrix();
}

static int get_nearest_start_point(int x, const double GRID_SIZE)
{
	int a, b;
	int gridSize = (int) GRID_SIZE;

	if (x > 0) {
		a = x % gridSize;
		b = x - a;
		return b; 
	}
	else {
		x = -x;
		a = x % gridSize;
		b = gridSize - a;
		return -x - b;	
	}
}
