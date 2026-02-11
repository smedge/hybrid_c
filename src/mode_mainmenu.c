#include "mode_mainmenu.h"
#include "render.h"
#include "text.h"
#include "background.h"
#include "view.h"

#include <stdlib.h>
#include <math.h>

#define MENU_BG_SPEED_MULT 3
#define MENU_CAM_SPEED 200.0

static double menu_cam_dx, menu_cam_dy;
static double menu_cam_x, menu_cam_y;

static ButtonState newButton = {{98.0f, 190.0f}, 59, 12, false, false, false, NEW_BUTTON_TEXT};
static ButtonState loadButton = {{98.0f, 208.0f}, 59, 12, false, false, true, LOAD_BUTTON_TEXT};
static ButtonState exitButton = {{98.0f, 228.0f}, 59, 12, false, false, false, EXIT_BUTTON_TEXT};

static void render_menu_text(void);
static void render_menu_button(const ButtonState *buttonState, const bool showBounds);

static void (*quit_callback)();
static void (*gameplay_mode_callback)();

void Mode_Mainmenu_initialize(
	void (*quit)(),
	void (*gameplay_mode)())
{
	quit_callback = quit;
	gameplay_mode_callback = gameplay_mode;

	View_initialize();
	View_set_scale(0.3);
	Background_initialize();

	/* Pick a random 8-directional drift for the camera */
	int dir = rand() % 8;
	double angles[] = {0, 45, 90, 135, 180, 225, 270, 315};
	double rad = angles[dir] * 3.14159265 / 180.0;
	menu_cam_dx = cos(rad) * MENU_CAM_SPEED;
	menu_cam_dy = sin(rad) * MENU_CAM_SPEED;
	menu_cam_x = 0.0;
	menu_cam_y = 0.0;

	Audio_loop_music(MENU_MUSIC_PATH);
}

void Mode_Mainmenu_cleanup(void)
{
	Audio_stop_music();
}

void Mode_Mainmenu_update(
	const Input *input,
	const unsigned int ticks)
{
	Background_update(ticks * MENU_BG_SPEED_MULT);

	/* Slowly drift camera in the chosen direction */
	double dt = ticks / 1000.0;
	menu_cam_x += menu_cam_dx * dt;
	menu_cam_y += menu_cam_dy * dt;
	Position pos = {menu_cam_x, menu_cam_y};
	View_set_position(pos);

	Screen screen = Graphics_get_screen();
	const int fifthScreenWidth = screen.width / 5;
	float menu_top = screen.height * 0.30f;
	newButton.position.x = fifthScreenWidth;
	loadButton.position.x = fifthScreenWidth;
	exitButton.position.x = fifthScreenWidth;
	newButton.position.y = menu_top + 110;
	loadButton.position.y = menu_top + 135;
	exitButton.position.y = menu_top + 160;

	newButton = imgui_update_button(input, &newButton, gameplay_mode_callback);
	exitButton = imgui_update_button(input, &exitButton, quit_callback);

	cursor_update(input);
}

void Mode_Mainmenu_render(void)
{
	Graphics_clear();

	/* Background world pass */
	Screen screen = Graphics_get_screen();
	Mat4 world_proj = Graphics_get_world_projection();
	Mat4 view = View_get_transform(&screen);
	Background_render();
	Render_flush(&world_proj, &view);

	/* UI pass */
	Mat4 ui_proj = Graphics_get_ui_projection();
	Mat4 identity = Mat4_identity();

	render_menu_text();
	bool showBounds = false;
	render_menu_button(&newButton, showBounds);
	render_menu_button(&loadButton, showBounds);
	render_menu_button(&exitButton, showBounds);

	cursor_render();

	Render_flush(&ui_proj, &identity);

	Graphics_flip();
}

static void render_menu_text(void)
{
	Screen screen = Graphics_get_screen();
	int fifthScreenWidth = screen.width / 5;

	TextRenderer *tr = Graphics_get_title_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	float menu_top = screen.height * 0.30f;
	Text_render(tr, shaders, &proj, &ident,
		TITLE_TEXT, (float)fifthScreenWidth, menu_top,
		1.0f, 1.0f, 1.0f, 1.0f);
}

static void render_menu_button(const ButtonState *buttonState, const bool showBounds)
{
	if (showBounds) {
		float bx = (float)buttonState->position.x;
		float by = (float)buttonState->position.y;
		float bw = (float)buttonState->width;
		float bh = (float)buttonState->height;
		Render_line_segment(bx, by, bx, by - bh, 1.0f, 0.0f, 0.0f, 1.0f);
		Render_line_segment(bx, by - bh, bx + bw, by - bh, 1.0f, 0.0f, 0.0f, 1.0f);
		Render_line_segment(bx + bw, by - bh, bx + bw, by, 1.0f, 0.0f, 0.0f, 1.0f);
		Render_line_segment(bx + bw, by, bx, by, 1.0f, 0.0f, 0.0f, 1.0f);
	}

	float r, g, b, a;
	if (buttonState->disabled) {
		r = 1.0f; g = 1.0f; b = 1.0f; a = 0.33f;
	} else if (buttonState->active) {
		r = 1.0f; g = 0.0f; b = 0.0f; a = 1.0f;
	} else if (buttonState->hover) {
		r = 1.0f; g = 1.0f; b = 1.0f; a = 1.0f;
	} else {
		r = 1.0f; g = 1.0f; b = 1.0f; a = 0.50f;
	}

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	Text_render(tr, shaders, &proj, &ident,
		buttonState->text,
		(float)buttonState->position.x, (float)buttonState->position.y,
		r, g, b, a);
}
