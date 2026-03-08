#include "confirm_dialog.h"
#include "render.h"
#include "text.h"
#include "graphics.h"
#include "mat4.h"

#define DIALOG_WIDTH 500.0f
#define DIALOG_HEIGHT 110.0f

static void ok_clicked(ConfirmDialog *d)
{
	d->open = false;
	d->confirmed = true;
}

static void cancel_clicked(ConfirmDialog *d)
{
	d->open = false;
}

/* Trampolines — imgui_update_button uses void(*)(void) callbacks,
   so we stash a pointer to the active dialog for the duration of update. */
static ConfirmDialog *active_dialog = NULL;

static void trampoline_ok(void) { ok_clicked(active_dialog); }
static void trampoline_cancel(void) { cancel_clicked(active_dialog); }

void ConfirmDialog_open(ConfirmDialog *d, const char *message)
{
	d->open = true;
	d->confirmed = false;
	d->message = message;
	d->ok_button = (ButtonState){{0, 0}, 30, 12, false, false, false, "OK"};
	d->cancel_button = (ButtonState){{0, 0}, 55, 12, false, false, false, "Cancel"};
}

void ConfirmDialog_update(ConfirmDialog *d, const Input *input)
{
	if (!d->open)
		return;

	if (input->keyEsc) {
		d->open = false;
		return;
	}

	Screen screen = Graphics_get_screen();
	float s = Graphics_get_ui_scale();
	float dw = DIALOG_WIDTH * s;
	float dh = DIALOG_HEIGHT * s;
	float dx = (float)screen.width * 0.5f - dw * 0.5f;
	float dy = (float)screen.height * 0.5f - dh * 0.5f;
	float btn_y = dy + dh - 15.0f * s;
	float cx = dx + dw * 0.5f;

	d->ok_button.position.x = cx - 55.0f * s;
	d->ok_button.position.y = btn_y;
	d->cancel_button.position.x = cx + 15.0f * s;
	d->cancel_button.position.y = btn_y;

	active_dialog = d;
	d->ok_button = imgui_update_button(input, &d->ok_button, trampoline_ok);
	d->cancel_button = imgui_update_button(input, &d->cancel_button, trampoline_cancel);
	active_dialog = NULL;
}

static void render_button(const ButtonState *bs)
{
	float r, g, b, a;
	if (bs->active)       { r = 1; g = 0; b = 0; a = 1; }
	else if (bs->hover)   { r = 1; g = 1; b = 1; a = 1; }
	else                  { r = 1; g = 1; b = 1; a = 0.5f; }

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();
	Text_render(tr, shaders, &proj, &ident,
		bs->text, (float)bs->position.x, (float)bs->position.y,
		r, g, b, a);
}

void ConfirmDialog_render(const ConfirmDialog *d)
{
	if (!d->open)
		return;

	float s = Graphics_get_ui_scale();
	Screen screen = Graphics_get_screen();
	float dw = DIALOG_WIDTH * s;
	float dh = DIALOG_HEIGHT * s;
	float dx = (float)screen.width * 0.5f - dw * 0.5f;
	float dy = (float)screen.height * 0.5f - dh * 0.5f;

	/* Background */
	Render_quad_absolute(dx, dy, dx + dw, dy + dh,
		0.08f, 0.08f, 0.12f, 0.95f);

	/* Border */
	float brc = 0.3f;
	Render_thick_line(dx, dy, dx + dw, dy,
		1.0f * s, brc, brc, brc, 0.8f);
	Render_thick_line(dx, dy + dh, dx + dw, dy + dh,
		1.0f * s, brc, brc, brc, 0.8f);
	Render_thick_line(dx, dy, dx, dy + dh,
		1.0f * s, brc, brc, brc, 0.8f);
	Render_thick_line(dx + dw, dy, dx + dw, dy + dh,
		1.0f * s, brc, brc, brc, 0.8f);

	/* Flush geometry before text */
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();
	Render_flush(&proj, &ident);

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	float center_x = dx + dw * 0.5f;

	/* WARNING! (slightly larger via scaled projection) */
	float scale = 1.4f;
	const char *warn = "WARNING!";
	float ww = Text_measure_width(tr, warn) * scale;
	Mat4 sm = Mat4_scale(scale, scale, 1.0f);
	Mat4 scaled_proj = Mat4_multiply(&proj, &sm);
	Text_render(tr, shaders, &scaled_proj, &ident,
		warn,
		(center_x - ww * 0.5f) / scale,
		(dy + 25.0f * s) / scale,
		1.0f, 0.0f, 0.0f, 1.0f);

	/* Message text */
	float tw = Text_measure_width(tr, d->message);
	Text_render(tr, shaders, &proj, &ident,
		d->message,
		center_x - tw * 0.5f,
		dy + 50.0f * s,
		1.0f, 1.0f, 1.0f, 0.9f);

	/* Buttons */
	render_button(&d->ok_button);
	render_button(&d->cancel_button);
}
