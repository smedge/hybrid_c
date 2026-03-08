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

/* Graphics tab scrolling */
#define GFX_SCROLLBAR_W 8.0f
#define GFX_SCALE_EXTRA_GAP 35.0f  /* breathing room before UI Scale section */
#define GFX_SCALE_LABEL_H 25.0f    /* height of label line above slider */
#define GFX_SCALE_PCT_W 40.0f      /* width reserved for percentage label */
#define GFX_SCALE_PCT_GAP 6.0f     /* gap between slider and percentage */

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
static int draggingSlider = -1;  /* -1=none, 0=music, 1=sfx, 2=voice, 3=ui_scale */

/* UI Scale pending values */
static float pendingUiScale = 1.0f;
static float origUiScale = 1.0f;

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

/* Graphics tab scroll state */
static float gfxScroll = 0.0f;
static bool gfxDraggingScrollbar = false;

/* Mouse tracking */
static bool mouseWasDown = false;

#define SETTINGS_FILE_PATH "./settings.cfg"

/* Helper: compute panel rect (centered on screen, same as catalog) */
static void get_panel_rect(const Screen *screen,
	float *px, float *py, float *pw, float *ph)
{
	float s = Graphics_get_ui_scale();
	*pw = SETTINGS_WIDTH * s;
	*ph = SETTINGS_HEIGHT * s;
	*px = ((float)screen->width - *pw) * 0.5f;
	*py = ((float)screen->height - *ph) * 0.5f - 30.0f * s;
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
		else if (strcmp(key, "ui_scale") == 0)
			Graphics_set_ui_scale(value / 100.0f);
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
	fprintf(f, "ui_scale %d\n", (int)(Graphics_get_ui_scale() * 100.0f + 0.5f));
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
		pendingUiScale = Graphics_get_ui_scale();
		origUiScale = pendingUiScale;

		/* Keybinds: copy current to pending */
		for (int i = 0; i < BIND_COUNT; i++) {
			pendingBinds[i] = Keybinds_get_binding((BindAction)i);
			origBinds[i] = pendingBinds[i];
		}
		listenAction = -1;
		scrollOffset = 0;
		draggingScrollbar = false;
		gfxScroll = 0.0f;
		gfxDraggingScrollbar = false;
	}
}

void Settings_update(Input *input, const unsigned int ticks)
{
	(void)ticks;

	if (!settingsOpen)
		return;

	float s = Graphics_get_ui_scale();
	float tab_w = TAB_WIDTH * s;
	float tab_h = TAB_HEIGHT * s;
	float tab_gap = TAB_GAP * s;
	float toggle_w = TOGGLE_WIDTH * s;
	float toggle_h = TOGGLE_HEIGHT * s;
	float button_w = BUTTON_WIDTH * s;
	float button_h = BUTTON_HEIGHT * s;
	float button_gap = BUTTON_GAP * s;
	float slider_w = SLIDER_WIDTH * s;
	float slider_knob_h = SLIDER_KNOB_H * s;
	float slider_left_pad = SLIDER_LEFT_PAD * s;
	float slider_combo_w = SLIDER_COMBO_W * s;
	float keybind_row_h = KEYBIND_ROW_HEIGHT * s;
	float keybind_btn_w = KEYBIND_BTN_WIDTH * s;
	float keybind_btn_h = KEYBIND_BTN_HEIGHT * s;

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
			Graphics_set_ui_scale(origUiScale);
			Graphics_rebuild_fonts();
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
		/* Still listening -- consume all input */
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
		float tab_x = px + tab_gap;
		float tab_y = py + tab_gap;
		for (int t = 0; t < TAB_COUNT; t++) {
			if (mx >= tab_x && mx <= tab_x + tab_w &&
				my >= tab_y && my <= tab_y + tab_h) {
				selectedTab = t;
				break;
			}
			tab_y += tab_h + tab_gap;
		}
	}

	/* Content area */
	float content_x = px + tab_w + tab_gap * 2.0f;
	float content_y = py + tab_gap;
	float content_width = pw - tab_w - tab_gap * 2.0f;
	float row_width = 190.0f * s + toggle_w;
	float row_offset = (content_width - row_width) * 0.5f;
	float toggle_x = content_x + row_offset + 190.0f * s;

	/* Graphics tab scroll + toggle clicks */
	if (selectedTab == TAB_GRAPHICS) {
		float gfx_sb_w = GFX_SCROLLBAR_W * s;
		float btn_y_bottom = py + ph - button_h - button_gap;
		float gfx_visible_h = btn_y_bottom - 10.0f * s - content_y;
		float gfx_total_h = 30.0f * s + 8.0f * 40.0f * s
			+ GFX_SCALE_EXTRA_GAP * s + GFX_SCALE_LABEL_H * s
			+ slider_knob_h + 15.0f * s;
		float gfx_max_scroll = gfx_total_h - gfx_visible_h;
		if (gfx_max_scroll < 0.0f) gfx_max_scroll = 0.0f;

		/* Mouse wheel */
		if (wheelUp) { gfxScroll -= 30.0f * s; wheelUp = false; }
		if (wheelDown) { gfxScroll += 30.0f * s; wheelDown = false; }
		if (gfxScroll < 0.0f) gfxScroll = 0.0f;
		if (gfxScroll > gfx_max_scroll) gfxScroll = gfx_max_scroll;

		/* Scrollbar drag */
		float sb_x = px + pw - gfx_sb_w - tab_gap;
		float track_y = content_y;
		float track_h = gfx_visible_h;
		if (clicked && gfx_max_scroll > 0.0f &&
			mx >= sb_x && mx <= sb_x + gfx_sb_w &&
			my >= track_y && my <= track_y + track_h) {
			gfxDraggingScrollbar = true;
		}
		if (gfxDraggingScrollbar && input->mouseLeft && gfx_max_scroll > 0.0f) {
			float thumb_h = (gfx_visible_h / gfx_total_h) * track_h;
			if (thumb_h < 20.0f * s) thumb_h = 20.0f * s;
			float ratio = (my - track_y - thumb_h * 0.5f) / (track_h - thumb_h);
			if (ratio < 0.0f) ratio = 0.0f;
			if (ratio > 1.0f) ratio = 1.0f;
			gfxScroll = ratio * gfx_max_scroll;
		}
		if (gfxDraggingScrollbar && !input->mouseLeft)
			gfxDraggingScrollbar = false;

		if (clicked) {
			/* Toggle hit tests (scrolled) */
			bool *toggles[8] = {
				&pendingFullscreen, &pendingMultisampling,
				&pendingAntialiasing, &pendingPixelSnapping,
				&pendingBloom, &pendingLighting,
				&pendingReflections, &pendingCircuitTraces
			};
			for (int i = 0; i < 8; i++) {
				float row_y = content_y + 30.0f * s + i * 40.0f * s - gfxScroll;
				if (mx >= toggle_x && mx <= toggle_x + toggle_w &&
					my >= row_y && my <= row_y + toggle_h &&
					my >= content_y && my <= content_y + gfx_visible_h) {
					*toggles[i] = !*toggles[i];
				}
			}

			/* UI Scale slider hit test (scrolled) */
			float scale_row_y = content_y + 30.0f * s + 8.0f * 40.0f * s
				+ GFX_SCALE_EXTRA_GAP * s + GFX_SCALE_LABEL_H * s - gfxScroll;
			float group_w = slider_w + GFX_SCALE_PCT_GAP * s + GFX_SCALE_PCT_W * s;
			float scale_slider_x = content_x + (content_width - gfx_sb_w - group_w) * 0.5f;
			if (mx >= scale_slider_x && mx <= scale_slider_x + slider_w &&
				my >= scale_row_y && my <= scale_row_y + slider_knob_h &&
				my >= content_y && my <= content_y + gfx_visible_h) {
				draggingSlider = 3;
				float val = (mx - scale_slider_x) / slider_w;
				if (val < 0.0f) val = 0.0f;
				if (val > 1.0f) val = 1.0f;
				float mapped = 0.5f + val * 2.0f;
				mapped = ((int)(mapped / 0.25f + 0.5f)) * 0.25f;
				if (mapped < 0.5f) mapped = 0.5f;
				if (mapped > 2.5f) mapped = 2.5f;
				pendingUiScale = mapped;
			}
		}
	}

	/* Audio tab slider interaction */
	if (selectedTab == TAB_AUDIO) {
		float combo_x = content_x + (content_width - slider_combo_w) * 0.5f;
		float slider_x = combo_x + slider_left_pad;
		float group_start = content_y + 40.0f * s;
		float group_spacing = 75.0f * s;
		float knob_offset = 22.0f * s;  /* label height + gap before slider */

		float *volumes[3] = { &pendingMusicVol, &pendingSFXVol, &pendingVoiceVol };
		void (*setters[3])(float) = { Audio_set_master_music, Audio_set_master_sfx, Audio_set_master_voice };

		/* Mouse down on a slider track -> begin drag */
		if (clicked) {
			for (int si = 0; si < 3; si++) {
				float knob_y = group_start + si * group_spacing + knob_offset;
				if (mx >= slider_x && mx <= slider_x + slider_w &&
					my >= knob_y && my <= knob_y + slider_knob_h) {
					draggingSlider = si;
					float val = (mx - slider_x) / slider_w;
					if (val < 0.0f) val = 0.0f;
					if (val > 1.0f) val = 1.0f;
					*volumes[si] = val;
					setters[si](val);
					break;
				}
			}
		}

		/* Continue drag while mouse held */
		if (draggingSlider >= 0 && draggingSlider < 3 && input->mouseLeft) {
			float val = (mx - slider_x) / slider_w;
			if (val < 0.0f) val = 0.0f;
			if (val > 1.0f) val = 1.0f;
			*volumes[draggingSlider] = val;
			setters[draggingSlider](val);
		}

		/* Release drag */
		if (draggingSlider >= 0 && draggingSlider < 3 && !input->mouseLeft) {
			draggingSlider = -1;
		}
	}

	/* UI Scale slider drag — update pending value only, apply on release */
	if (draggingSlider == 3 && selectedTab == TAB_GRAPHICS) {
		float gfx_sb_w = GFX_SCROLLBAR_W * s;
		float group_w = slider_w + GFX_SCALE_PCT_GAP * s + GFX_SCALE_PCT_W * s;
		float scale_slider_x = content_x + (content_width - gfx_sb_w - group_w) * 0.5f;
		if (input->mouseLeft) {
			float val = (mx - scale_slider_x) / slider_w;
			if (val < 0.0f) val = 0.0f;
			if (val > 1.0f) val = 1.0f;
			float mapped = 0.5f + val * 2.0f;
			mapped = ((int)(mapped / 0.25f + 0.5f)) * 0.25f;
			if (mapped < 0.5f) mapped = 0.5f;
			if (mapped > 2.5f) mapped = 2.5f;
			pendingUiScale = mapped;
		} else {
			/* Apply on release */
			Graphics_set_ui_scale(pendingUiScale);
			Graphics_rebuild_fonts();
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
		float list_x = content_x + 10.0f * s;
		float list_y = content_y + 10.0f * s;
		float sb_w = 8.0f * s;
		float sb_x = list_x + content_width - 20.0f * s;
		float track_y = list_y;
		float track_h = KEYBIND_VISIBLE_ROWS * keybind_row_h;

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
			float list_x2 = content_x + 10.0f * s;
			float list_y2 = content_y + 10.0f * s;
			float btn_x = list_x2 + content_width - 30.0f * s - keybind_btn_w;

			for (int i = 0; i < KEYBIND_VISIBLE_ROWS && (i + scrollOffset) < BIND_COUNT; i++) {
				float row_y = list_y2 + i * keybind_row_h;
				if (mx >= btn_x && mx <= btn_x + keybind_btn_w &&
					my >= row_y && my <= row_y + keybind_btn_h) {
					listenAction = i + scrollOffset;
					listenReady = false;
					break;
				}
			}

			/* Reset Defaults button */
			float reset_w = 180.0f * s;
			float reset_x = content_x + (content_width - reset_w) * 0.5f;
			float reset_y = list_y2 + KEYBIND_VISIBLE_ROWS * keybind_row_h + 8.0f * s;
			if (mx >= reset_x && mx <= reset_x + reset_w &&
				my >= reset_y && my <= reset_y + button_h) {
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
		float btn_y = py + ph - button_h - button_gap;
		float btn_gap_inner = 40.0f * s;
		float btn_total = button_w * 2.0f + btn_gap_inner;
		float btn_off = (content_width - btn_total) * 0.5f;

		/* Cancel button (left side) */
		float cancel_x = content_x + btn_off;
		if (mx >= cancel_x && mx <= cancel_x + button_w &&
			my >= btn_y && my <= btn_y + button_h) {
			/* Discard pending, restore audio to pre-open values */
			Audio_set_master_music(origMusicVol);
			Audio_set_master_sfx(origSFXVol);
			Audio_set_master_voice(origVoiceVol);
			Graphics_set_ui_scale(origUiScale);
			Graphics_rebuild_fonts();
			settingsOpen = false;
			mouseWasDown = false;
			return;
		}

		/* OK button (right side) */
		float ok_x = cancel_x + button_w + btn_gap_inner;
		if (mx >= ok_x && mx <= ok_x + button_w &&
			my >= btn_y && my <= btn_y + button_h) {
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
			Graphics_set_ui_scale(pendingUiScale);
			Graphics_rebuild_fonts();
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

	float s = Graphics_get_ui_scale();
	float tab_w = TAB_WIDTH * s;
	float tab_h = TAB_HEIGHT * s;
	float tab_gap = TAB_GAP * s;
	float tab_chamf = TAB_CHAMF * s;
	float button_w = BUTTON_WIDTH * s;
	float button_h = BUTTON_HEIGHT * s;
	float button_gap = BUTTON_GAP * s;
	float toggle_w = TOGGLE_WIDTH * s;
	float toggle_h = TOGGLE_HEIGHT * s;
	float slider_w = SLIDER_WIDTH * s;
	float slider_h = SLIDER_HEIGHT * s;
	float slider_knob_w = SLIDER_KNOB_W * s;
	float slider_knob_h = SLIDER_KNOB_H * s;
	float slider_left_pad = SLIDER_LEFT_PAD * s;
	float slider_combo_w = SLIDER_COMBO_W * s;
	float keybind_row_h = KEYBIND_ROW_HEIGHT * s;
	float keybind_btn_w = KEYBIND_BTN_WIDTH * s;
	float keybind_btn_h = KEYBIND_BTN_HEIGHT * s;

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
	Render_thick_line(px, py, px + pw, py, 1.0f * s, brc, brc, brc, 0.8f);
	Render_thick_line(px, py + ph, px + pw, py + ph, 1.0f * s, brc, brc, brc, 0.8f);
	Render_thick_line(px, py, px, py + ph, 1.0f * s, brc, brc, brc, 0.8f);
	Render_thick_line(px + pw, py, px + pw, py + ph, 1.0f * s, brc, brc, brc, 0.8f);

	/* Tab backgrounds */
	float tab_x = px + tab_gap;
	float tab_y = py + tab_gap;
	for (int t = 0; t < TAB_COUNT; t++) {
		bool selected = (t == selectedTab);
		float tr_col = selected ? 0.15f : 0.1f;
		float tg_col = selected ? 0.15f : 0.1f;
		float tb_col = selected ? 0.25f : 0.15f;

		if (selected) {
			/* Chamfered fill: sharp NW + SE, chamfered NE + SW */
			float c = tab_chamf;
			float tx0 = tab_x, ty0 = tab_y;
			float tx1 = tab_x + tab_w, ty1 = tab_y + tab_h;
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
					1.0f * s, tc, tc, 1.0f, 0.8f);
			}
		} else {
			Render_quad_absolute(tab_x, tab_y,
				tab_x + tab_w, tab_y + tab_h,
				tr_col, tg_col, tb_col, 0.9f);
		}

		tab_y += tab_h + tab_gap;
	}

	/* Separator line */
	float sep_x = px + tab_w + tab_gap * 1.5f;
	Render_thick_line(sep_x, py + tab_gap, sep_x, py + ph - tab_gap,
		1.0f * s, 0.25f, 0.25f, 0.3f, 0.6f);

	/* Content area */
	float content_x = px + tab_w + tab_gap * 2.0f;
	float content_y = py + tab_gap;
	float content_width = pw - tab_w - tab_gap * 2.0f;
	float row_width = 190.0f * s + toggle_w;
	float row_offset = (content_width - row_width) * 0.5f;
	float label_x = content_x + row_offset;
	float toggle_x = label_x + 190.0f * s;

	if (selectedTab == TAB_GRAPHICS) {
		float gfx_sb_w = GFX_SCROLLBAR_W * s;
		float btn_y_vis = py + ph - button_h - button_gap;
		float gfx_visible_h = btn_y_vis - 10.0f * s - content_y;
		float gfx_total_h = 30.0f * s + 8.0f * 40.0f * s
			+ GFX_SCALE_EXTRA_GAP * s + GFX_SCALE_LABEL_H * s
			+ slider_knob_h + 15.0f * s;
		float gfx_max_scroll = gfx_total_h - gfx_visible_h;
		if (gfx_max_scroll < 0.0f) gfx_max_scroll = 0.0f;

		/* Scrollbar (outside scissor) */
		if (gfx_max_scroll > 0.0f) {
			float sb_x = px + pw - gfx_sb_w - tab_gap;
			float trk_y = content_y;
			float trk_h = gfx_visible_h;
			float thumb_h = (gfx_visible_h / gfx_total_h) * trk_h;
			if (thumb_h < 20.0f * s) thumb_h = 20.0f * s;
			float scroll_ratio = gfxScroll / gfx_max_scroll;
			float thumb_y = trk_y + scroll_ratio * (trk_h - thumb_h);

			Render_quad_absolute(sb_x, trk_y, sb_x + gfx_sb_w, trk_y + trk_h,
				0.2f, 0.2f, 0.2f, 0.3f);
			Render_quad_absolute(sb_x, thumb_y, sb_x + gfx_sb_w, thumb_y + thumb_h,
				0.7f, 0.7f, 0.7f, 0.4f);
		}

		/* Flush scrollbar, then scissor for content */
		Render_flush(&proj, &ident);
		int draw_w, draw_h;
		Graphics_get_drawable_size(&draw_w, &draw_h);
		float dpi_x = (float)draw_w / screen->width;
		float dpi_y = (float)draw_h / screen->height;
		Render_scissor_begin(
			(int)(content_x * dpi_x),
			draw_h - (int)((content_y + gfx_visible_h) * dpi_y),
			(int)((content_width - gfx_sb_w - tab_gap) * dpi_x),
			(int)(gfx_visible_h * dpi_y));

		/* Toggle rows (scrolled) */
		bool toggle_vals[8] = {
			pendingFullscreen, pendingMultisampling,
			pendingAntialiasing, pendingPixelSnapping,
			pendingBloom, pendingLighting,
			pendingReflections, pendingCircuitTraces
		};
		for (int i = 0; i < 8; i++) {
			float row_y = content_y + 30.0f * s + i * 40.0f * s - gfxScroll;
			if (toggle_vals[i]) {
				Render_quad_absolute(toggle_x, row_y,
					toggle_x + toggle_w, row_y + toggle_h,
					0.1f, 0.4f, 0.1f, 0.9f);
			} else {
				Render_quad_absolute(toggle_x, row_y,
					toggle_x + toggle_w, row_y + toggle_h,
					0.2f, 0.2f, 0.2f, 0.9f);
			}
			Render_thick_line(toggle_x, row_y,
				toggle_x + toggle_w, row_y, 1.0f * s, 0.4f, 0.4f, 0.4f, 0.6f);
			Render_thick_line(toggle_x, row_y + toggle_h,
				toggle_x + toggle_w, row_y + toggle_h, 1.0f * s, 0.4f, 0.4f, 0.4f, 0.6f);
			Render_thick_line(toggle_x, row_y,
				toggle_x, row_y + toggle_h, 1.0f * s, 0.4f, 0.4f, 0.4f, 0.6f);
			Render_thick_line(toggle_x + toggle_w, row_y,
				toggle_x + toggle_w, row_y + toggle_h, 1.0f * s, 0.4f, 0.4f, 0.4f, 0.6f);
		}

		/* UI Scale slider (scrolled, centered as group with percentage) */
		{
			float scale_row_y = content_y + 30.0f * s + 8.0f * 40.0f * s
				+ GFX_SCALE_EXTRA_GAP * s + GFX_SCALE_LABEL_H * s - gfxScroll;
			float group_w = slider_w + GFX_SCALE_PCT_GAP * s + GFX_SCALE_PCT_W * s;
			float scale_slider_x = content_x + (content_width - gfx_sb_w - group_w) * 0.5f;
			float scale_val = (pendingUiScale - 0.5f) / 2.0f;
			if (scale_val < 0.0f) scale_val = 0.0f;
			if (scale_val > 1.0f) scale_val = 1.0f;

			float track_y = scale_row_y + (slider_knob_h - slider_h) * 0.5f;
			float knob_x = scale_slider_x + scale_val * slider_w - slider_knob_w * 0.5f;
			float fill_end = scale_slider_x + scale_val * slider_w;

			Render_quad_absolute(scale_slider_x, track_y,
				scale_slider_x + slider_w, track_y + slider_h,
				0.15f, 0.15f, 0.2f, 0.9f);
			if (fill_end > scale_slider_x) {
				Render_quad_absolute(scale_slider_x, track_y,
					fill_end, track_y + slider_h,
					0.3f, 0.5f, 0.8f, 0.9f);
			}
			Render_quad_absolute(knob_x, scale_row_y,
				knob_x + slider_knob_w, scale_row_y + slider_knob_h,
				0.8f, 0.85f, 0.95f, 0.95f);
			Render_thick_line(knob_x, scale_row_y,
				knob_x + slider_knob_w, scale_row_y, 1.0f * s, 0.5f, 0.5f, 0.6f, 0.8f);
			Render_thick_line(knob_x, scale_row_y + slider_knob_h,
				knob_x + slider_knob_w, scale_row_y + slider_knob_h, 1.0f * s, 0.5f, 0.5f, 0.6f, 0.8f);
			Render_thick_line(knob_x, scale_row_y,
				knob_x, scale_row_y + slider_knob_h, 1.0f * s, 0.5f, 0.5f, 0.6f, 0.8f);
			Render_thick_line(knob_x + slider_knob_w, scale_row_y,
				knob_x + slider_knob_w, scale_row_y + slider_knob_h, 1.0f * s, 0.5f, 0.5f, 0.6f, 0.8f);
		}

		Render_flush(&proj, &ident);
		Render_scissor_end();
	}

	/* Audio tab slider geometry */
	if (selectedTab == TAB_AUDIO) {
		float combo_x = content_x + (content_width - slider_combo_w) * 0.5f;
		float slider_x = combo_x + slider_left_pad;
		float group_start = content_y + 40.0f * s;
		float group_spacing = 75.0f * s;
		float knob_offset = 22.0f * s;

		float volumes[3] = { pendingMusicVol, pendingSFXVol, pendingVoiceVol };

		for (int si = 0; si < 3; si++) {
			float knob_y = group_start + si * group_spacing + knob_offset;
			float track_y = knob_y + (slider_knob_h - slider_h) * 0.5f;
			float knob_x = slider_x + volumes[si] * slider_w - slider_knob_w * 0.5f;
			float fill_end = slider_x + volumes[si] * slider_w;

			/* Track background */
			Render_quad_absolute(slider_x, track_y,
				slider_x + slider_w, track_y + slider_h,
				0.15f, 0.15f, 0.2f, 0.9f);

			/* Fill */
			if (fill_end > slider_x) {
				Render_quad_absolute(slider_x, track_y,
					fill_end, track_y + slider_h,
					0.3f, 0.5f, 0.8f, 0.9f);
			}

			/* Knob */
			Render_quad_absolute(knob_x, knob_y,
				knob_x + slider_knob_w, knob_y + slider_knob_h,
				0.8f, 0.85f, 0.95f, 0.95f);
			/* Knob border */
			Render_thick_line(knob_x, knob_y,
				knob_x + slider_knob_w, knob_y, 1.0f * s, 0.5f, 0.5f, 0.6f, 0.8f);
			Render_thick_line(knob_x, knob_y + slider_knob_h,
				knob_x + slider_knob_w, knob_y + slider_knob_h, 1.0f * s, 0.5f, 0.5f, 0.6f, 0.8f);
			Render_thick_line(knob_x, knob_y,
				knob_x, knob_y + slider_knob_h, 1.0f * s, 0.5f, 0.5f, 0.6f, 0.8f);
			Render_thick_line(knob_x + slider_knob_w, knob_y,
				knob_x + slider_knob_w, knob_y + slider_knob_h, 1.0f * s, 0.5f, 0.5f, 0.6f, 0.8f);
		}
	}

	/* Keybinds tab geometry */
	if (selectedTab == TAB_KEYBINDS) {
		float list_x = content_x + 10.0f * s;
		float list_y = content_y + 10.0f * s;
		float btn_x = list_x + content_width - 30.0f * s - keybind_btn_w;

		for (int i = 0; i < KEYBIND_VISIBLE_ROWS && (i + scrollOffset) < BIND_COUNT; i++) {
			float row_y = list_y + i * keybind_row_h;
			int action = i + scrollOffset;

			/* Alternating row background */
			if (i % 2 == 0) {
				Render_quad_absolute(list_x, row_y,
					list_x + content_width - 20.0f * s, row_y + keybind_row_h,
					0.1f, 0.1f, 0.15f, 0.3f);
			}

			/* Bind button background */
			float br, bg, bb, ba;
			if (action == listenAction) {
				br = 0.2f; bg = 0.25f; bb = 0.4f; ba = 0.9f;
			} else {
				br = 0.15f; bg = 0.15f; bb = 0.2f; ba = 0.8f;
			}
			Render_quad_absolute(btn_x, row_y + 3.0f * s,
				btn_x + keybind_btn_w, row_y + 3.0f * s + keybind_btn_h,
				br, bg, bb, ba);

			/* Bind button border */
			float bdr, bdg, bdb, bda;
			if (action == listenAction) {
				bdr = 0.4f; bdg = 0.5f; bdb = 1.0f; bda = 0.9f;
			} else {
				bdr = 0.3f; bdg = 0.3f; bdb = 0.4f; bda = 0.6f;
			}
			float bx0 = btn_x, by0 = row_y + 3.0f * s;
			float bx1 = btn_x + keybind_btn_w, by1 = row_y + 3.0f * s + keybind_btn_h;
			Render_thick_line(bx0, by0, bx1, by0, 1.0f * s, bdr, bdg, bdb, bda);
			Render_thick_line(bx0, by1, bx1, by1, 1.0f * s, bdr, bdg, bdb, bda);
			Render_thick_line(bx0, by0, bx0, by1, 1.0f * s, bdr, bdg, bdb, bda);
			Render_thick_line(bx1, by0, bx1, by1, 1.0f * s, bdr, bdg, bdb, bda);
		}

		/* Reset Defaults button -- chamfered NE+SW, centered */
		float reset_w = 180.0f * s;
		float reset_x = content_x + (content_width - reset_w) * 0.5f;
		float reset_y = list_y + KEYBIND_VISIBLE_ROWS * keybind_row_h + 8.0f * s;
		float reset_h = button_h;
		{
			BatchRenderer *batch = Graphics_get_batch();
			float ch = 6.0f * s;
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
					1.0f * s, 0.4f, 0.3f, 0.3f, 0.6f);
			}
		}

		/* Scrollbar */
		int maxScroll = BIND_COUNT - KEYBIND_VISIBLE_ROWS;
		if (maxScroll < 0) maxScroll = 0;
		if (maxScroll > 0) {
			float sb_w = 8.0f * s;
			float sb_x = list_x + content_width - 20.0f * s;
			float track_y = list_y;
			float track_h = KEYBIND_VISIBLE_ROWS * keybind_row_h;
			float thumb_h = (track_h * KEYBIND_VISIBLE_ROWS) / (float)BIND_COUNT;
			if (thumb_h < 20.0f * s) thumb_h = 20.0f * s;
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
	float btn_y = py + ph - button_h - button_gap;
	float btn_gap_inner = 40.0f * s;
	float btn_total = button_w * 2.0f + btn_gap_inner;
	float btn_off = (content_width - btn_total) * 0.5f;

	/* Cancel button -- chamfered NE+SW */
	float cancel_x = content_x + btn_off;
	{
		BatchRenderer *batch = Graphics_get_batch();
		float bx = cancel_x, by = btn_y;
		float bw = button_w, bh = button_h;
		float ch = 8.0f * s;
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
				1.0f * s, 0.4f, 0.3f, 0.3f, 0.7f);
		}
	}

	/* OK button -- chamfered NE+SW */
	float ok_x = cancel_x + button_w + btn_gap_inner;
	{
		BatchRenderer *batch = Graphics_get_batch();
		float bx = ok_x, by = btn_y;
		float bw = button_w, bh = button_h;
		float ch = 8.0f * s;
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
				1.0f * s, 0.3f, 0.4f, 0.6f, 0.7f);
		}
	}

	/* Flush all geometry so text renders on top */
	Render_flush(&proj, &ident);

	/*
	 * Pass 2: All text
	 */

	/* Title */
	Text_render(tr, shaders, &proj, &ident,
		"SETTINGS", px + pw * 0.5f - 35.0f * s, py - 5.0f * s,
		0.7f, 0.7f, 1.0f, 0.9f);

	/* Tab labels */
	tab_y = py + tab_gap;
	for (int t = 0; t < TAB_COUNT; t++) {
		bool selected = (t == selectedTab);
		float text_y = tab_y + tab_h * 0.5f + 5.0f * s;
		Text_render(tr, shaders, &proj, &ident,
			tab_names[t], tab_x + 8.0f * s, text_y,
			selected ? 1.0f : 0.5f,
			selected ? 1.0f : 0.5f,
			selected ? 1.0f : 0.5f,
			0.9f);

		tab_y += tab_h + tab_gap;
	}

	/* Content text */
	if (selectedTab == TAB_GRAPHICS) {
		float gfx_sb_w = GFX_SCROLLBAR_W * s;
		float btn_y_vis = py + ph - button_h - button_gap;
		float gfx_visible_h = btn_y_vis - 10.0f * s - content_y;

		/* Scissor for text too */
		int draw_w, draw_h;
		Graphics_get_drawable_size(&draw_w, &draw_h);
		float dpi_x = (float)draw_w / screen->width;
		float dpi_y = (float)draw_h / screen->height;
		Render_scissor_begin(
			(int)(content_x * dpi_x),
			draw_h - (int)((content_y + gfx_visible_h) * dpi_y),
			(int)((content_width - gfx_sb_w - tab_gap) * dpi_x),
			(int)(gfx_visible_h * dpi_y));

		const char *toggle_labels[8] = {
			"Fullscreen", "Multisampling", "Antialiasing", "Pixel Snapping",
			"Bloom", "Lighting", "Reflections", "Circuit Traces"
		};
		bool toggle_vals[8] = {
			pendingFullscreen, pendingMultisampling,
			pendingAntialiasing, pendingPixelSnapping,
			pendingBloom, pendingLighting,
			pendingReflections, pendingCircuitTraces
		};
		for (int i = 0; i < 8; i++) {
			float row_y = content_y + 30.0f * s + i * 40.0f * s - gfxScroll;
			Text_render(tr, shaders, &proj, &ident,
				toggle_labels[i], label_x, row_y + 15.0f * s,
				0.8f, 0.8f, 0.9f, 0.9f);
			Text_render(tr, shaders, &proj, &ident,
				toggle_vals[i] ? "ON" : "OFF",
				toggle_x + 12.0f * s, row_y + 15.0f * s,
				toggle_vals[i] ? 0.3f : 0.6f,
				toggle_vals[i] ? 1.0f : 0.6f,
				toggle_vals[i] ? 0.3f : 0.6f,
				0.9f);
		}

		/* UI Scale — label left-aligned with toggle labels */
		float scale_label_y = content_y + 30.0f * s + 8.0f * 40.0f * s
			+ GFX_SCALE_EXTRA_GAP * s - gfxScroll;
		Text_render(tr, shaders, &proj, &ident,
			"UI Scale", label_x, scale_label_y + 14.0f * s,
			0.8f, 0.8f, 0.9f, 0.9f);

		/* Percentage label next to slider (group centered) */
		float scale_row_y = scale_label_y + GFX_SCALE_LABEL_H * s;
		{
			float group_w = slider_w + GFX_SCALE_PCT_GAP * s + GFX_SCALE_PCT_W * s;
			float scale_slider_x = content_x + (content_width - gfx_sb_w - group_w) * 0.5f;
			char scale_buf[16];
			snprintf(scale_buf, sizeof(scale_buf), "%d%%", (int)(pendingUiScale * 100.0f + 0.5f));
			Text_render(tr, shaders, &proj, &ident,
				scale_buf, scale_slider_x + slider_w + GFX_SCALE_PCT_GAP * s,
				scale_row_y + slider_knob_h * 0.5f + 4.0f * s,
				0.6f, 0.75f, 1.0f, 0.9f);
		}

		Render_scissor_end();
	}

	/* Audio tab text */
	if (selectedTab == TAB_AUDIO) {
		float combo_x = content_x + (content_width - slider_combo_w) * 0.5f;
		float slider_x = combo_x + slider_left_pad;
		float group_start = content_y + 40.0f * s;
		float group_spacing = 75.0f * s;
		float knob_offset = 22.0f * s;
		float combo_center = combo_x + slider_combo_w * 0.5f;

		const char *labels[3] = { "Background Music", "Sound Effects", "Voice Recordings" };

		for (int si = 0; si < 3; si++) {
			float gy = group_start + si * group_spacing;
			float knob_y = gy + knob_offset;
			float bar_center_y = knob_y + slider_knob_h * 0.5f + 4.0f * s;

			/* Label centered above combo */
			float lw = Text_measure_width(tr, labels[si]);
			Text_render(tr, shaders, &proj, &ident,
				labels[si], combo_center - lw * 0.5f, gy + 14.0f * s,
				0.8f, 0.8f, 0.9f, 0.9f);

			/* "0" left of slider */
			Text_render(tr, shaders, &proj, &ident,
				"0", slider_x - 16.0f * s, bar_center_y,
				0.5f, 0.5f, 0.55f, 0.7f);

			/* "100" right of slider */
			Text_render(tr, shaders, &proj, &ident,
				"100", slider_x + slider_w + 6.0f * s, bar_center_y,
				0.5f, 0.5f, 0.55f, 0.7f);

			/* Current value */
			float volumes[3] = { pendingMusicVol, pendingSFXVol, pendingVoiceVol };
			char val_buf[8];
			snprintf(val_buf, sizeof(val_buf), "%d", (int)(volumes[si] * 100.0f + 0.5f));
			Text_render(tr, shaders, &proj, &ident,
				val_buf, slider_x + slider_w + 42.0f * s, bar_center_y,
				0.6f, 0.75f, 1.0f, 0.9f);
		}
	}

	/* Keybinds tab text */
	if (selectedTab == TAB_KEYBINDS) {
		float list_x = content_x + 10.0f * s;
		float list_y = content_y + 10.0f * s;
		float btn_x = list_x + content_width - 30.0f * s - keybind_btn_w;

		for (int i = 0; i < KEYBIND_VISIBLE_ROWS && (i + scrollOffset) < BIND_COUNT; i++) {
			float row_y = list_y + i * keybind_row_h;
			int action = i + scrollOffset;

			/* Action name */
			Text_render(tr, shaders, &proj, &ident,
				Keybinds_get_action_name((BindAction)action),
				list_x + 8.0f * s, row_y + 18.0f * s,
				0.8f, 0.8f, 0.9f, 0.9f);

			/* Binding name or "Press key..." */
			if (action == listenAction) {
				Text_render(tr, shaders, &proj, &ident,
					"Press key...",
					btn_x + 8.0f * s, row_y + 18.0f * s,
					0.4f, 0.6f, 1.0f, 0.9f);
			} else {
				const char *bindName = Keybinds_input_name(pendingBinds[action]);
				Text_render(tr, shaders, &proj, &ident,
					bindName,
					btn_x + 8.0f * s, row_y + 18.0f * s,
					0.6f, 0.7f, 0.9f, 0.9f);
			}
		}

		/* Reset Defaults button text */
		float reset_w = 180.0f * s;
		float reset_x = content_x + (content_width - reset_w) * 0.5f;
		float reset_y = list_y + KEYBIND_VISIBLE_ROWS * keybind_row_h + 8.0f * s;
		float rtext_w = Text_measure_width(tr, "Reset Defaults");
		Text_render(tr, shaders, &proj, &ident,
			"Reset Defaults", reset_x + (reset_w - rtext_w) * 0.5f, reset_y + 20.0f * s,
			0.8f, 0.6f, 0.6f, 0.9f);
	}

	/* Button text */
	Text_render(tr, shaders, &proj, &ident,
		"Cancel", cancel_x + 10.0f * s, btn_y + 20.0f * s,
		0.8f, 0.6f, 0.6f, 0.9f);

	Text_render(tr, shaders, &proj, &ident,
		"OK", ok_x + 30.0f * s, btn_y + 20.0f * s,
		0.6f, 0.8f, 1.0f, 0.9f);

	/* Help text */
	Text_render(tr, shaders, &proj, &ident,
		"[I] Close    [ESC] Cancel",
		px + 10.0f * s, py + ph + 15.0f * s,
		0.6f, 0.6f, 0.65f, 0.9f);
}
