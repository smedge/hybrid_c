#include "grid.h"

#include <math.h>

const double GRID_SIZE = 100.0;
const double GRID_MIN_LINE_SIZE = 1.0;

void grid_render(const Screen *screen, const View *view)
{
	const double HALF_SCREEN_WIDTH = (screen->width / 2) / view->scale;
	const double HALF_SCREEN_HEIGHT = (screen->height / 2) / view->scale;

	glPushMatrix();

	// draw centerlines
	glLineWidth(3);
	glBegin(GL_LINES);
		glColor4f(0.0, 1.0, 0.0, 1.0);
		glVertex2f(view->position.x + (-HALF_SCREEN_WIDTH), 0.0);
		glVertex2f(view->position.x + HALF_SCREEN_WIDTH, 0.0);
		glVertex2f(0.0, view->position.y + HALF_SCREEN_HEIGHT);
		glVertex2f(0.0, view->position.y + (-HALF_SCREEN_HEIGHT));
	glEnd();

	// draw others
	glLineWidth(1);
	glBegin(GL_LINES);
		// draw verts along x positive
		for (int i = GRID_SIZE;
				i<HALF_SCREEN_WIDTH + view->position.x;
				i+=GRID_SIZE) {
			if (fmod(i, GRID_SIZE * 20.0) == 0.0)
				glColor4f(0.0, 1.0, 0.0, 0.8);
			else
				glColor4f(0.0, 1.0, 0.0, 0.35);
			glVertex2f(i, view->position.y + HALF_SCREEN_HEIGHT);
			glVertex2f(i, view->position.y + (-HALF_SCREEN_HEIGHT));
		}

		// draw verts along x negative
		for (int i = GRID_SIZE;
				i < HALF_SCREEN_WIDTH-view->position.x;
				i+=GRID_SIZE) {
			if (fmod(i, GRID_SIZE * 20.0) == 0.0)
				glColor4f(0.0, 1.0, 0.0, 0.8);
			else
				glColor4f(0.0, 1.0, 0.0, 0.35);
			glVertex2f(-i, view->position.y + HALF_SCREEN_HEIGHT);
			glVertex2f(-i, view->position.y + (-HALF_SCREEN_HEIGHT));
		}

		// draw horz along y positive
		for (int i = GRID_SIZE;
				i<HALF_SCREEN_HEIGHT + view->position.y;
				i+=GRID_SIZE) {
			if (fmod(i, GRID_SIZE * 20.0) == 0.0)
				glColor4f(0.0, 1.0, 0.0, 0.8);
			else
				glColor4f(0.0, 1.0, 0.0, 0.35);
			glVertex2f(view->position.x + (-HALF_SCREEN_WIDTH), i);
			glVertex2f(view->position.x + HALF_SCREEN_WIDTH, i);
		}

		// draw horz along y negative
		for (int i = GRID_SIZE;
				i<HALF_SCREEN_HEIGHT - view->position.y;
				i+=GRID_SIZE) {
			if (fmod(i, GRID_SIZE * 20.0) == 0.0)
				glColor4f(0.0, 1.0, 0.0, 0.8);
			else
				glColor4f(0.0, 1.0, 0.0, 0.35);
			glVertex2f(view->position.x + (-HALF_SCREEN_WIDTH), -i);
			glVertex2f(view->position.x + HALF_SCREEN_WIDTH, -i);
		}

	glEnd();
	glPopMatrix();
}