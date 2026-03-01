#include "settings.h"

#include "render.h"
#include "text.h"
#include "graphics.h"
#include "batch.h"
#include "view.h"
#include "map_lighting.h"
#include "map_reflect.h"
#include "map.h"
#include "audio.h"

#include <stdio.h>
#include <string.h>

/* Layout constants (matching catalog) */
#define SETTINGS_WIDTH 600.0f
#define SETTINGS_HEIGHT 440.0f
#define TAB_WIDTH 140.0f
#define TAB_HEIGHT 35.0f
#define TAB_GAP 5.0f
#define TAB_CHAMF 6.0f

#define BUTTON_WIDTH 80.0f
#define BUTTON_HEIGHT 30.0f
#define BUTTON_GAP 10.0f

#define TOGGLE_WIDTH 50.0f
#define TOGGLE_HEIGHT 22.0f

/* Tabs */
#define TAB_GRAPHICS 0
#define TAB_AUDIO    1
#define TAB_KEYBINDS 2
#define TAB_COUNT 3

static const char *tab_names[TAB_COUNT] = {
	"Graphics",
	"Audio",
	"Keybinds"
};

/* Slider constants */
#define SLIDER_WIDTH   200.0f
#define SLIDER_HEIGHT   12.0f
#define SLIDER_KNOB_W   10.0f
#define SLIDER_KNOB_H   20.0f
#define SLIDER_LEFT_PAD  16.0f   /* space for "0" + gap left of bar */
#define SLIDER_RIGHT_PAD 72.0f   /* space for gap + "100" + gap + value right of bar */
#define SLIDER_COMBO_W  (SLIDER_LEFT_PAD + SLIDER_WIDTH + SLIDER_RIGHT_PAD)

/* State */
static bool settingsOpen = false;
static int selectedTab = 0;

/* Pending values (edited but not applied) */
static bool pendingMultisampling = true;
static bool pendingAntialiasing = true;
static bool pendingFullscreen = false;
static bool pendingPixelSnapping = true;
static bool pendingBloom = true;
static bool pendingLighting = true;
static bool pendingReflections = true;
static bool pendingCircuitTraces = true;

/* Audio pending values */
static float pendingMusicVol = 1.0f;
static float pendingSFXVol   = 1.0f;
static float pendingVoiceVol = 1.0f;
static float origMusicVol = 1.0f;
static float origSFXVol   = 1.0f;
static float origVoiceVol = 1.0f;
static int draggingSlider = -1;  /* -1=none, 0=music, 1=sfx, 2=voice */

/* Mouse tracking */
static bool mouseWasDown = false;

#define SETTINGS_FILE_PATH "./settings.cfg"

/* Helper: compute panel rect (centered on screen, same as catalog) */
static void get_panel_rect(const Screen *screen,
	float *px, float *py, float *pw, float *ph)
{
	*pw = SETTINGS_WIDTH;
	*ph = SETTINGS_HEIGHT;
	*px = ((float)screen->width - *pw) * 0.5f;
	*py = ((float)screen->height - *ph) * 0.5f - 30.0f;
}

void Settings_load(void)
{
	printf("Settings_load: attempting to open %s\n", SETTINGS_FILE_PATH);
	FILE *f = fopen(SETTINGS_FILE_PATH, "r");
	if (!f) {
		printf("Settings_load: file not found, using defaults\n");
		return;
	}

	char key[64];
	int value;
	while (fscanf(f, "%63s %d", key, &value) == 2) {
		printf("Settings_load: parsed key='%s' value=%d\n", key, value);
		if (strcmp(key, "multisampling") == 0)
			Graphics_set_multisampling(value != 0);
		else if (strcmp(key, "antialiasing") == 0)
			Graphics_set_antialiasing(value != 0);
		else if (strcmp(key, "fullscreen") == 0)
			Graphics_set_fullscreen(value != 0);
		else if (strcmp(key, "pixel_snapping") == 0)
			View_set_pixel_snapping(value != 0);
		else if (strcmp(key, "bloom") == 0)
			Graphics_set_bloom_enabled(value != 0);
		else if (strcmp(key, "lighting") == 0)
			MapLighting_set_enabled(value != 0);
		else if (strcmp(key, "reflections") == 0)
			MapReflect_set_enabled(value != 0);
		else if (strcmp(key, "circuit_traces") == 0)
			Map_set_circuit_traces(value != 0);
		else if (strcmp(key, "music_volume") == 0)
			Audio_set_master_music(value / 100.0f);
		else if (strcmp(key, "sfx_volume") == 0)
			Audio_set_master_sfx(value / 100.0f);
		else if (strcmp(key, "voice_volume") == 0)
			Audio_set_master_voice(value / 100.0f);
	}
	fclose(f);
	printf("Settings_load: done. ms=%d aa=%d fs=%d\n",
		Graphics_get_multisampling(), Graphics_get_antialiasing(),
		Graphics_get_fullscreen());
}

void Settings_save(void)
{
	FILE *f = fopen(SETTINGS_FILE_PATH, "w");
	if (!f) {
		printf("Settings: failed to write %s\n", SETTINGS_FILE_PATH);
		return;
	}

	fprintf(f, "multisampling %d\n", Graphics_get_multisampling() ? 1 : 0);
	fprintf(f, "antialiasing %d\n", Graphics_get_antialiasing() ? 1 : 0);
	fprintf(f, "fullscreen %d\n", Graphics_get_fullscreen() ? 1 : 0);
	fprintf(f, "pixel_snapping %d\n", View_get_pixel_snapping() ? 1 : 0);
	fprintf(f, "bloom %d\n", Graphics_get_bloom_enabled() ? 1 : 0);
	fprintf(f, "lighting %d\n", MapLighting_get_enabled() ? 1 : 0);
	fprintf(f, "reflections %d\n", MapReflect_get_enabled() ? 1 : 0);
	fprintf(f, "circuit_traces %d\n", Map_get_circuit_traces() ? 1 : 0);
	fprintf(f, "music_volume %d\n", (int)(Audio_get_master_music() * 100.0f + 0.5f));
	fprintf(f, "sfx_volume %d\n", (int)(Audio_get_master_sfx() * 100.0f + 0.5f));
	fprintf(f, "voice_volume %d\n", (int)(Audio_get_master_voice() * 100.0f + 0.5f));
	fclose(f);
	printf("Settings saved to %s\n", SETTINGS_FILE_PATH);
}

void Settings_initialize(void)
{
	settingsOpen = false;
	selectedTab = 0;
	mouseWasDown = false;
	pendingMultisampling = Graphics_get_multisampling();
	pendingAntialiasing = Graphics_get_antialiasing();
	pendingFullscreen = Graphics_get_fullscreen();
	pendingPixelSnapping = View_get_pixel_snapping();
	pendingBloom = Graphics_get_bloom_enabled();
	pendingLighting = MapLighting_get_enabled();
	pendingReflections = MapReflect_get_enabled();
	pendingCircuitTraces = Map_get_circuit_traces();
	pendingMusicVol = Audio_get_master_music();
	pendingSFXVol   = Audio_get_master_sfx();
	pendingVoiceVol = Audio_get_master_voice();
}

void Settings_cleanup(void)
{
}

bool Settings_is_open(void)
{
	return settingsOpen;
}

void Settings_toggle(void)
{
	settingsOpen = !settingsOpen;
	if (settingsOpen) {
		mouseWasDown = false;
		draggingSlider = -1;
		/* Copy current values to pending */
		pendingMultisampling = Graphics_get_multisampling();
		pendingAntialiasing = Graphics_get_antialiasing();
		pendingFullscreen = Graphics_get_fullscreen();
		pendingPixelSnapping = View_get_pixel_snapping();
		pendingBloom = Graphics_get_bloom_enabled();
		pendingLighting = MapLighting_get_enabled();
		pendingReflections = MapReflect_get_enabled();
		pendingCircuitTraces = Map_get_circuit_traces();
		pendingMusicVol = Audio_get_master_music();
		pendingSFXVol   = Audio_get_master_sfx();
		pendingVoiceVol = Audio_get_master_voice();
		origMusicVol = pendingMusicVol;
		origSFXVol   = pendingSFXVol;
		origVoiceVol = pendingVoiceVol;
	}
}

void Settings_update(const Input *input, const unsigned int ticks)
{
	(void)ticks;

	if (!settingsOpen)
		return;

	/* ESC closes settings (cancel) */
	if (input->keyEsc) {
		Audio_set_master_music(origMusicVol);
		Audio_set_master_sfx(origSFXVol);
		Audio_set_master_voice(origVoiceVol);
		settingsOpen = false;
		mouseWasDown = false;
		return;
	}

	Screen screen = Graphics_get_screen();
	float mx = (float)input->mouseX;
	float my = (float)input->mouseY;

	float px, py, pw, ph;
	get_panel_rect(&screen, &px, &py, &pw, &ph);

	bool clicked = input->mouseLeft && !mouseWasDown;

	/* Tab clicks */
	if (clicked) {
		float tab_x = px + TAB_GAP;
		float tab_y = py + TAB_GAP;
		for (int t = 0; t < TAB_COUNT; t++) {
			if (mx >= tab_x && mx <= tab_x + TAB_WIDTH &&
				my >= tab_y && my <= tab_y + TAB_HEIGHT) {
				selectedTab = t;
				break;
			}
			tab_y += TAB_HEIGHT + TAB_GAP;
		}
	}

	/* Content area */
	float content_x = px + TAB_WIDTH + TAB_GAP * 2.0f;
	float content_y = py + TAB_GAP;
	float content_width = pw - TAB_WIDTH - TAB_GAP * 2.0f;
	float row_width = 190.0f + TOGGLE_WIDTH;
	float row_offset = (content_width - row_width) * 0.5f;
	float toggle_x = content_x + row_offset + 190.0f;

	/* Graphics tab toggle clicks */
	if (clicked && selectedTab == TAB_GRAPHICS) {
		/* Fullscreen toggle */
		float row_y = content_y + 30.0f;
		if (mx >= toggle_x && mx <= toggle_x + TOGGLE_WIDTH &&
			my >= row_y && my <= row_y + TOGGLE_HEIGHT) {
			pendingFullscreen = !pendingFullscreen;
			printf("Toggle fullscreen -> %d\n", pendingFullscreen);
		}

		/* Multisampling toggle */
		row_y += 40.0f;
		if (mx >= toggle_x && mx <= toggle_x + TOGGLE_WIDTH &&
			my >= row_y && my <= row_y + TOGGLE_HEIGHT) {
			pendingMultisampling = !pendingMultisampling;
			printf("Toggle multisampling -> %d\n", pendingMultisampling);
		}

		/* Antialiasing toggle */
		row_y += 40.0f;
		if (mx >= toggle_x && mx <= toggle_x + TOGGLE_WIDTH &&
			my >= row_y && my <= row_y + TOGGLE_HEIGHT) {
			pendingAntialiasing = !pendingAntialiasing;
			printf("Toggle antialiasing -> %d\n", pendingAntialiasing);
		}

		/* Pixel Snapping toggle */
		row_y += 40.0f;
		if (mx >= toggle_x && mx <= toggle_x + TOGGLE_WIDTH &&
			my >= row_y && my <= row_y + TOGGLE_HEIGHT) {
			pendingPixelSnapping = !pendingPixelSnapping;
			printf("Toggle pixel_snapping -> %d\n", pendingPixelSnapping);
		}

		/* Bloom toggle */
		row_y += 40.0f;
		if (mx >= toggle_x && mx <= toggle_x + TOGGLE_WIDTH &&
			my >= row_y && my <= row_y + TOGGLE_HEIGHT) {
			pendingBloom = !pendingBloom;
		}

		/* Lighting toggle */
		row_y += 40.0f;
		if (mx >= toggle_x && mx <= toggle_x + TOGGLE_WIDTH &&
			my >= row_y && my <= row_y + TOGGLE_HEIGHT) {
			pendingLighting = !pendingLighting;
		}

		/* Reflections toggle */
		row_y += 40.0f;
		if (mx >= toggle_x && mx <= toggle_x + TOGGLE_WIDTH &&
			my >= row_y && my <= row_y + TOGGLE_HEIGHT) {
			pendingReflections = !pendingReflections;
		}

		/* Circuit Traces toggle */
		row_y += 40.0f;
		if (mx >= toggle_x && mx <= toggle_x + TOGGLE_WIDTH &&
			my >= row_y && my <= row_y + TOGGLE_HEIGHT) {
			pendingCircuitTraces = !pendingCircuitTraces;
		}
	}

	/* Audio tab slider interaction */
	if (selectedTab == TAB_AUDIO) {
		float combo_x = content_x + (content_width - SLIDER_COMBO_W) * 0.5f;
		float slider_x = combo_x + SLIDER_LEFT_PAD;
		float group_start = content_y + 40.0f;
		float group_spacing = 75.0f;
		float knob_offset = 22.0f;  /* label height + gap before slider */

		float *volumes[3] = { &pendingMusicVol, &pendingSFXVol, &pendingVoiceVol };
		void (*setters[3])(float) = { Audio_set_master_music, Audio_set_master_sfx, Audio_set_master_voice };

		/* Mouse down on a slider track → begin drag */
		if (clicked) {
			for (int s = 0; s < 3; s++) {
				float knob_y = group_start + s * group_spacing + knob_offset;
				if (mx >= slider_x && mx <= slider_x + SLIDER_WIDTH &&
					my >= knob_y && my <= knob_y + SLIDER_KNOB_H) {
					draggingSlider = s;
					float val = (mx - slider_x) / SLIDER_WIDTH;
					if (val < 0.0f) val = 0.0f;
					if (val > 1.0f) val = 1.0f;
					*volumes[s] = val;
					setters[s](val);
					break;
				}
			}
		}

		/* Continue drag while mouse held */
		if (draggingSlider >= 0 && input->mouseLeft) {
			float val = (mx - slider_x) / SLIDER_WIDTH;
			if (val < 0.0f) val = 0.0f;
			if (val > 1.0f) val = 1.0f;
			*volumes[draggingSlider] = val;
			setters[draggingSlider](val);
		}

		/* Release drag */
		if (draggingSlider >= 0 && !input->mouseLeft) {
			draggingSlider = -1;
		}
	}

	/* Button clicks */
	if (clicked) {
		float btn_y = py + ph - BUTTON_HEIGHT - BUTTON_GAP;
		float btn_gap = 40.0f;
		float btn_total = BUTTON_WIDTH * 2.0f + btn_gap;
		float btn_offset = (content_width - btn_total) * 0.5f;

		/* Cancel button (left side) */
		float cancel_x = content_x + btn_offset;
		if (mx >= cancel_x && mx <= cancel_x + BUTTON_WIDTH &&
			my >= btn_y && my <= btn_y + BUTTON_HEIGHT) {
			/* Discard pending, restore audio to pre-open values */
			Audio_set_master_music(origMusicVol);
			Audio_set_master_sfx(origSFXVol);
			Audio_set_master_voice(origVoiceVol);
			settingsOpen = false;
			mouseWasDown = false;
			return;
		}

		/* OK button (right side) */
		float ok_x = cancel_x + BUTTON_WIDTH + btn_gap;
		if (mx >= ok_x && mx <= ok_x + BUTTON_WIDTH &&
			my >= btn_y && my <= btn_y + BUTTON_HEIGHT) {
			/* Apply pending values */
			printf("Settings OK: ms=%d aa=%d fs=%d\n",
				pendingMultisampling, pendingAntialiasing,
				pendingFullscreen);
			bool msChanged = (pendingMultisampling != Graphics_get_multisampling());
			bool aaChanged = (pendingAntialiasing != Graphics_get_antialiasing());
			bool fsChanged = (pendingFullscreen != Graphics_get_fullscreen());
			Graphics_set_multisampling(pendingMultisampling);
			Graphics_set_antialiasing(pendingAntialiasing);
			Graphics_set_fullscreen(pendingFullscreen);
			View_set_pixel_snapping(pendingPixelSnapping);
			Graphics_set_bloom_enabled(pendingBloom);
			MapLighting_set_enabled(pendingLighting);
			MapReflect_set_enabled(pendingReflections);
			Map_set_circuit_traces(pendingCircuitTraces);
			Audio_set_master_music(pendingMusicVol);
			Audio_set_master_sfx(pendingSFXVol);
			Audio_set_master_voice(pendingVoiceVol);
			Settings_save();
			if (msChanged || aaChanged || fsChanged)
				Graphics_recreate();
			settingsOpen = false;
			mouseWasDown = false;
			return;
		}
	}

	mouseWasDown = input->mouseLeft;
}

void Settings_render(const Screen *screen)
{
	if (!settingsOpen)
		return;

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	float px, py, pw, ph;
	get_panel_rect(screen, &px, &py, &pw, &ph);

	/*
	 * Pass 1: All geometry
	 */

	/* Panel background */
	Render_quad_absolute(px, py, px + pw, py + ph,
		0.08f, 0.08f, 0.12f, 0.95f);

	/* Panel border */
	float brc = 0.3f;
	Render_thick_line(px, py, px + pw, py, 1.0f, brc, brc, brc, 0.8f);
	Render_thick_line(px, py + ph, px + pw, py + ph, 1.0f, brc, brc, brc, 0.8f);
	Render_thick_line(px, py, px, py + ph, 1.0f, brc, brc, brc, 0.8f);
	Render_thick_line(px + pw, py, px + pw, py + ph, 1.0f, brc, brc, brc, 0.8f);

	/* Tab backgrounds */
	float tab_x = px + TAB_GAP;
	float tab_y = py + TAB_GAP;
	for (int t = 0; t < TAB_COUNT; t++) {
		bool selected = (t == selectedTab);
		float tr_col = selected ? 0.15f : 0.1f;
		float tg_col = selected ? 0.15f : 0.1f;
		float tb_col = selected ? 0.25f : 0.15f;

		if (selected) {
			/* Chamfered fill: sharp NW + SE, chamfered NE + SW */
			float c = TAB_CHAMF;
			float tx0 = tab_x, ty0 = tab_y;
			float tx1 = tab_x + TAB_WIDTH, ty1 = tab_y + TAB_HEIGHT;
			float vx[6] = { tx0,      tx1 - c, tx1,
			                tx1,      tx0 + c, tx0 };
			float vy[6] = { ty0,      ty0,     ty0 + c,
			                ty1,      ty1,     ty1 - c };
			float fcx = (tx0 + tx1) * 0.5f;
			float fcy = (ty0 + ty1) * 0.5f;
			BatchRenderer *batch = Graphics_get_batch();
			for (int v = 0; v < 6; v++) {
				int nv = (v + 1) % 6;
				Batch_push_triangle_vertices(batch,
					fcx, fcy, vx[v], vy[v], vx[nv], vy[nv],
					tr_col, tg_col, tb_col, 0.9f);
			}
			/* Chamfered border */
			float tc = 0.5f;
			for (int v = 0; v < 6; v++) {
				int nv = (v + 1) % 6;
				Render_thick_line(vx[v], vy[v], vx[nv], vy[nv],
					1.0f, tc, tc, 1.0f, 0.8f);
			}
		} else {
			Render_quad_absolute(tab_x, tab_y,
				tab_x + TAB_WIDTH, tab_y + TAB_HEIGHT,
				tr_col, tg_col, tb_col, 0.9f);
		}

		tab_y += TAB_HEIGHT + TAB_GAP;
	}

	/* Separator line */
	float sep_x = px + TAB_WIDTH + TAB_GAP * 1.5f;
	Render_thick_line(sep_x, py + TAB_GAP, sep_x, py + ph - TAB_GAP,
		1.0f, 0.25f, 0.25f, 0.3f, 0.6f);

	/* Content area */
	float content_x = px + TAB_WIDTH + TAB_GAP * 2.0f;
	float content_y = py + TAB_GAP;
	float content_width = pw - TAB_WIDTH - TAB_GAP * 2.0f;
	float row_width = 190.0f + TOGGLE_WIDTH;
	float row_offset = (content_width - row_width) * 0.5f;
	float label_x = content_x + row_offset;
	float toggle_x = label_x + 190.0f;

	if (selectedTab == TAB_GRAPHICS) {
		/* Fullscreen toggle */
		float row_y = content_y + 30.0f;

		/* Toggle button bg */
		if (pendingFullscreen) {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.1f, 0.4f, 0.1f, 0.9f);
		} else {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.2f, 0.2f, 0.2f, 0.9f);
		}
		/* Toggle border */
		Render_thick_line(toggle_x, row_y,
			toggle_x + TOGGLE_WIDTH, row_y, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y + TOGGLE_HEIGHT,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.6f, 0.6f);
		Render_thick_line(toggle_x, row_y,
			toggle_x, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x + TOGGLE_WIDTH, row_y,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);

		/* Multisampling toggle */
		row_y += 40.0f;
		if (pendingMultisampling) {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.1f, 0.4f, 0.1f, 0.9f);
		} else {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.2f, 0.2f, 0.2f, 0.9f);
		}
		Render_thick_line(toggle_x, row_y,
			toggle_x + TOGGLE_WIDTH, row_y, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y + TOGGLE_HEIGHT,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y,
			toggle_x, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x + TOGGLE_WIDTH, row_y,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);

		/* Antialiasing toggle */
		row_y += 40.0f;
		if (pendingAntialiasing) {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.1f, 0.4f, 0.1f, 0.9f);
		} else {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.2f, 0.2f, 0.2f, 0.9f);
		}
		Render_thick_line(toggle_x, row_y,
			toggle_x + TOGGLE_WIDTH, row_y, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y + TOGGLE_HEIGHT,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y,
			toggle_x, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x + TOGGLE_WIDTH, row_y,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);

		/* Pixel Snapping toggle */
		row_y += 40.0f;
		if (pendingPixelSnapping) {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.1f, 0.4f, 0.1f, 0.9f);
		} else {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.2f, 0.2f, 0.2f, 0.9f);
		}
		Render_thick_line(toggle_x, row_y,
			toggle_x + TOGGLE_WIDTH, row_y, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y + TOGGLE_HEIGHT,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y,
			toggle_x, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x + TOGGLE_WIDTH, row_y,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);

		/* Bloom toggle */
		row_y += 40.0f;
		if (pendingBloom) {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.1f, 0.4f, 0.1f, 0.9f);
		} else {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.2f, 0.2f, 0.2f, 0.9f);
		}
		Render_thick_line(toggle_x, row_y,
			toggle_x + TOGGLE_WIDTH, row_y, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y + TOGGLE_HEIGHT,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y,
			toggle_x, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x + TOGGLE_WIDTH, row_y,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);

		/* Lighting toggle */
		row_y += 40.0f;
		if (pendingLighting) {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.1f, 0.4f, 0.1f, 0.9f);
		} else {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.2f, 0.2f, 0.2f, 0.9f);
		}
		Render_thick_line(toggle_x, row_y,
			toggle_x + TOGGLE_WIDTH, row_y, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y + TOGGLE_HEIGHT,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y,
			toggle_x, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x + TOGGLE_WIDTH, row_y,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);

		/* Reflections toggle */
		row_y += 40.0f;
		if (pendingReflections) {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.1f, 0.4f, 0.1f, 0.9f);
		} else {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.2f, 0.2f, 0.2f, 0.9f);
		}
		Render_thick_line(toggle_x, row_y,
			toggle_x + TOGGLE_WIDTH, row_y, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y + TOGGLE_HEIGHT,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y,
			toggle_x, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x + TOGGLE_WIDTH, row_y,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);

		/* Circuit Traces toggle */
		row_y += 40.0f;
		if (pendingCircuitTraces) {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.1f, 0.4f, 0.1f, 0.9f);
		} else {
			Render_quad_absolute(toggle_x, row_y,
				toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT,
				0.2f, 0.2f, 0.2f, 0.9f);
		}
		Render_thick_line(toggle_x, row_y,
			toggle_x + TOGGLE_WIDTH, row_y, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y + TOGGLE_HEIGHT,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x, row_y,
			toggle_x, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);
		Render_thick_line(toggle_x + TOGGLE_WIDTH, row_y,
			toggle_x + TOGGLE_WIDTH, row_y + TOGGLE_HEIGHT, 1.0f, 0.4f, 0.4f, 0.4f, 0.6f);

	}

	/* Audio tab slider geometry */
	if (selectedTab == TAB_AUDIO) {
		float combo_x = content_x + (content_width - SLIDER_COMBO_W) * 0.5f;
		float slider_x = combo_x + SLIDER_LEFT_PAD;
		float group_start = content_y + 40.0f;
		float group_spacing = 75.0f;
		float knob_offset = 22.0f;

		float volumes[3] = { pendingMusicVol, pendingSFXVol, pendingVoiceVol };

		for (int s = 0; s < 3; s++) {
			float knob_y = group_start + s * group_spacing + knob_offset;
			float track_y = knob_y + (SLIDER_KNOB_H - SLIDER_HEIGHT) * 0.5f;
			float knob_x = slider_x + volumes[s] * SLIDER_WIDTH - SLIDER_KNOB_W * 0.5f;
			float fill_end = slider_x + volumes[s] * SLIDER_WIDTH;

			/* Track background */
			Render_quad_absolute(slider_x, track_y,
				slider_x + SLIDER_WIDTH, track_y + SLIDER_HEIGHT,
				0.15f, 0.15f, 0.2f, 0.9f);

			/* Fill */
			if (fill_end > slider_x) {
				Render_quad_absolute(slider_x, track_y,
					fill_end, track_y + SLIDER_HEIGHT,
					0.3f, 0.5f, 0.8f, 0.9f);
			}

			/* Knob */
			Render_quad_absolute(knob_x, knob_y,
				knob_x + SLIDER_KNOB_W, knob_y + SLIDER_KNOB_H,
				0.8f, 0.85f, 0.95f, 0.95f);
			/* Knob border */
			Render_thick_line(knob_x, knob_y,
				knob_x + SLIDER_KNOB_W, knob_y, 1.0f, 0.5f, 0.5f, 0.6f, 0.8f);
			Render_thick_line(knob_x, knob_y + SLIDER_KNOB_H,
				knob_x + SLIDER_KNOB_W, knob_y + SLIDER_KNOB_H, 1.0f, 0.5f, 0.5f, 0.6f, 0.8f);
			Render_thick_line(knob_x, knob_y,
				knob_x, knob_y + SLIDER_KNOB_H, 1.0f, 0.5f, 0.5f, 0.6f, 0.8f);
			Render_thick_line(knob_x + SLIDER_KNOB_W, knob_y,
				knob_x + SLIDER_KNOB_W, knob_y + SLIDER_KNOB_H, 1.0f, 0.5f, 0.5f, 0.6f, 0.8f);
		}
	}

	/* Buttons */
	float btn_y = py + ph - BUTTON_HEIGHT - BUTTON_GAP;
	float btn_gap = 40.0f;
	float btn_total = BUTTON_WIDTH * 2.0f + btn_gap;
	float btn_offset = (content_width - btn_total) * 0.5f;

	/* Cancel button — chamfered NE+SW */
	float cancel_x = content_x + btn_offset;
	{
		BatchRenderer *batch = Graphics_get_batch();
		float bx = cancel_x, by = btn_y;
		float bw = BUTTON_WIDTH, bh = BUTTON_HEIGHT;
		float ch = 8.0f;
		float cvx[6] = { bx,        bx + bw - ch, bx + bw,
		                  bx + bw,   bx + ch,      bx };
		float cvy[6] = { by,        by,            by + ch,
		                  by + bh,   by + bh,       by + bh - ch };
		float cx = bx + bw * 0.5f, cy = by + bh * 0.5f;
		for (int i = 0; i < 6; i++) {
			int j = (i + 1) % 6;
			Batch_push_triangle_vertices(batch,
				cx, cy, cvx[i], cvy[i], cvx[j], cvy[j],
				0.15f, 0.12f, 0.12f, 0.9f);
		}
		for (int i = 0; i < 6; i++) {
			int j = (i + 1) % 6;
			Render_thick_line(cvx[i], cvy[i], cvx[j], cvy[j],
				1.0f, 0.4f, 0.3f, 0.3f, 0.7f);
		}
	}

	/* OK button — chamfered NE+SW */
	float ok_x = cancel_x + BUTTON_WIDTH + btn_gap;
	{
		BatchRenderer *batch = Graphics_get_batch();
		float bx = ok_x, by = btn_y;
		float bw = BUTTON_WIDTH, bh = BUTTON_HEIGHT;
		float ch = 8.0f;
		float cvx[6] = { bx,        bx + bw - ch, bx + bw,
		                  bx + bw,   bx + ch,      bx };
		float cvy[6] = { by,        by,            by + ch,
		                  by + bh,   by + bh,       by + bh - ch };
		float cx = bx + bw * 0.5f, cy = by + bh * 0.5f;
		for (int i = 0; i < 6; i++) {
			int j = (i + 1) % 6;
			Batch_push_triangle_vertices(batch,
				cx, cy, cvx[i], cvy[i], cvx[j], cvy[j],
				0.12f, 0.15f, 0.2f, 0.9f);
		}
		for (int i = 0; i < 6; i++) {
			int j = (i + 1) % 6;
			Render_thick_line(cvx[i], cvy[i], cvx[j], cvy[j],
				1.0f, 0.3f, 0.4f, 0.6f, 0.7f);
		}
	}

	/* Flush all geometry so text renders on top */
	Render_flush(&proj, &ident);

	/*
	 * Pass 2: All text
	 */

	/* Title */
	Text_render(tr, shaders, &proj, &ident,
		"SETTINGS", px + pw * 0.5f - 35.0f, py - 5.0f,
		0.7f, 0.7f, 1.0f, 0.9f);

	/* Tab labels */
	tab_y = py + TAB_GAP;
	for (int t = 0; t < TAB_COUNT; t++) {
		bool selected = (t == selectedTab);
		float text_y = tab_y + TAB_HEIGHT * 0.5f + 5.0f;
		Text_render(tr, shaders, &proj, &ident,
			tab_names[t], tab_x + 8.0f, text_y,
			selected ? 1.0f : 0.5f,
			selected ? 1.0f : 0.5f,
			selected ? 1.0f : 0.5f,
			0.9f);

		tab_y += TAB_HEIGHT + TAB_GAP;
	}

	/* Content text */
	if (selectedTab == TAB_GRAPHICS) {
		float row_y = content_y + 30.0f;

		/* Fullscreen label */
		Text_render(tr, shaders, &proj, &ident,
			"Fullscreen", label_x, row_y + 15.0f,
			0.8f, 0.8f, 0.9f, 0.9f);

		/* Fullscreen toggle text */
		Text_render(tr, shaders, &proj, &ident,
			pendingFullscreen ? "ON" : "OFF",
			toggle_x + 12.0f, row_y + 15.0f,
			pendingFullscreen ? 0.3f : 0.6f,
			pendingFullscreen ? 1.0f : 0.6f,
			pendingFullscreen ? 0.3f : 0.6f,
			0.9f);

		/* Multisampling row */
		row_y += 40.0f;
		Text_render(tr, shaders, &proj, &ident,
			"Multisampling", label_x, row_y + 15.0f,
			0.8f, 0.8f, 0.9f, 0.9f);

		Text_render(tr, shaders, &proj, &ident,
			pendingMultisampling ? "ON" : "OFF",
			toggle_x + 12.0f, row_y + 15.0f,
			pendingMultisampling ? 0.3f : 0.6f,
			pendingMultisampling ? 1.0f : 0.6f,
			pendingMultisampling ? 0.3f : 0.6f,
			0.9f);

		/* Antialiasing row */
		row_y += 40.0f;
		Text_render(tr, shaders, &proj, &ident,
			"Antialiasing", label_x, row_y + 15.0f,
			0.8f, 0.8f, 0.9f, 0.9f);

		Text_render(tr, shaders, &proj, &ident,
			pendingAntialiasing ? "ON" : "OFF",
			toggle_x + 12.0f, row_y + 15.0f,
			pendingAntialiasing ? 0.3f : 0.6f,
			pendingAntialiasing ? 1.0f : 0.6f,
			pendingAntialiasing ? 0.3f : 0.6f,
			0.9f);

		/* Pixel Snapping row */
		row_y += 40.0f;
		Text_render(tr, shaders, &proj, &ident,
			"Pixel Snapping", label_x, row_y + 15.0f,
			0.8f, 0.8f, 0.9f, 0.9f);

		Text_render(tr, shaders, &proj, &ident,
			pendingPixelSnapping ? "ON" : "OFF",
			toggle_x + 12.0f, row_y + 15.0f,
			pendingPixelSnapping ? 0.3f : 0.6f,
			pendingPixelSnapping ? 1.0f : 0.6f,
			pendingPixelSnapping ? 0.3f : 0.6f,
			0.9f);

		/* Bloom row */
		row_y += 40.0f;
		Text_render(tr, shaders, &proj, &ident,
			"Bloom", label_x, row_y + 15.0f,
			0.8f, 0.8f, 0.9f, 0.9f);

		Text_render(tr, shaders, &proj, &ident,
			pendingBloom ? "ON" : "OFF",
			toggle_x + 12.0f, row_y + 15.0f,
			pendingBloom ? 0.3f : 0.6f,
			pendingBloom ? 1.0f : 0.6f,
			pendingBloom ? 0.3f : 0.6f,
			0.9f);

		/* Lighting row */
		row_y += 40.0f;
		Text_render(tr, shaders, &proj, &ident,
			"Lighting", label_x, row_y + 15.0f,
			0.8f, 0.8f, 0.9f, 0.9f);

		Text_render(tr, shaders, &proj, &ident,
			pendingLighting ? "ON" : "OFF",
			toggle_x + 12.0f, row_y + 15.0f,
			pendingLighting ? 0.3f : 0.6f,
			pendingLighting ? 1.0f : 0.6f,
			pendingLighting ? 0.3f : 0.6f,
			0.9f);

		/* Reflections row */
		row_y += 40.0f;
		Text_render(tr, shaders, &proj, &ident,
			"Reflections", label_x, row_y + 15.0f,
			0.8f, 0.8f, 0.9f, 0.9f);

		Text_render(tr, shaders, &proj, &ident,
			pendingReflections ? "ON" : "OFF",
			toggle_x + 12.0f, row_y + 15.0f,
			pendingReflections ? 0.3f : 0.6f,
			pendingReflections ? 1.0f : 0.6f,
			pendingReflections ? 0.3f : 0.6f,
			0.9f);

		/* Circuit Traces row */
		row_y += 40.0f;
		Text_render(tr, shaders, &proj, &ident,
			"Circuit Traces", label_x, row_y + 15.0f,
			0.8f, 0.8f, 0.9f, 0.9f);

		Text_render(tr, shaders, &proj, &ident,
			pendingCircuitTraces ? "ON" : "OFF",
			toggle_x + 12.0f, row_y + 15.0f,
			pendingCircuitTraces ? 0.3f : 0.6f,
			pendingCircuitTraces ? 1.0f : 0.6f,
			pendingCircuitTraces ? 0.3f : 0.6f,
			0.9f);

	}

	/* Audio tab text */
	if (selectedTab == TAB_AUDIO) {
		float combo_x = content_x + (content_width - SLIDER_COMBO_W) * 0.5f;
		float slider_x = combo_x + SLIDER_LEFT_PAD;
		float group_start = content_y + 40.0f;
		float group_spacing = 75.0f;
		float knob_offset = 22.0f;
		float combo_center = combo_x + SLIDER_COMBO_W * 0.5f;

		const char *labels[3] = { "Background Music", "Sound Effects", "Voice Recordings" };

		for (int s = 0; s < 3; s++) {
			float gy = group_start + s * group_spacing;
			float knob_y = gy + knob_offset;
			float bar_center_y = knob_y + SLIDER_KNOB_H * 0.5f + 4.0f;

			/* Label centered above combo */
			float lw = Text_measure_width(tr, labels[s]);
			Text_render(tr, shaders, &proj, &ident,
				labels[s], combo_center - lw * 0.5f, gy + 14.0f,
				0.8f, 0.8f, 0.9f, 0.9f);

			/* "0" left of slider */
			Text_render(tr, shaders, &proj, &ident,
				"0", slider_x - 16.0f, bar_center_y,
				0.5f, 0.5f, 0.55f, 0.7f);

			/* "100" right of slider */
			Text_render(tr, shaders, &proj, &ident,
				"100", slider_x + SLIDER_WIDTH + 6.0f, bar_center_y,
				0.5f, 0.5f, 0.55f, 0.7f);

			/* Current value */
			float volumes[3] = { pendingMusicVol, pendingSFXVol, pendingVoiceVol };
			char val_buf[8];
			snprintf(val_buf, sizeof(val_buf), "%d", (int)(volumes[s] * 100.0f + 0.5f));
			Text_render(tr, shaders, &proj, &ident,
				val_buf, slider_x + SLIDER_WIDTH + 42.0f, bar_center_y,
				0.6f, 0.75f, 1.0f, 0.9f);
		}
	}

	/* Button text */
	Text_render(tr, shaders, &proj, &ident,
		"Cancel", cancel_x + 10.0f, btn_y + 20.0f,
		0.8f, 0.6f, 0.6f, 0.9f);

	Text_render(tr, shaders, &proj, &ident,
		"OK", ok_x + 30.0f, btn_y + 20.0f,
		0.6f, 0.8f, 1.0f, 0.9f);

	/* Help text */
	Text_render(tr, shaders, &proj, &ident,
		"[I] Close    [ESC] Cancel",
		px + 10.0f, py + ph + 15.0f,
		0.6f, 0.6f, 0.65f, 0.9f);
}
