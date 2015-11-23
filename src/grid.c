#include "grid.h"
#include <math.h>

#define GRID_SIZE 100.0
#define GRID_MIN_LINE_SIZE 1.0

void grid_render(const Screen *screen, const View *view)
{
	// glPushMatrix();
	// glLineWidth(2.0 * view->scale);
	// glColor4f(0.0, 1.0, 0.0, 0.50);
	// glBegin(GL_LINES);
	// 	glVertex2f(0.0, screen->height/2.0);
	// 	glVertex2f(screen->width, screen->height/2.0);
	// 	glVertex2f(screen->width/2.0, 0.0);
	// 	glVertex2f(screen->width/2.0, screen->height);
	// glEnd();
	// glPopMatrix();


	double halfScreenWidth = (screen->width / 2) / view->scale;
	double halfScreenHeight = (screen->height / 2) / view->scale;

	glPushMatrix();

	// draw centerlines
	glLineWidth(3);
	glBegin(GL_LINES);
		glColor4f(0.0, 1.0, 0.0, 1.0);


		glVertex2f(view->position.x + (-halfScreenWidth), 0.0);
		glVertex2f(view->position.x + halfScreenWidth, 0.0);
		glVertex2f(0.0, view->position.y + halfScreenHeight);
		glVertex2f(0.0, view->position.y + (-halfScreenHeight));
	glEnd();

	// draw others
	glLineWidth(1);
	glBegin(GL_LINES);
		// draw verts along x positive
		for (int i = GRID_SIZE;
				i<halfScreenWidth + view->position.x;
				i+=GRID_SIZE) {
			if (fmod(i, GRID_SIZE * 20.0) == 0.0)
				glColor4f(0.0, 1.0, 0.0, 0.8);
			else
				glColor4f(0.0, 1.0, 0.0, 0.35);
			glVertex2f(i, view->position.y + halfScreenHeight);
			glVertex2f(i, view->position.y + (-halfScreenHeight));
		}

		// draw verts along x negative
		for (int i = GRID_SIZE;
				i < halfScreenWidth-view->position.x;
				i+=GRID_SIZE) {
			if (fmod(i, GRID_SIZE * 20.0) == 0.0)
				glColor4f(0.0, 1.0, 0.0, 0.8);
			else
				glColor4f(0.0, 1.0, 0.0, 0.35);
			glVertex2f(-i, view->position.y + halfScreenHeight);
			glVertex2f(-i, view->position.y + (-halfScreenHeight));
		}

		// draw horz along y positive
		for (int i = GRID_SIZE;
				i<halfScreenHeight + view->position.y;
				i+=GRID_SIZE) {
			if (fmod(i, GRID_SIZE * 20.0) == 0.0)
				glColor4f(0.0, 1.0, 0.0, 0.8);
			else
				glColor4f(0.0, 1.0, 0.0, 0.35);
			glVertex2f(view->position.x + (-halfScreenWidth), i);
			glVertex2f(view->position.x + halfScreenWidth, i);
		}

		// draw horz along y negative
		for (int i = GRID_SIZE;
				i<halfScreenHeight - view->position.y;
				i+=GRID_SIZE) {
			if (fmod(i, GRID_SIZE * 20.0) == 0.0)
				glColor4f(0.0, 1.0, 0.0, 0.8);
			else
				glColor4f(0.0, 1.0, 0.0, 0.35);
			glVertex2f(view->position.x + (-halfScreenWidth), -i);
			glVertex2f(view->position.x + halfScreenWidth, -i);
		}

	glEnd();
	glPopMatrix();
}