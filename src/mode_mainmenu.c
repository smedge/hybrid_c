#include "mode_mainmenu.h"

//static bool hoverNew = false;
//static bool hoverLoad = false;
static bool hoverExit = false;

static bool clickBeginExit = false;

static void update_exit_button(const Input *input, bool *quit);
static void render_menu_text();

void mode_mainmenu_update(const Input *input, const unsigned int ticks, bool *quit)
{
	update_exit_button(input, quit);
	cursor_update(input);
}

void mode_mainmenu_render()
{
	graphics_clear();
	graphics_set_ui_projection();

	render_menu_text();
	cursor_render();

	graphics_flip();
}

static void update_exit_button(const Input *input, bool *quit) {
	if (input->mouseX >= 98 && input->mouseX <= 157 &&
		input->mouseY >= 228 && input->mouseY <= 242) {
		hoverExit = true;

		// begin quititng on mouse down
		if (input->mouseLeft) {
			clickBeginExit = true;
		}

		// quit on mouse up
		if (clickBeginExit && !input->mouseLeft) {
			*quit = true;
		}
	}
	else {
		hoverExit = false;
		clickBeginExit = false;
	}
}

static void render_menu_text() {
	glPushMatrix();

	FTGLfont *font = ftglCreatePixmapFont("./resources/fonts/square_sans_serif_7.ttf");

	// if(!font)
	//     exit();

	ftglSetFontFaceSize(font, 80, 80);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glRasterPos2f(100.0f, 100.0f);
	ftglRenderFont(font, "HYBRID", FTGL_RENDER_ALL);

	ftglSetFontFaceSize(font, 20, 20);
	glColor4f(1.0f, 1.0f, 1.0f, 0.50f);
	
	glRasterPos2f(100.0f, 200.0f);
	ftglRenderFont(font, "NEW", FTGL_RENDER_ALL);

	glRasterPos2f(100.0f, 220.0f);
	ftglRenderFont(font, "LOAD", FTGL_RENDER_ALL);

	if (clickBeginExit)
		glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
	else if (hoverExit)
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	else
		glColor4f(1.0f, 1.0f, 1.0f, 0.50f);

	glRasterPos2f(100.0f, 240.0f);
	ftglRenderFont(font, "EXIT", FTGL_RENDER_ALL);
	
	ftglDestroyFont(font);

	glPopMatrix();
}