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
#include "keybinds.h"

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

/* Keybind pending values */
static BindInput pendingBinds[BIND_COUNT];
static BindInput origBinds[BIND_COUNT];
static int listenAction = -1;  /* -1=not listening, >=0 = action waiting for input */
static bool listenReady = false; /* true once all inputs released after entering listen mode */
static int scrollOffset = 0;
static bool draggingScrollbar = false;
#define KEYBIND_ROW_HEIGHT 30.0f
#define KEYBIND_VISIBLE_ROWS 11
#define KEYBIND_BTN_WIDTH 140.0f
#define KEYBIND_BTN_HEIGHT 24.0f

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
	Keybinds_initialize();
	printf("Settings_load: attempting to open %s\n", SETTINGS_FILE_PATH);
	FILE *f = fopen(SETTINGS_FILE_PATH, "r");
	if (!f) {
		printf("Settings_load: file not found, using defaults\n");
		return;
	}

	char line[256];
	while (fgets(line, sizeof(line), f)) {
		char key[64];
		int v1, v2;
		int tokens = sscanf(line, "%63s %d %d", key, &v1, &v2);
		if (tokens < 2) continue;

		/* Keybind entries have 3 tokens: key device code */
		if (tokens == 3 && strncmp(key, "bind_", 5) == 0) {
			Keybinds_load_entry(key, v1, v2);
			continue;
		}

		int value = v1;
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
	Keybinds_save(f);
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

		/* Keybinds: copy current to pending */
		for (int i = 0; i < BIND_COUNT; i++) {
			pendingBinds[i] = Keybinds_get_binding((BindAction)i);
			origBinds[i] = pendingBinds[i];
		}
		listenAction = -1;
		scrollOffset = 0;
		draggingScrollbar = false;
	}
}

void Settings_update(Input *input, const unsigned int ticks)
{
	(void)ticks;

	if (!settingsOpen)
		return;

	/* Consume mouse wheel after reading for scroll (below) */
	bool wheelUp = input->mouseWheelUp;
	bool wheelDown = input->mouseWheelDown;
	input->mouseWheelUp = false;
	input->mouseWheelDown = false;

	/* ESC: cancel listen mode, or close settings */
	if (input->keyEsc) {
		if (listenAction >= 0) {
			listenAction = -1;
		} else {
			Audio_set_master_music(origMusicVol);
			Audio_set_master_sfx(origSFXVol);
			Audio_set_master_voice(origVoiceVol);
			settingsOpen = false;
			mouseWasDown = false;
		}
		return;
	}

	/* Listen mode: capture next key or mouse button */
	if (listenAction >= 0) {
		/* Wait for all inputs to be released before accepting a new binding.
		 * This prevents the click that activated listen mode from immediately
		 * being captured as a LMB binding. */
		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		Uint32 mb = SDL_GetMouseState(NULL, NULL);

		bool anyKeyDown = false;
		for (int sc = 0; sc < SDL_NUM_SCANCODES; sc++) {
			if (keys[sc] && SDL_GetKeyFromScancode((SDL_Scancode)sc) != SDLK_ESCAPE) {
				anyKeyDown = true;
				break;
			}
		}
		bool anyMouseDown = (mb != 0);

		if (!listenReady) {
			/* Wait until everything is released first */
			if (!anyKeyDown && !anyMouseDown)
				listenReady = true;
			mouseWasDown = input->mouseLeft;
			return;
		}

		/* Check keyboard */
		if (anyKeyDown) {
			for (int sc = 0; sc < SDL_NUM_SCANCODES; sc++) {
				if (keys[sc]) {
					SDL_Keycode kc = SDL_GetKeyFromScancode((SDL_Scancode)sc);
					if (kc == SDLK_ESCAPE) continue;
					BindInput newBind = BindInput_key(kc);
					for (int j = 0; j < BIND_COUNT; j++) {
						if (j != listenAction && BindInput_equals(pendingBinds[j], newBind))
							pendingBinds[j] = BindInput_none();
					}
					pendingBinds[listenAction] = newBind;
					listenAction = -1;
					mouseWasDown = true;
					return;
				}
			}
		}
		/* Check mouse buttons */
		if (anyMouseDown) {
			for (int btn = SDL_BUTTON_LEFT; btn <= SDL_BUTTON_X2; btn++) {
				if (mb & SDL_BUTTON(btn)) {
					BindInput newBind = BindInput_mouse(btn);
					for (int j = 0; j < BIND_COUNT; j++) {
						if (j != listenAction && BindInput_equals(pendingBinds[j], newBind))
							pendingBinds[j] = BindInput_none();
					}
					pendingBinds[listenAction] = newBind;
					listenAction = -1;
					mouseWasDown = true;
					return;
				}
			}
		}
		/* Still listening — consume all input */
		mouseWasDown = input->mouseLeft;
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

	/* Keybinds tab interaction */
	if (selectedTab == TAB_KEYBINDS) {
		/* Scroll with mouse wheel */
		int maxScroll = BIND_COUNT - KEYBIND_VISIBLE_ROWS;
		if (maxScroll < 0) maxScroll = 0;
		if (wheelUp && scrollOffset > 0)
			scrollOffset--;
		if (wheelDown && scrollOffset < maxScroll)
			scrollOffset++;

		/* Scrollbar drag */
		float list_x = content_x + 10.0f;
		float list_y = content_y + 10.0f;
		float sb_w = 8.0f;
		float sb_x = list_x + content_width - 20.0f;
		float track_y = list_y;
		float track_h = KEYBIND_VISIBLE_ROWS * KEYBIND_ROW_HEIGHT;

		if (clicked && maxScroll > 0 &&
			mx >= sb_x && mx <= sb_x + sb_w &&
			my >= track_y && my <= track_y + track_h) {
			draggingScrollbar = true;
		}

		if (draggingScrollbar && input->mouseLeft && maxScroll > 0) {
			float ratio = (my - track_y) / track_h;
			if (ratio < 0.0f) ratio = 0.0f;
			if (ratio > 1.0f) ratio = 1.0f;
			scrollOffset = (int)(ratio * maxScroll + 0.5f);
		}

		if (draggingScrollbar && !input->mouseLeft)
			draggingScrollbar = false;

		/* Click on bind buttons or reset button */
		if (clicked && !draggingScrollbar) {
			float list_x = content_x + 10.0f;
			float list_y = content_y + 10.0f;
			float btn_x = list_x + content_width - 30.0f - KEYBIND_BTN_WIDTH;

			for (int i = 0; i < KEYBIND_VISIBLE_ROWS && (i + scrollOffset) < BIND_COUNT; i++) {
				float row_y = list_y + i * KEYBIND_ROW_HEIGHT;
				if (mx >= btn_x && mx <= btn_x + KEYBIND_BTN_WIDTH &&
					my >= row_y && my <= row_y + KEYBIND_BTN_HEIGHT) {
					listenAction = i + scrollOffset;
					listenReady = false;
					break;
				}
			}

			/* Reset Defaults button */
			float reset_w = 160.0f;
			float reset_x = content_x + (content_width - reset_w) * 0.5f;
			float reset_y = list_y + KEYBIND_VISIBLE_ROWS * KEYBIND_ROW_HEIGHT + 8.0f;
			if (mx >= reset_x && mx <= reset_x + reset_w &&
				my >= reset_y && my <= reset_y + KEYBIND_BTN_HEIGHT) {
				/* Reset all pending to defaults */
				Keybinds_reset_defaults();
				for (int i = 0; i < BIND_COUNT; i++)
					pendingBinds[i] = Keybinds_get_binding((BindAction)i);
				/* Restore originals so we can re-read them on OK */
				for (int i = 0; i < BIND_COUNT; i++)
					Keybinds_set_binding((BindAction)i, origBinds[i]);
				listenAction = -1;
			}
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
			/* Apply keybinds */
			for (int i = 0; i < BIND_COUNT; i++)
				Keybinds_set_binding((BindAction)i, pendingBinds[i]);
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

	/* Keybinds tab geometry */
	if (selectedTab == TAB_KEYBINDS) {
		float list_x = content_x + 10.0f;
		float list_y = content_y + 10.0f;
		float btn_x = list_x + content_width - 30.0f - KEYBIND_BTN_WIDTH;

		for (int i = 0; i < KEYBIND_VISIBLE_ROWS && (i + scrollOffset) < BIND_COUNT; i++) {
			float row_y = list_y + i * KEYBIND_ROW_HEIGHT;
			int action = i + scrollOffset;

			/* Alternating row background */
			if (i % 2 == 0) {
				Render_quad_absolute(list_x, row_y,
					list_x + content_width - 20.0f, row_y + KEYBIND_ROW_HEIGHT,
					0.1f, 0.1f, 0.15f, 0.3f);
			}

			/* Bind button background */
			float br, bg, bb, ba;
			if (action == listenAction) {
				br = 0.2f; bg = 0.25f; bb = 0.4f; ba = 0.9f;
			} else {
				br = 0.15f; bg = 0.15f; bb = 0.2f; ba = 0.8f;
			}
			Render_quad_absolute(btn_x, row_y + 3.0f,
				btn_x + KEYBIND_BTN_WIDTH, row_y + 3.0f + KEYBIND_BTN_HEIGHT,
				br, bg, bb, ba);

			/* Bind button border */
			float bdr, bdg, bdb, bda;
			if (action == listenAction) {
				bdr = 0.4f; bdg = 0.5f; bdb = 1.0f; bda = 0.9f;
			} else {
				bdr = 0.3f; bdg = 0.3f; bdb = 0.4f; bda = 0.6f;
			}
			float bx0 = btn_x, by0 = row_y + 3.0f;
			float bx1 = btn_x + KEYBIND_BTN_WIDTH, by1 = row_y + 3.0f + KEYBIND_BTN_HEIGHT;
			Render_thick_line(bx0, by0, bx1, by0, 1.0f, bdr, bdg, bdb, bda);
			Render_thick_line(bx0, by1, bx1, by1, 1.0f, bdr, bdg, bdb, bda);
			Render_thick_line(bx0, by0, bx0, by1, 1.0f, bdr, bdg, bdb, bda);
			Render_thick_line(bx1, by0, bx1, by1, 1.0f, bdr, bdg, bdb, bda);
		}

		/* Reset Defaults button — chamfered NE+SW, centered */
		float reset_w = 160.0f;
		float reset_x = content_x + (content_width - reset_w) * 0.5f;
		float reset_y = list_y + KEYBIND_VISIBLE_ROWS * KEYBIND_ROW_HEIGHT + 8.0f;
		float reset_h = KEYBIND_BTN_HEIGHT;
		{
			BatchRenderer *batch = Graphics_get_batch();
			float ch = 6.0f;
			float rvx[6] = { reset_x,            reset_x + reset_w - ch, reset_x + reset_w,
			                  reset_x + reset_w,  reset_x + ch,           reset_x };
			float rvy[6] = { reset_y,             reset_y,                reset_y + ch,
			                  reset_y + reset_h,   reset_y + reset_h,      reset_y + reset_h - ch };
			float rcx = reset_x + reset_w * 0.5f, rcy = reset_y + reset_h * 0.5f;
			for (int i = 0; i < 6; i++) {
				int j = (i + 1) % 6;
				Batch_push_triangle_vertices(batch,
					rcx, rcy, rvx[i], rvy[i], rvx[j], rvy[j],
					0.15f, 0.12f, 0.12f, 0.9f);
			}
			for (int i = 0; i < 6; i++) {
				int j = (i + 1) % 6;
				Render_thick_line(rvx[i], rvy[i], rvx[j], rvy[j],
					1.0f, 0.4f, 0.3f, 0.3f, 0.6f);
			}
		}

		/* Scrollbar */
		int maxScroll = BIND_COUNT - KEYBIND_VISIBLE_ROWS;
		if (maxScroll < 0) maxScroll = 0;
		if (maxScroll > 0) {
			float sb_w = 8.0f;
			float sb_x = list_x + content_width - 20.0f;
			float track_y = list_y;
			float track_h = KEYBIND_VISIBLE_ROWS * KEYBIND_ROW_HEIGHT;
			float thumb_h = (track_h * KEYBIND_VISIBLE_ROWS) / (float)BIND_COUNT;
			if (thumb_h < 20.0f) thumb_h = 20.0f;
			float scroll_ratio = (float)scrollOffset / (float)maxScroll;
			float thumb_y = track_y + scroll_ratio * (track_h - thumb_h);

			/* Track */
			Render_quad_absolute(sb_x, track_y, sb_x + sb_w, track_y + track_h,
				0.2f, 0.2f, 0.2f, 0.3f);
			/* Thumb */
			Render_quad_absolute(sb_x, thumb_y, sb_x + sb_w, thumb_y + thumb_h,
				0.7f, 0.7f, 0.7f, 0.4f);
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

	/* Keybinds tab text */
	if (selectedTab == TAB_KEYBINDS) {
		float list_x = content_x + 10.0f;
		float list_y = content_y + 10.0f;
		float btn_x = list_x + content_width - 30.0f - KEYBIND_BTN_WIDTH;

		for (int i = 0; i < KEYBIND_VISIBLE_ROWS && (i + scrollOffset) < BIND_COUNT; i++) {
			float row_y = list_y + i * KEYBIND_ROW_HEIGHT;
			int action = i + scrollOffset;

			/* Action name */
			Text_render(tr, shaders, &proj, &ident,
				Keybinds_get_action_name((BindAction)action),
				list_x + 8.0f, row_y + 18.0f,
				0.8f, 0.8f, 0.9f, 0.9f);

			/* Binding name or "Press key..." */
			if (action == listenAction) {
				Text_render(tr, shaders, &proj, &ident,
					"Press key...",
					btn_x + 8.0f, row_y + 18.0f,
					0.4f, 0.6f, 1.0f, 0.9f);
			} else {
				const char *bindName = Keybinds_input_name(pendingBinds[action]);
				Text_render(tr, shaders, &proj, &ident,
					bindName,
					btn_x + 8.0f, row_y + 18.0f,
					0.6f, 0.7f, 0.9f, 0.9f);
			}
		}

		/* Reset Defaults button text */
		float reset_w = 160.0f;
		float reset_x = content_x + (content_width - reset_w) * 0.5f;
		float reset_y = list_y + KEYBIND_VISIBLE_ROWS * KEYBIND_ROW_HEIGHT + 8.0f;
		float rtext_w = Text_measure_width(tr, "Reset Defaults");
		Text_render(tr, shaders, &proj, &ident,
			"Reset Defaults", reset_x + (reset_w - rtext_w) * 0.5f, reset_y + 16.0f,
			0.8f, 0.6f, 0.6f, 0.9f);
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
