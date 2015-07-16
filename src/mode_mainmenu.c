#include "mode_mainmenu.h"

static bool hoverNew = false;
static bool hoverLoad = false;
static bool hoverExit = false;

void mode_mainmenu_update(const Input *input, const unsigned int ticks, bool *quit)
{
	if (input->mouseX >= 100 && input->mouseX <= 155 &&
		input->mouseY >= 230 && input->mouseY <= 240) {
		hoverExit = true;

		if (input->mouseLeft)
			*quit = true;
	}
	else {
		hoverExit = false;
	}

	cursor_update(input);
}

void mode_mainmenu_render()
{
	graphics_clear();
	graphics_set_ui_projection();


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

	if (hoverExit)
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	else
		glColor4f(1.0f, 1.0f, 1.0f, 0.50f);

	glRasterPos2f(100.0f, 240.0f);
	ftglRenderFont(font, "EXIT", FTGL_RENDER_ALL);
	
	ftglDestroyFont(font);

	glPopMatrix();


	cursor_render();

	graphics_flip();
}