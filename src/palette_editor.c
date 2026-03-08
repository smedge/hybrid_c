#include "palette_editor.h"
#include "zone.h"
#include "color.h"
#include "render.h"
#include "graphics.h"
#include "text.h"
#include "mat4.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* Adjustment rates (per second) */
#define HUE_RATE     120.0f
#define SAT_RATE     0.5f
#define VAL_RATE     0.5f
#define ALPHA_RATE   128.0f  /* 0-255 range, ~2 sec full sweep */

/* UI layout */
#define PANEL_X      10.0f
#define PANEL_Y      60.0f
#define SWATCH_SIZE  14.0f
#define SWATCH_PAD   4.0f
#define LINE_H       18.0f
#define LABEL_OFFSET_X (SWATCH_SIZE + 8.0f)

static int selectedSwatch;
static int totalSwatches;
static ColorHSV currentHSV;
static bool tabHeld;

static void cache_hsv_from_swatch(void);
static void get_swatch_rgb(int idx, float *r, float *g, float *b);
static int get_swatch_alpha(int idx);

void PaletteEditor_enter(void)
{
	const Zone *z = Zone_get();
	totalSwatches = 4 + z->cell_type_count * 2;
	selectedSwatch = 0;
	tabHeld = false;

	/* Ensure bg_colors are initialized if zone has none */
	if (!z->has_bg_colors) {
		/* Force default palette colors into zone bg_colors */
		Zone_set_bg_color(0, (ColorRGB){89, 25, 140, 255});
		Zone_set_bg_color(1, (ColorRGB){51, 25, 128, 255});
		Zone_set_bg_color(2, (ColorRGB){115, 20, 102, 255});
		Zone_set_bg_color(3, (ColorRGB){38, 13, 89, 255});
	}

	cache_hsv_from_swatch();
}

void PaletteEditor_exit(void)
{
	/* Changes already persisted via dirty flag */
}

void PaletteEditor_update(const Input *input, unsigned int ticks)
{
	float dt = (float)ticks / 1000.0f;

	/* Tab cycles swatches (one-shot on press) */
	if (input->keyTab && !tabHeld) {
		tabHeld = true;
		selectedSwatch = (selectedSwatch + 1) % totalSwatches;
		cache_hsv_from_swatch();
	}
	if (!input->keyTab)
		tabHeld = false;

	/* Arrow key adjustments */
	bool hsv_changed = false;
	bool alpha_changed = false;
	float alpha_delta = 0.0f;

	if (input->keyLShift) {
		/* Shift+Up/Down = Value */
		if (input->keyUp) {
			currentHSV.v += VAL_RATE * dt;
			if (currentHSV.v > 1.0f) currentHSV.v = 1.0f;
			hsv_changed = true;
		}
		if (input->keyDown) {
			currentHSV.v -= VAL_RATE * dt;
			if (currentHSV.v < 0.0f) currentHSV.v = 0.0f;
			hsv_changed = true;
		}
		/* Shift+Left/Right = Alpha */
		if (input->keyRight) {
			alpha_delta = ALPHA_RATE * dt;
			alpha_changed = true;
		}
		if (input->keyLeft) {
			alpha_delta = -ALPHA_RATE * dt;
			alpha_changed = true;
		}
	} else {
		/* Left/Right = Hue */
		if (input->keyLeft) {
			currentHSV.h -= HUE_RATE * dt;
			if (currentHSV.h < 0.0f) currentHSV.h += 360.0f;
			hsv_changed = true;
		}
		if (input->keyRight) {
			currentHSV.h += HUE_RATE * dt;
			if (currentHSV.h >= 360.0f) currentHSV.h -= 360.0f;
			hsv_changed = true;
		}
		/* Up/Down = Saturation */
		if (input->keyUp) {
			currentHSV.s += SAT_RATE * dt;
			if (currentHSV.s > 1.0f) currentHSV.s = 1.0f;
			hsv_changed = true;
		}
		if (input->keyDown) {
			currentHSV.s -= SAT_RATE * dt;
			if (currentHSV.s < 0.0f) currentHSV.s = 0.0f;
			hsv_changed = true;
		}
	}

	if (!hsv_changed && !alpha_changed) return;

	/* Get current colors for the swatch */
	float r, g, b;
	if (hsv_changed)
		Color_hsv_to_rgb(currentHSV, &r, &g, &b);
	else
		get_swatch_rgb(selectedSwatch, &r, &g, &b);

	unsigned char cr = (unsigned char)(r * 255.0f + 0.5f);
	unsigned char cg = (unsigned char)(g * 255.0f + 0.5f);
	unsigned char cb = (unsigned char)(b * 255.0f + 0.5f);

	/* Apply to the appropriate zone data */
	if (selectedSwatch < 4) {
		Zone_set_bg_color(selectedSwatch, (ColorRGB){cr, cg, cb, 255});
	} else {
		const Zone *z = Zone_get();
		int tile_idx = (selectedSwatch - 4) / 2;
		int is_outline = (selectedSwatch - 4) % 2;

		if (tile_idx < z->cell_type_count) {
			ColorRGB primary = z->cell_types[tile_idx].primaryColor;
			ColorRGB outline = z->cell_types[tile_idx].outlineColor;

			if (is_outline) {
				unsigned char new_a = outline.alpha;
				if (alpha_changed) {
					float a = (float)new_a + alpha_delta;
					if (a < 0.0f) a = 0.0f;
					if (a > 255.0f) a = 255.0f;
					new_a = (unsigned char)(a + 0.5f);
				}
				outline = (ColorRGB){cr, cg, cb, new_a};
			} else {
				unsigned char new_a = primary.alpha;
				if (alpha_changed) {
					float a = (float)new_a + alpha_delta;
					if (a < 0.0f) a = 0.0f;
					if (a > 255.0f) a = 255.0f;
					new_a = (unsigned char)(a + 0.5f);
				}
				primary = (ColorRGB){cr, cg, cb, new_a};
			}

			Zone_set_celltype_colors(tile_idx, primary, outline);
		}
	}
}

void PaletteEditor_render(const Screen *screen)
{
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	float s = Graphics_get_ui_scale();
	float panel_x = PANEL_X * s;
	float panel_y = PANEL_Y * s;
	float swatch_size = SWATCH_SIZE * s;
	float swatch_pad = SWATCH_PAD * s;
	float line_h = LINE_H * s;
	float label_offset_x = (swatch_size + 8.0f * s);

	float x = panel_x;
	float y = panel_y;
	char buf[128];
	const Zone *z = Zone_get();

	/* Panel background */
	float panel_w = 220.0f * s;
	float panel_h = panel_y + (float)(totalSwatches + 8) * line_h;
	Render_quad_absolute(x - 4.0f * s, y - 4.0f * s,
		x + panel_w, panel_h,
		0.0f, 0.0f, 0.0f, 0.65f);

	Render_flush(&proj, &ident);

	/* Title */
	Text_render(tr, shaders, &proj, &ident,
		"PALETTE EDITOR", x, y,
		0.0f, 1.0f, 1.0f, 1.0f);
	y += line_h + 4.0f * s;

	/* --- CLOUDS section --- */
	Text_render(tr, shaders, &proj, &ident,
		"CLOUDS", x, y,
		0.7f, 0.7f, 0.7f, 0.9f);
	y += line_h;

	for (int i = 0; i < 4; i++) {
		float sr, sg, sb;
		get_swatch_rgb(i, &sr, &sg, &sb);

		/* Swatch filled quad */
		Render_quad_absolute(x, y + 1.0f * s,
			x + swatch_size, y + 1.0f * s + swatch_size,
			sr, sg, sb, 1.0f);

		/* Selection border */
		if (i == selectedSwatch) {
			Render_thick_line(x - 1.0f * s, y, x + swatch_size + 1.0f * s, y,
				2.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
			Render_thick_line(x - 1.0f * s, y + swatch_size + 2.0f * s,
				x + swatch_size + 1.0f * s, y + swatch_size + 2.0f * s,
				2.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
			Render_thick_line(x - 1.0f * s, y, x - 1.0f * s, y + swatch_size + 2.0f * s,
				2.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
			Render_thick_line(x + swatch_size + 1.0f * s, y,
				x + swatch_size + 1.0f * s, y + swatch_size + 2.0f * s,
				2.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
		}

		Render_flush(&proj, &ident);

		/* Label */
		snprintf(buf, sizeof(buf), "C%d", i + 1);
		Text_render(tr, shaders, &proj, &ident,
			buf, x + label_offset_x, y + 2.0f * s,
			0.8f, 0.8f, 0.8f, 0.9f);

		/* HSV readout for selected */
		if (i == selectedSwatch) {
			snprintf(buf, sizeof(buf), "H:%.0f S:%.0f V:%.0f",
				currentHSV.h, currentHSV.s * 100.0f, currentHSV.v * 100.0f);
			Text_render(tr, shaders, &proj, &ident,
				buf, x + label_offset_x + 28.0f * s, y + 2.0f * s,
				0.0f, 1.0f, 1.0f, 0.9f);
		}

		y += line_h;
	}

	y += swatch_pad;

	/* --- TILES section --- */
	Text_render(tr, shaders, &proj, &ident,
		"TILES", x, y,
		0.7f, 0.7f, 0.7f, 0.9f);
	y += line_h;

	for (int t = 0; t < z->cell_type_count; t++) {
		int fill_idx = 4 + t * 2;
		int edge_idx = 4 + t * 2 + 1;

		/* Fill swatch */
		{
			float sr, sg, sb;
			get_swatch_rgb(fill_idx, &sr, &sg, &sb);

			Render_quad_absolute(x, y + 1.0f * s,
				x + swatch_size, y + 1.0f * s + swatch_size,
				sr, sg, sb, 1.0f);

			if (fill_idx == selectedSwatch) {
				Render_thick_line(x - 1.0f * s, y, x + swatch_size + 1.0f * s, y,
					2.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
				Render_thick_line(x - 1.0f * s, y + swatch_size + 2.0f * s,
					x + swatch_size + 1.0f * s, y + swatch_size + 2.0f * s,
					2.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
				Render_thick_line(x - 1.0f * s, y, x - 1.0f * s, y + swatch_size + 2.0f * s,
					2.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
				Render_thick_line(x + swatch_size + 1.0f * s, y,
					x + swatch_size + 1.0f * s, y + swatch_size + 2.0f * s,
					2.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
			}

			Render_flush(&proj, &ident);

			snprintf(buf, sizeof(buf), "%s Fill", z->cell_types[t].id);
			Text_render(tr, shaders, &proj, &ident,
				buf, x + label_offset_x, y + 2.0f * s,
				0.8f, 0.8f, 0.8f, 0.9f);

			if (fill_idx == selectedSwatch) {
				int alpha = get_swatch_alpha(fill_idx);
				snprintf(buf, sizeof(buf), "H:%.0f S:%.0f V:%.0f A:%d",
					currentHSV.h, currentHSV.s * 100.0f, currentHSV.v * 100.0f, alpha);
				Text_render(tr, shaders, &proj, &ident,
					buf, x + label_offset_x + 80.0f * s, y + 2.0f * s,
					0.0f, 1.0f, 1.0f, 0.9f);
			}

			y += line_h;
		}

		/* Edge swatch */
		{
			float sr, sg, sb;
			get_swatch_rgb(edge_idx, &sr, &sg, &sb);

			Render_quad_absolute(x, y + 1.0f * s,
				x + swatch_size, y + 1.0f * s + swatch_size,
				sr, sg, sb, 1.0f);

			if (edge_idx == selectedSwatch) {
				Render_thick_line(x - 1.0f * s, y, x + swatch_size + 1.0f * s, y,
					2.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
				Render_thick_line(x - 1.0f * s, y + swatch_size + 2.0f * s,
					x + swatch_size + 1.0f * s, y + swatch_size + 2.0f * s,
					2.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
				Render_thick_line(x - 1.0f * s, y, x - 1.0f * s, y + swatch_size + 2.0f * s,
					2.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
				Render_thick_line(x + swatch_size + 1.0f * s, y,
					x + swatch_size + 1.0f * s, y + swatch_size + 2.0f * s,
					2.0f * s, 1.0f, 1.0f, 1.0f, 1.0f);
			}

			Render_flush(&proj, &ident);

			snprintf(buf, sizeof(buf), "%s Edge", z->cell_types[t].id);
			Text_render(tr, shaders, &proj, &ident,
				buf, x + label_offset_x, y + 2.0f * s,
				0.8f, 0.8f, 0.8f, 0.9f);

			if (edge_idx == selectedSwatch) {
				int alpha = get_swatch_alpha(edge_idx);
				snprintf(buf, sizeof(buf), "H:%.0f S:%.0f V:%.0f A:%d",
					currentHSV.h, currentHSV.s * 100.0f, currentHSV.v * 100.0f, alpha);
				Text_render(tr, shaders, &proj, &ident,
					buf, x + label_offset_x + 80.0f * s, y + 2.0f * s,
					0.0f, 1.0f, 1.0f, 0.9f);
			}

			y += line_h;
		}
	}

	y += swatch_pad + 4.0f * s;

	/* Controls help */
	Text_render(tr, shaders, &proj, &ident,
		"Tab: next swatch", x, y,
		0.5f, 0.5f, 0.5f, 0.8f);
	y += line_h;
	Text_render(tr, shaders, &proj, &ident,
		"L/R: Hue  U/D: Sat", x, y,
		0.5f, 0.5f, 0.5f, 0.8f);
	y += line_h;
	Text_render(tr, shaders, &proj, &ident,
		"Shift+U/D: Val", x, y,
		0.5f, 0.5f, 0.5f, 0.8f);
	y += line_h;
	Text_render(tr, shaders, &proj, &ident,
		"Shift+L/R: Alpha", x, y,
		0.5f, 0.5f, 0.5f, 0.8f);
}

static void cache_hsv_from_swatch(void)
{
	float r, g, b;
	get_swatch_rgb(selectedSwatch, &r, &g, &b);
	currentHSV = Color_rgb_to_hsv(r, g, b);
}

static void get_swatch_rgb(int idx, float *r, float *g, float *b)
{
	const Zone *z = Zone_get();

	if (idx < 4) {
		/* Cloud color */
		*r = z->bg_colors[idx].red / 255.0f;
		*g = z->bg_colors[idx].green / 255.0f;
		*b = z->bg_colors[idx].blue / 255.0f;
	} else {
		int tile_idx = (idx - 4) / 2;
		int is_outline = (idx - 4) % 2;

		if (tile_idx < z->cell_type_count) {
			const ColorRGB *c = is_outline
				? &z->cell_types[tile_idx].outlineColor
				: &z->cell_types[tile_idx].primaryColor;
			*r = c->red / 255.0f;
			*g = c->green / 255.0f;
			*b = c->blue / 255.0f;
		} else {
			*r = *g = *b = 0.5f;
		}
	}
}

static int get_swatch_alpha(int idx)
{
	const Zone *z = Zone_get();

	if (idx < 4) {
		return z->bg_colors[idx].alpha;
	} else {
		int tile_idx = (idx - 4) / 2;
		int is_outline = (idx - 4) % 2;

		if (tile_idx < z->cell_type_count) {
			const ColorRGB *c = is_outline
				? &z->cell_types[tile_idx].outlineColor
				: &z->cell_types[tile_idx].primaryColor;
			return c->alpha;
		}
		return 255;
	}
}
