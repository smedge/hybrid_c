#include "mode_mainmenu.h"
#include "render.h"
#include "text.h"
#include "background.h"
#include "view.h"
#include "savepoint.h"
#include "mat4.h"

#include <stdlib.h>
#include <math.h>

#define MENU_BG_SPEED_MULT 3
#define MENU_CAM_SPEED 200.0

/* Confirmation dialog layout */
#define DIALOG_WIDTH 500.0f
#define DIALOG_HEIGHT 110.0f

static double menu_cam_dx, menu_cam_dy;
static double menu_cam_x, menu_cam_y;

static ButtonState newButton = {{98.0f, 190.0f}, 59, 12, false, false, false, NEW_BUTTON_TEXT};
static ButtonState loadButton = {{98.0f, 208.0f}, 59, 12, false, false, true, LOAD_BUTTON_TEXT};
static ButtonState exitButton = {{98.0f, 228.0f}, 59, 12, false, false, false, EXIT_BUTTON_TEXT};

/* Dialog state */
static bool dialogOpen = false;
static ButtonState okButton = {{0, 0}, 30, 12, false, false, false, "OK"};
static ButtonState cancelButton = {{0, 0}, 55, 12, false, false, false, "Cancel"};

static void render_menu_text(void);
static void render_menu_button(const ButtonState *buttonState, const bool showBounds);

static void (*quit_callback)();
static void (*gameplay_mode_callback)();
static void (*load_game_callback)();

static float measure_text(TextRenderer *tr, const char *text)
{
	float w = 0.0f;
	for (int i = 0; text[i]; i++) {
		int ch = (unsigned char)text[i];
		if (ch >= 32 && ch <= 127)
			w += tr->char_data[ch - 32][6];
	}
	return w;
}

/* Called when New button is clicked */
static void new_game_clicked(void)
{
	if (Savepoint_has_save_file()) {
		dialogOpen = true;
		okButton.hover = false;
		okButton.active = false;
		cancelButton.hover = false;
		cancelButton.active = false;
	} else {
		gameplay_mode_callback();
	}
}

static void dialog_ok_clicked(void)
{
	dialogOpen = false;
	gameplay_mode_callback();
}

static void dialog_cancel_clicked(void)
{
	dialogOpen = false;
}

void Mode_Mainmenu_initialize(
	void (*quit)(),
	void (*gameplay_mode)(),
	void (*load_game)())
{
	quit_callback = quit;
	gameplay_mode_callback = gameplay_mode;
	load_game_callback = load_game;

	/* Enable Load button if save file exists */
	loadButton.disabled = !Savepoint_has_save_file();

	dialogOpen = false;

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

	if (dialogOpen) {
		/* Position dialog buttons */
		float dx = (float)screen.width * 0.5f - DIALOG_WIDTH * 0.5f;
		float dy = (float)screen.height * 0.5f - DIALOG_HEIGHT * 0.5f;
		float btn_y = dy + DIALOG_HEIGHT - 15.0f;
		float center_x = dx + DIALOG_WIDTH * 0.5f;

		okButton.position.x = center_x - 55.0f;
		okButton.position.y = btn_y;
		cancelButton.position.x = center_x + 15.0f;
		cancelButton.position.y = btn_y;

		okButton = imgui_update_button(input, &okButton, dialog_ok_clicked);
		cancelButton = imgui_update_button(input, &cancelButton, dialog_cancel_clicked);
	} else {
		newButton = imgui_update_button(input, &newButton, new_game_clicked);
		loadButton = imgui_update_button(input, &loadButton, load_game_callback);
		exitButton = imgui_update_button(input, &exitButton, quit_callback);
	}

	cursor_update(input);
}

static void render_dialog(const Screen *screen)
{
	float dx = (float)screen->width * 0.5f - DIALOG_WIDTH * 0.5f;
	float dy = (float)screen->height * 0.5f - DIALOG_HEIGHT * 0.5f;

	/* Background */
	Render_quad_absolute(dx, dy, dx + DIALOG_WIDTH, dy + DIALOG_HEIGHT,
		0.08f, 0.08f, 0.12f, 0.95f);

	/* Border */
	float brc = 0.3f;
	Render_thick_line(dx, dy, dx + DIALOG_WIDTH, dy,
		1.0f, brc, brc, brc, 0.8f);
	Render_thick_line(dx, dy + DIALOG_HEIGHT, dx + DIALOG_WIDTH, dy + DIALOG_HEIGHT,
		1.0f, brc, brc, brc, 0.8f);
	Render_thick_line(dx, dy, dx, dy + DIALOG_HEIGHT,
		1.0f, brc, brc, brc, 0.8f);
	Render_thick_line(dx + DIALOG_WIDTH, dy, dx + DIALOG_WIDTH, dy + DIALOG_HEIGHT,
		1.0f, brc, brc, brc, 0.8f);

	/* Flush geometry before text */
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();
	Render_flush(&proj, &ident);

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	float center_x = dx + DIALOG_WIDTH * 0.5f;

	/*
	 * Vertical layout (all in screen coords):
	 *   dy + 25  = WARNING! baseline (scaled 1.4x)
	 *   dy + 50  = message baseline
	 *   dy + DIALOG_HEIGHT - 15 = button baseline
	 */

	/* WARNING! (slightly larger via scaled projection) */
	float scale = 1.4f;
	const char *warn = "WARNING!";
	float ww = measure_text(tr, warn) * scale;
	Mat4 s = Mat4_scale(scale, scale, 1.0f);
	Mat4 scaled_proj = Mat4_multiply(&proj, &s);
	Text_render(tr, shaders, &scaled_proj, &ident,
		warn,
		(center_x - ww * 0.5f) / scale,
		(dy + 25.0f) / scale,
		1.0f, 0.0f, 0.0f, 1.0f);

	/* Message text */
	const char *msg = "This will erase previously saved data.";
	float tw = measure_text(tr, msg);
	Text_render(tr, shaders, &proj, &ident,
		msg,
		center_x - tw * 0.5f,
		dy + 50.0f,
		1.0f, 1.0f, 1.0f, 0.9f);

	/* OK / Cancel buttons */
	render_menu_button(&okButton, false);
	render_menu_button(&cancelButton, false);
}

void Mode_Mainmenu_render(void)
{
	Graphics_clear();

	/* Background bloom pass (blurred only — no raw polygon render) */
	Screen screen = Graphics_get_screen();
	Mat4 world_proj = Graphics_get_world_projection();
	Mat4 view = View_get_transform(&screen);
	{
		int draw_w, draw_h;
		Graphics_get_drawable_size(&draw_w, &draw_h);
		Bloom *bg_bloom = Graphics_get_bg_bloom();

		Bloom_begin_source(bg_bloom);
		Background_render();
		Render_flush(&world_proj, &view);
		Bloom_end_source(bg_bloom, draw_w, draw_h);

		Bloom_blur(bg_bloom);
		Bloom_composite(bg_bloom, draw_w, draw_h);
	}

	/* UI pass */
	Mat4 ui_proj = Graphics_get_ui_projection();
	Mat4 identity = Mat4_identity();

	render_menu_text();
	bool showBounds = false;
	render_menu_button(&newButton, showBounds);
	render_menu_button(&loadButton, showBounds);
	render_menu_button(&exitButton, showBounds);

	if (dialogOpen)
		render_dialog(&screen);

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
