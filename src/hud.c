#include "hud.h"

static void render_skill_button();
static void render_radar();

void hud_update(const Input *input, const unsigned int ticks)
{

}

void hud_render(const Screen *screen)
{
	glPushMatrix();
	glColor4f(1.0, 1.0, 1.0, 0.2);
	glTranslatef(screen->width - 210.0, screen->height - 210.0, 0.0);
	
	render_radar();

	glPopMatrix();
	glPushMatrix();

	glTranslatef(10.0, screen->height - 60.0, 0.0);
	render_skill_button();
	glTranslatef(60.0, 0.0, 0.0);
	render_skill_button();
	glTranslatef(60.0, 0.0, 0.0);
	render_skill_button();
	glTranslatef(60.0, 0.0, 0.0);
	render_skill_button();
	glTranslatef(60.0, 0.0, 0.0);
	render_skill_button();
	glTranslatef(60.0, 0.0, 0.0);
	render_skill_button();
	glTranslatef(60.0, 0.0, 0.0);
	render_skill_button();
	glTranslatef(60.0, 0.0, 0.0);
	render_skill_button();
	glTranslatef(60.0, 0.0, 0.0);
	render_skill_button();
	glTranslatef(60.0, 0.0, 0.0);
	render_skill_button();

	glPopMatrix();
}

static void render_skill_button()
{
	glBegin(GL_QUADS);
		glVertex2f(0.0, 0.0);
		glVertex2f(0.0, 50.0);
		glVertex2f(50.0, 50.0);
		glVertex2f(50.0, 0.0);
	glEnd();
}

static void render_radar()
{
	glBegin(GL_QUADS);
		glVertex2f(0.0, 0.0);
		glVertex2f(0.0, 200.0);
		glVertex2f(200.0, 200.0);
		glVertex2f(200.0, 0.0);
	glEnd();
}