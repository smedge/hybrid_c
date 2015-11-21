#include "hud.h"

static void render_left_status(const Screen *screen);
static void render_right_status(const Screen *screen);
static void render_skill_bar(const Screen *screen);
static void render_skill_button();
static void render_radar(const Screen *screen);

void hud_update(const Input *input, const unsigned int ticks)
{

}

void hud_render(const Screen *screen)
{
	glPushMatrix();
	render_left_status(screen);
	render_right_status(screen);
	render_radar(screen);
	render_skill_bar(screen);
	glPopMatrix();
}

static void render_left_status(const Screen *screen)
{
	glPushMatrix();
	glLineWidth(1.0);
	glColor4f(1.0, 1.0, 1.0, 0.2);
	glTranslatef(10.0, 10.0, 0.0);

	glBegin(GL_LINE_LOOP);
		glVertex2f(0.0, 0.0);
		glVertex2f(0.0, 100.0);
		glVertex2f(250.0, 100.0);
		glVertex2f(250.0, 0.0);
	glEnd();
	glPopMatrix();
}

static void render_right_status(const Screen *screen)
{
	glPushMatrix();
	glLineWidth(1.0);
	glColor4f(1.0, 1.0, 1.0, 0.2);
	glTranslatef(screen->width - 260.0, 10.0, 0.0);

	glBegin(GL_LINE_LOOP);
		glVertex2f(0.0, 0.0);
		glVertex2f(0.0, 100.0);
		glVertex2f(250.0, 100.0);
		glVertex2f(250.0, 0.0);
	glEnd();
	glPopMatrix();
}

static void render_skill_bar(const Screen *screen)
{
	glColor4f(1.0, 1.0, 1.0, 0.2);
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

static void render_radar(const Screen *screen)
{
	glPushMatrix();
	glColor4f(1.0, 1.0, 1.0, 0.2);
	glTranslatef(screen->width - 210.0, screen->height - 210.0, 0.0);

	glBegin(GL_QUADS);
		glVertex2f(0.0, 0.0);
		glVertex2f(0.0, 200.0);
		glVertex2f(200.0, 200.0);
		glVertex2f(200.0, 0.0);
	glEnd();
	glPopMatrix();
}