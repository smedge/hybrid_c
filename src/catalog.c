#include "catalog.h"

#include "render.h"
#include "text.h"
#include "graphics.h"
#include "skillbar.h"
#include "progression.h"
#include "fragment.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Layout constants */
#define CATALOG_WIDTH 600.0f
#define CATALOG_HEIGHT 400.0f
#define TAB_WIDTH 140.0f
#define TAB_HEIGHT 35.0f
#define TAB_GAP 5.0f
#define ITEM_HEIGHT 70.0f
#define ITEM_ICON_SIZE 50.0f
#define ITEM_GAP 5.0f
#define DRAG_THRESHOLD 5.0f

/* Tab names indexed by SubroutineType */
static const char *tab_names[SUB_TYPE_COUNT] = {
	"Projectile",
	"Deployable",
	"Movement",
	"Shield",
	"Healing"
};

/* State */
static bool catalogOpen = false;
static int selectedTab = 0;
static float scrollOffset = 0.0f;

/* Drag state */
typedef struct {
	bool active;
	bool from_slot;
	int source_slot;
	SubroutineId source_id;
	float start_x, start_y;
	float current_x, current_y;
	bool threshold_met;
} DragState;
static DragState drag = {0};

/* Track mouse button held state across frames */
static bool mouseWasDown = false;
static bool rightWasDown = false;

/* Marquee animation */
#define MARQUEE_PAUSE_MS 1500
#define MARQUEE_CHAR_MS  80
static unsigned int marqueeTimer = 0;

/* Tab notification dots: track last-seen state per sub.
   0=not discovered, 1=discovered/locked, 2=unlocked.
   Mismatch with current state → dot on that sub's tab. */
static int seenState[SUB_ID_COUNT];

/* Helper: compute catalog panel rect (centered on screen) */
static void get_panel_rect(const Screen *screen,
	float *px, float *py, float *pw, float *ph)
{
	*pw = CATALOG_WIDTH;
	*ph = CATALOG_HEIGHT;
	*px = ((float)screen->width - *pw) * 0.5f;
	*py = ((float)screen->height - *ph) * 0.5f - 30.0f;
}

/* Helper: count discovered subs of a given type */
static int count_discovered_for_type(SubroutineType type)
{
	int count = 0;
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (Skillbar_get_sub_type(i) == type && Progression_is_discovered(i))
			count++;
	}
	return count;
}

/* Helper: check if a tab has any content to show */
static bool tab_has_content(SubroutineType type)
{
	return count_discovered_for_type(type) > 0;
}

/* Helper: get current progression state for a sub (0/1/2) */
static int get_sub_state(SubroutineId id)
{
	if (Progression_is_unlocked(id)) return 2;
	if (Progression_is_discovered(id)) return 1;
	return 0;
}

/* Helper: check if a tab has any unseen state changes */
static bool tab_has_new(SubroutineType type)
{
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (Skillbar_get_sub_type(i) != type)
			continue;
		if (get_sub_state(i) != seenState[i] && get_sub_state(i) > 0)
			return true;
	}
	return false;
}

/* Helper: mark all subs on a tab as seen */
static void mark_tab_seen(SubroutineType type)
{
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (Skillbar_get_sub_type(i) == type)
			seenState[i] = get_sub_state(i);
	}
}

/* Helper: find first tab with content */
static int first_valid_tab(void)
{
	for (int i = 0; i < SUB_TYPE_COUNT; i++) {
		if (tab_has_content(i))
			return i;
	}
	return 0;
}

/* Helper: measure text width in pixels */
static float text_width(TextRenderer *tr, const char *text)
{
	float w = 0.0f;
	for (int i = 0; text[i]; i++) {
		int ch = (unsigned char)text[i];
		if (ch < 32 || ch > 127)
			continue;
		w += tr->char_data[ch - 32][6];
	}
	return w;
}

/* Helper: truncate text with "..." to fit within max_width pixels.
   Writes result into buf (must be at least buf_size). */
static void truncate_text(TextRenderer *tr, const char *text,
	float max_width, char *buf, int buf_size)
{
	float w = text_width(tr, text);
	if (w <= max_width) {
		snprintf(buf, buf_size, "%s", text);
		return;
	}

	float ellipsis_w = text_width(tr, "...");
	float budget = max_width - ellipsis_w;
	float accum = 0.0f;
	int fit = 0;
	for (int i = 0; text[i]; i++) {
		int ch = (unsigned char)text[i];
		if (ch < 32 || ch > 127)
			continue;
		accum += tr->char_data[ch - 32][6];
		if (accum > budget)
			break;
		fit = i + 1;
	}
	if (fit + 3 < buf_size) {
		memcpy(buf, text, fit);
		buf[fit] = '.';
		buf[fit + 1] = '.';
		buf[fit + 2] = '.';
		buf[fit + 3] = '\0';
	} else {
		snprintf(buf, buf_size, "...");
	}
}

/* Helper: marquee text — truncate, pause, scroll to end, pause, reset.
   Scrolls by whole characters so no clipping needed. */
static void marquee_text(TextRenderer *tr, const char *text,
	float max_width, unsigned int timer, char *buf, int buf_size)
{
	float full_w = text_width(tr, text);
	if (full_w <= max_width) {
		snprintf(buf, buf_size, "%s", text);
		return;
	}

	int len = (int)strlen(text);
	float ellipsis_w = text_width(tr, "...");

	/* Count how many chars we can skip before the end is visible */
	int max_skip = 0;
	for (int s = 1; s < len; s++) {
		float remaining_w = 0.0f;
		for (int j = s; text[j]; j++) {
			int ch = (unsigned char)text[j];
			if (ch >= 32 && ch <= 127)
				remaining_w += tr->char_data[ch - 32][6];
		}
		if (ellipsis_w + remaining_w <= max_width) {
			max_skip = s;
			break;
		}
		max_skip = s;
	}
	if (max_skip == 0) {
		truncate_text(tr, text, max_width, buf, buf_size);
		return;
	}

	/* Cycle: pause → scroll forward → pause → snap back */
	unsigned int scroll_duration = max_skip * MARQUEE_CHAR_MS;
	unsigned int cycle = MARQUEE_PAUSE_MS + scroll_duration + MARQUEE_PAUSE_MS;
	unsigned int t = timer % cycle;

	int char_offset = 0;
	if (t < MARQUEE_PAUSE_MS) {
		/* Initial pause — show truncated from start */
		char_offset = 0;
	} else if (t < MARQUEE_PAUSE_MS + scroll_duration) {
		/* Scrolling forward */
		char_offset = (t - MARQUEE_PAUSE_MS) / MARQUEE_CHAR_MS;
		if (char_offset > max_skip) char_offset = max_skip;
	} else {
		/* End pause — hold at end */
		char_offset = max_skip;
	}

	/* Build the visible string */
	const char *visible = text + char_offset;
	if (char_offset == 0) {
		/* At start: truncate end with "..." */
		truncate_text(tr, visible, max_width, buf, buf_size);
	} else {
		/* Scrolled: "..." + visible portion, truncated to fit */
		float budget = max_width - ellipsis_w;
		int fit = 0;
		float accum = 0.0f;
		for (int i = 0; visible[i]; i++) {
			int ch = (unsigned char)visible[i];
			if (ch >= 32 && ch <= 127)
				accum += tr->char_data[ch - 32][6];
			if (accum > budget)
				break;
			fit = i + 1;
		}
		if (3 + fit < buf_size) {
			buf[0] = '.'; buf[1] = '.'; buf[2] = '.';
			memcpy(buf + 3, visible, fit);
			buf[3 + fit] = '\0';
		} else {
			snprintf(buf, buf_size, "...");
		}
	}
}

void Catalog_initialize(void)
{
	catalogOpen = false;
	selectedTab = 0;
	scrollOffset = 0.0f;
	memset(&drag, 0, sizeof(drag));
	mouseWasDown = false;
	rightWasDown = false;
	marqueeTimer = 0;
	for (int i = 0; i < SUB_ID_COUNT; i++)
		seenState[i] = get_sub_state(i);
}

void Catalog_cleanup(void)
{
}

bool Catalog_is_open(void)
{
	return catalogOpen;
}

void Catalog_toggle(void)
{
	catalogOpen = !catalogOpen;
	if (catalogOpen) {
		selectedTab = first_valid_tab();
		scrollOffset = 0.0f;
		marqueeTimer = 0;
		memset(&drag, 0, sizeof(drag));
		mouseWasDown = false;
		rightWasDown = false;
		mark_tab_seen(selectedTab);
	}
}

void Catalog_update(const Input *input, const unsigned int ticks)
{
	if (!catalogOpen)
		return;

	marqueeTimer += ticks;

	/* ESC closes catalog (P toggle is handled by mode_gameplay) */
	if (input->keyEsc) {
		catalogOpen = false;
		memset(&drag, 0, sizeof(drag));
		mouseWasDown = false;
		rightWasDown = false;
		return;
	}

	Screen screen = Graphics_get_screen();
	float mx = (float)input->mouseX;
	float my = (float)input->mouseY;

	float px, py, pw, ph;
	get_panel_rect(&screen, &px, &py, &pw, &ph);

	/* Tab bar: rendered at left side of panel */
	float tab_x = px + TAB_GAP;
	float tab_y = py + TAB_GAP;
	if (input->mouseLeft && !mouseWasDown && !drag.active) {
		for (int t = 0; t < SUB_TYPE_COUNT; t++) {
			if (!tab_has_content(t))
				continue;
			float tx = tab_x;
			float ty = tab_y;
			if (mx >= tx && mx <= tx + TAB_WIDTH &&
				my >= ty && my <= ty + TAB_HEIGHT) {
				selectedTab = t;
				scrollOffset = 0.0f;
				marqueeTimer = 0;
				mark_tab_seen(t);
				break;
			}
			tab_y += TAB_HEIGHT + TAB_GAP;
		}
	}

	/* Mouse wheel scrolling */
	float item_area_x = px + TAB_WIDTH + TAB_GAP * 2.0f;
	float item_area_y = py + TAB_GAP;
	float item_area_w = pw - TAB_WIDTH - TAB_GAP * 3.0f;
	float item_area_h = ph - TAB_GAP * 2.0f;
	(void)item_area_w;

	int visible_items = count_discovered_for_type(selectedTab);
	float total_content = visible_items * (ITEM_HEIGHT + ITEM_GAP);
	float max_scroll = total_content - item_area_h;
	if (max_scroll < 0.0f) max_scroll = 0.0f;

	if (input->mouseWheelUp)
		scrollOffset -= ITEM_HEIGHT;
	if (input->mouseWheelDown)
		scrollOffset += ITEM_HEIGHT;
	if (scrollOffset < 0.0f) scrollOffset = 0.0f;
	if (scrollOffset > max_scroll) scrollOffset = max_scroll;

	/* Drag logic */
	if (input->mouseLeft) {
		if (!mouseWasDown && !drag.active) {
			/* Mouse just pressed — check for drag initiation */

			/* Check skill bar slots first */
			int slot = Skillbar_slot_at_position(mx, my, &screen);
			if (slot >= 0 && Skillbar_get_slot_sub(slot) != SUB_NONE) {
				drag.active = true;
				drag.from_slot = true;
				drag.source_slot = slot;
				drag.source_id = Skillbar_get_slot_sub(slot);
				drag.start_x = mx;
				drag.start_y = my;
				drag.current_x = mx;
				drag.current_y = my;
				drag.threshold_met = false;
			} else {
				/* Check catalog item list */
				float iy = item_area_y - scrollOffset;
				for (int i = 0; i < SUB_ID_COUNT; i++) {
					if (Skillbar_get_sub_type(i) != selectedTab)
						continue;
					if (!Progression_is_discovered(i))
						continue;

					float item_top = iy;
					float item_bot = iy + ITEM_HEIGHT;

					if (Progression_is_unlocked(i) &&
						mx >= item_area_x && mx <= item_area_x + item_area_w &&
						my >= item_top && my <= item_bot &&
						item_top >= item_area_y &&
						item_bot <= item_area_y + item_area_h) {
						drag.active = true;
						drag.from_slot = false;
						drag.source_slot = -1;
						drag.source_id = i;
						drag.start_x = mx;
						drag.start_y = my;
						drag.current_x = mx;
						drag.current_y = my;
						drag.threshold_met = false;
						break;
					}
					iy += ITEM_HEIGHT + ITEM_GAP;
				}
			}
		} else if (drag.active) {
			/* Mouse held — update drag position */
			drag.current_x = mx;
			drag.current_y = my;
			float dx = mx - drag.start_x;
			float dy = my - drag.start_y;
			if (sqrtf(dx * dx + dy * dy) > DRAG_THRESHOLD)
				drag.threshold_met = true;
		}
	} else {
		/* Mouse released */
		if (drag.active && drag.threshold_met) {
			int target_slot = Skillbar_slot_at_position(mx, my, &screen);
			if (target_slot >= 0) {
				if (drag.from_slot) {
					/* Slot-to-slot swap */
					Skillbar_swap_slots(drag.source_slot, target_slot);
				} else {
					/* Catalog-to-slot equip */
					/* If already equipped elsewhere, clear that slot first */
					int existing = Skillbar_find_equipped_slot(drag.source_id);
					if (existing >= 0)
						Skillbar_clear_slot(existing);
					Skillbar_equip(target_slot, drag.source_id);
				}
			} else if (drag.from_slot) {
				/* Dragged off the bar — unequip */
				Skillbar_clear_slot(drag.source_slot);
			}
		}
		memset(&drag, 0, sizeof(drag));
	}

	/* Right-click on skill bar slot → unequip */
	if (input->mouseRight && !rightWasDown) {
		int slot = Skillbar_slot_at_position(mx, my, &screen);
		if (slot >= 0)
			Skillbar_clear_slot(slot);
	}

	mouseWasDown = input->mouseLeft;
	rightWasDown = input->mouseRight;
}

void Catalog_render(const Screen *screen)
{
	if (!catalogOpen)
		return;

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	/* Panel rect */
	float px, py, pw, ph;
	get_panel_rect(screen, &px, &py, &pw, &ph);

	float item_area_x = px + TAB_WIDTH + TAB_GAP * 2.0f;
	float item_area_y = py + TAB_GAP;
	float item_area_w = pw - TAB_WIDTH - TAB_GAP * 3.0f;
	float item_area_h = ph - TAB_GAP * 2.0f;

	/*
	 * Pass 1: All geometry (quads, lines, points).
	 * Batched primitives flush together — must come before text.
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
	for (int t = 0; t < SUB_TYPE_COUNT; t++) {
		if (!tab_has_content(t))
			continue;

		bool selected = (t == selectedTab);
		float tr_col = selected ? 0.15f : 0.1f;
		float tg_col = selected ? 0.15f : 0.1f;
		float tb_col = selected ? 0.25f : 0.15f;

		Render_quad_absolute(tab_x, tab_y,
			tab_x + TAB_WIDTH, tab_y + TAB_HEIGHT,
			tr_col, tg_col, tb_col, 0.9f);

		if (selected) {
			float tc = 0.5f;
			Render_thick_line(tab_x, tab_y,
				tab_x + TAB_WIDTH, tab_y, 1.0f, tc, tc, 1.0f, 0.8f);
			Render_thick_line(tab_x, tab_y + TAB_HEIGHT,
				tab_x + TAB_WIDTH, tab_y + TAB_HEIGHT, 1.0f, tc, tc, 1.0f, 0.8f);
			Render_thick_line(tab_x, tab_y,
				tab_x, tab_y + TAB_HEIGHT, 1.0f, tc, tc, 1.0f, 0.8f);
			Render_thick_line(tab_x + TAB_WIDTH, tab_y,
				tab_x + TAB_WIDTH, tab_y + TAB_HEIGHT, 1.0f, tc, tc, 1.0f, 0.8f);
		}

		/* New item notification dot */
		if (tab_has_new(t)) {
			Position dot_pos = {
				tab_x + TAB_WIDTH - 8.0f,
				tab_y + TAB_HEIGHT * 0.5f
			};
			ColorFloat magenta = {1.0f, 0.0f, 1.0f, 0.9f};
			Render_point(&dot_pos, 6.0f, &magenta);
		}

		tab_y += TAB_HEIGHT + TAB_GAP;
	}

	/* Separator line */
	float sep_x = px + TAB_WIDTH + TAB_GAP * 1.5f;
	Render_thick_line(sep_x, py + TAB_GAP, sep_x, py + ph - TAB_GAP,
		1.0f, 0.25f, 0.25f, 0.3f, 0.6f);

	/* Item backgrounds, highlights, icons, progress bars */
	float iy = item_area_y - scrollOffset;
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (Skillbar_get_sub_type(i) != selectedTab)
			continue;
		if (!Progression_is_discovered(i))
			continue;
		if (iy + ITEM_HEIGHT < item_area_y || iy > item_area_y + item_area_h) {
			iy += ITEM_HEIGHT + ITEM_GAP;
			continue;
		}

		bool unlocked = Progression_is_unlocked(i);

		float ibg = 0.12f;
		Render_quad_absolute(item_area_x, iy,
			item_area_x + item_area_w, iy + ITEM_HEIGHT,
			ibg, ibg, ibg + 0.02f, 0.9f);

		int equipped_slot = Skillbar_find_equipped_slot(i);
		if (equipped_slot >= 0) {
			Render_thick_line(item_area_x, iy,
				item_area_x, iy + ITEM_HEIGHT,
				2.0f, 0.3f, 1.0f, 0.3f, 0.8f);
		}

		if (unlocked) {
			float icon_cx = item_area_x + ITEM_ICON_SIZE * 0.5f + 10.0f;
			float icon_cy = iy + ITEM_HEIGHT * 0.5f;
			float half = ITEM_ICON_SIZE * 0.5f;
			float br, bg, bb;
			if (Skillbar_is_elite(i)) {
				br = 1.0f; bg = 0.84f; bb = 0.0f;
			} else {
				br = 1.0f; bg = 1.0f; bb = 1.0f;
			}
			Render_thick_line(icon_cx - half, icon_cy - half,
				icon_cx + half, icon_cy - half,
				1.0f, br, bg, bb, 0.6f);
			Render_thick_line(icon_cx - half, icon_cy + half,
				icon_cx + half, icon_cy + half,
				1.0f, br, bg, bb, 0.6f);
			Render_thick_line(icon_cx - half, icon_cy - half,
				icon_cx - half, icon_cy + half,
				1.0f, br, bg, bb, 0.6f);
			Render_thick_line(icon_cx + half, icon_cy - half,
				icon_cx + half, icon_cy + half,
				1.0f, br, bg, bb, 0.6f);
			Skillbar_render_icon_at(i, icon_cx, icon_cy, 1.0f);
		}

		if (!unlocked) {
			/* Grey icon border for unknown sub */
			float icon_cx = item_area_x + ITEM_ICON_SIZE * 0.5f + 10.0f;
			float icon_cy = iy + ITEM_HEIGHT * 0.5f;
			float half = ITEM_ICON_SIZE * 0.5f;
			Render_thick_line(icon_cx - half, icon_cy - half,
				icon_cx + half, icon_cy - half,
				1.0f, 0.3f, 0.3f, 0.3f, 0.6f);
			Render_thick_line(icon_cx - half, icon_cy + half,
				icon_cx + half, icon_cy + half,
				1.0f, 0.3f, 0.3f, 0.3f, 0.6f);
			Render_thick_line(icon_cx - half, icon_cy - half,
				icon_cx - half, icon_cy + half,
				1.0f, 0.3f, 0.3f, 0.3f, 0.6f);
			Render_thick_line(icon_cx + half, icon_cy - half,
				icon_cx + half, icon_cy + half,
				1.0f, 0.3f, 0.3f, 0.3f, 0.6f);

			float name_x = item_area_x + ITEM_ICON_SIZE + 25.0f;
			float desc_y = iy + 22.0f + 18.0f;
			float bar_x = name_x;
			float bar_y = desc_y + 6.0f;
			float bar_w = 120.0f;
			float bar_h = 4.0f;
			float progress = Progression_get_progress(i);

			Render_quad_absolute(bar_x, bar_y,
				bar_x + bar_w, bar_y + bar_h,
				0.15f, 0.15f, 0.2f, 0.8f);
			Render_quad_absolute(bar_x, bar_y,
				bar_x + bar_w * progress, bar_y + bar_h,
				1.0f, 0.0f, 1.0f, 0.7f);
		}

		iy += ITEM_HEIGHT + ITEM_GAP;
	}

	/* Slot highlights during drag */
	if (drag.active && drag.threshold_met) {
		for (int s = 0; s < SKILLBAR_SLOTS; s++) {
			float bx = 10.0f + s * 60.0f;
			float by = (float)screen->height - 50.0f - 10.0f;
			float sz = 50.0f;

			Render_thick_line(bx, by, bx + sz, by,
				2.0f, 1.0f, 1.0f, 0.0f, 0.6f);
			Render_thick_line(bx, by + sz, bx + sz, by + sz,
				2.0f, 1.0f, 1.0f, 0.0f, 0.6f);
			Render_thick_line(bx, by, bx, by + sz,
				2.0f, 1.0f, 1.0f, 0.0f, 0.6f);
			Render_thick_line(bx + sz, by, bx + sz, by + sz,
				2.0f, 1.0f, 1.0f, 0.0f, 0.6f);
		}
	}

	/* Drag ghost — slot border + icon */
	if (drag.active && drag.threshold_met) {
		float gx = drag.current_x - 25.0f;
		float gy = drag.current_y - 25.0f;
		float gsz = 50.0f;

		Render_quad_absolute(gx, gy, gx + gsz, gy + gsz,
			0.1f, 0.1f, 0.1f, 0.4f);
		Render_thick_line(gx, gy, gx + gsz, gy,
			1.0f, 1.0f, 1.0f, 1.0f, 0.4f);
		Render_thick_line(gx, gy + gsz, gx + gsz, gy + gsz,
			1.0f, 1.0f, 1.0f, 1.0f, 0.4f);
		Render_thick_line(gx, gy, gx, gy + gsz,
			1.0f, 1.0f, 1.0f, 1.0f, 0.4f);
		Render_thick_line(gx + gsz, gy, gx + gsz, gy + gsz,
			1.0f, 1.0f, 1.0f, 1.0f, 0.4f);

		Skillbar_render_icon_at(drag.source_id,
			drag.current_x, drag.current_y, 0.5f);
	}

	/* Flush all geometry so text renders on top */
	Render_flush(&proj, &ident);

	/*
	 * Pass 2: All text (renders immediately via text shader).
	 */

	/* Title */
	Text_render(tr, shaders, &proj, &ident,
		"SUBROUTINE REGISTRY", px + pw * 0.5f - 80.0f, py - 5.0f,
		0.7f, 0.7f, 1.0f, 0.9f);

	/* Tab labels */
	tab_y = py + TAB_GAP;
	for (int t = 0; t < SUB_TYPE_COUNT; t++) {
		if (!tab_has_content(t))
			continue;

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

	/* Item text */
	iy = item_area_y - scrollOffset;
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (Skillbar_get_sub_type(i) != selectedTab)
			continue;
		if (!Progression_is_discovered(i))
			continue;
		if (iy + ITEM_HEIGHT < item_area_y || iy > item_area_y + item_area_h) {
			iy += ITEM_HEIGHT + ITEM_GAP;
			continue;
		}

		bool unlocked = Progression_is_unlocked(i);
		int equipped_slot = Skillbar_find_equipped_slot(i);
		float name_x = item_area_x + ITEM_ICON_SIZE + 25.0f;
		float name_y = iy + 22.0f;
		float desc_y = name_y + 18.0f;
		float right_edge = px + pw - 10.0f;
		float max_text_w = right_edge - name_x;
		char tbuf[128];

		if (unlocked) {
			/* Name — leave room for ELITE tag and [Slot N] if equipped */
			bool elite = Skillbar_is_elite(i);
			float elite_w = elite ? text_width(tr, " ELITE") : 0.0f;
			float name_budget = equipped_slot >= 0
				? max_text_w - 90.0f - elite_w : max_text_w - elite_w;
			truncate_text(tr, Skillbar_get_sub_name(i),
				name_budget, tbuf, sizeof(tbuf));
			Text_render(tr, shaders, &proj, &ident,
				tbuf, name_x, name_y,
				0.9f, 0.9f, 1.0f, 0.95f);

			if (elite) {
				float after_name = name_x + text_width(tr, tbuf);
				Text_render(tr, shaders, &proj, &ident,
					" ELITE", after_name, name_y,
					1.0f, 0.84f, 0.0f, 0.95f);
			}

			marquee_text(tr, Skillbar_get_sub_description(i),
				max_text_w, marqueeTimer, tbuf, sizeof(tbuf));
			Text_render(tr, shaders, &proj, &ident,
				tbuf, name_x, desc_y,
				0.5f, 0.5f, 0.6f, 0.8f);

			if (equipped_slot >= 0) {
				char slotbuf[32];
				snprintf(slotbuf, sizeof(slotbuf), "[Slot %d]", equipped_slot + 1);
				float slot_w = text_width(tr, slotbuf);
				Text_render(tr, shaders, &proj, &ident,
					slotbuf, right_edge - slot_w, name_y,
					0.3f, 1.0f, 0.3f, 0.7f);
			}

			Text_render(tr, shaders, &proj, &ident,
				"drag to equip", name_x, desc_y + 16.0f,
				0.3f, 0.3f, 0.4f, 0.5f);
		} else {
			Text_render(tr, shaders, &proj, &ident,
				"???", name_x, name_y,
				0.5f, 0.5f, 0.5f, 0.7f);

			float icon_cx = item_area_x + ITEM_ICON_SIZE * 0.5f + 10.0f;
			float icon_cy = iy + ITEM_HEIGHT * 0.5f;
			Text_render(tr, shaders, &proj, &ident,
				"?", icon_cx - 4.0f, icon_cy + 5.0f,
				0.5f, 0.5f, 0.5f, 0.7f);

			char buf[64];
			snprintf(buf, sizeof(buf), "Fragments: %d/%d",
				Progression_get_current(i),
				Progression_get_threshold(i));
			Text_render(tr, shaders, &proj, &ident,
				buf, name_x, desc_y,
				1.0f, 0.0f, 1.0f, 0.6f);
		}

		iy += ITEM_HEIGHT + ITEM_GAP;
	}

	/* Help text */
	Text_render(tr, shaders, &proj, &ident,
		"[P] Close    [Right-click slot] Unequip    [Drag] Equip/Swap",
		px + 10.0f, py + ph + 15.0f,
		0.8f, 0.8f, 0.85f, 0.9f);
}
