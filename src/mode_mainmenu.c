#include "mode_mainmenu.h"

static ButtonState newButton = {{98.0, 190.0}, 59, 12, false, false, "NEW"};
static ButtonState loadButton = {{98.0, 208.0}, 59, 12, false, false, "LOAD"};
static ButtonState exitButton = {{98.0, 228.0}, 59, 12, false, false, "EXIT"};
static FTGLfont *font = 0;

static void render_menu_text();
static void render_menu_button(const ButtonState *buttonState, bool showBounds);

void mode_mainmenu_initialize()
{
	font = ftglCreatePixmapFont(TITLE_FONT_PATH);
	if(!font) {
		puts("error: failed to load font");
		exit(-1);
	}

	audio_loop_music(MENU_MUSIC_PATH);
}

void mode_mainmenu_cleanup()
{
	ftglDestroyFont(font);
	audio_stop_music();
}

void mode_mainmenu_update(
	const Input *input, 
	const unsigned int ticks,
	void (*quit)(),
	void (*mode)())
{
	Screen screen = graphics_get_screen();
	int fifthScreenWidth = screen.width / 5;
	newButton.position.x = fifthScreenWidth;
	loadButton.position.x = fifthScreenWidth;
	exitButton.position.x = fifthScreenWidth;

	imgui_update_button(input, &newButton, mode);
	imgui_update_button(input, &exitButton, quit);

	cursor_update(input);
}

void mode_mainmenu_render()
{
	graphics_clear();
	graphics_set_ui_projection();
	
	render_menu_text();
	bool showBounds = false;
	render_menu_button(&newButton, showBounds);
	render_menu_button(&loadButton, showBounds);
	render_menu_button(&exitButton, showBounds);

	cursor_render();

	graphics_flip();
}

static void render_menu_text() 
{
	Screen screen = graphics_get_screen();
	int fifthScreenWidth = screen.width / 5;
	ftglSetFontFaceSize(font, 80, 72);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glRasterPos2f(fifthScreenWidth, 100.0f);
	ftglRenderFont(font, "HYBRID", FTGL_RENDER_ALL);
}

static void render_menu_button(const ButtonState *buttonState, bool showBounds)
{
	glPushMatrix();

	if (showBounds) {
		glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
		glLineWidth(1.0);
		glBegin(GL_LINE_LOOP);
			glVertex2f(buttonState->position.x, buttonState->position.y);
			glVertex2f(buttonState->position.x, buttonState->position.y - buttonState->height);
			glVertex2f(buttonState->position.x + buttonState->width, buttonState->position.y - buttonState->height);
			glVertex2f(buttonState->position.x + buttonState->width, buttonState->position.y);
		glEnd();
	}

	ftglSetFontFaceSize(font, 20, 72);
	if (buttonState->active)
		glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
	else if (buttonState->hover)
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	else
		glColor4f(1.0f, 1.0f, 1.0f, 0.50f);
	glRasterPos2f(buttonState->position.x, buttonState->position.y);
	ftglRenderFont(font, buttonState->text, FTGL_RENDER_ALL);
	
	glPopMatrix();
}