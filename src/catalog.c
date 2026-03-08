#include "catalog.h"

#include "render.h"
#include "text.h"
#include "graphics.h"
#include "batch.h"
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
#define TAB_CHAMF 6.0f
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
	"Healing",
	"Stealth",
	"Control",
	"Area Effect"
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
	float s = Graphics_get_ui_scale();
	*pw = CATALOG_WIDTH * s;
	*ph = CATALOG_HEIGHT * s;
	*px = ((float)screen->width - *pw) * 0.5f;
	*py = ((float)screen->height - *ph) * 0.5f - 30.0f * s;
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

/* Helper: truncate text with "..." to fit within max_width pixels.
   Writes result into buf (must be at least buf_size). */
static void truncate_text(TextRenderer *tr, const char *text,
	float max_width, char *buf, int buf_size)
{
	float w = Text_measure_width(tr, text);
	if (w <= max_width) {
		snprintf(buf, buf_size, "%s", text);
		return;
	}

	float ellipsis_w = Text_measure_width(tr, "...");
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
	float full_w = Text_measure_width(tr, text);
	if (full_w <= max_width) {
		snprintf(buf, buf_size, "%s", text);
		return;
	}

	int len = (int)strlen(text);
	float ellipsis_w = Text_measure_width(tr, "...");

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
		if (!tab_has_content(selectedTab))
			selectedTab = first_valid_tab();
		scrollOffset = 0.0f;
		marqueeTimer = 0;
		memset(&drag, 0, sizeof(drag));
		mouseWasDown = false;
		rightWasDown = false;
		mark_tab_seen(selectedTab);
	}
}

void Catalog_mark_all_seen(void)
{
	for (int i = 0; i < SUB_ID_COUNT; i++)
		seenState[i] = get_sub_state(i);
}

void Catalog_update(Input *input, const unsigned int ticks)
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

	float s = Graphics_get_ui_scale();
	float tab_width = TAB_WIDTH * s;
	float tab_height = TAB_HEIGHT * s;
	float tab_gap = TAB_GAP * s;
	float item_height = ITEM_HEIGHT * s;
	float item_gap = ITEM_GAP * s;

	Screen screen = Graphics_get_screen();
	float mx = (float)input->mouseX;
	float my = (float)input->mouseY;

	float px, py, pw, ph;
	get_panel_rect(&screen, &px, &py, &pw, &ph);

	/* Tab bar: rendered at left side of panel */
	float tab_x = px + tab_gap;
	float tab_y = py + tab_gap;
	if (input->mouseLeft && !mouseWasDown && !drag.active) {
		for (int t = 0; t < SUB_TYPE_COUNT; t++) {
			if (!tab_has_content(t))
				continue;
			float tx = tab_x;
			float ty = tab_y;
			if (mx >= tx && mx <= tx + tab_width &&
				my >= ty && my <= ty + tab_height) {
				selectedTab = t;
				scrollOffset = 0.0f;
				marqueeTimer = 0;
				mark_tab_seen(t);
				break;
			}
			tab_y += tab_height + tab_gap;
		}
	}

	/* Mouse wheel scrolling */
	float item_area_x = px + tab_width + tab_gap * 2.0f;
	float item_area_y = py + tab_gap;
	float item_area_w = pw - tab_width - tab_gap * 3.0f;
	float item_area_h = ph - tab_gap * 2.0f;
	(void)item_area_w;

	int visible_items = count_discovered_for_type(selectedTab);
	float total_content = visible_items * (item_height + item_gap);
	float max_scroll = total_content - item_area_h;
	if (max_scroll < 0.0f) max_scroll = 0.0f;

	if (input->mouseWheelUp) {
		scrollOffset -= item_height;
		input->mouseWheelUp = false;
	}
	if (input->mouseWheelDown) {
		scrollOffset += item_height;
		input->mouseWheelDown = false;
	}
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
					float item_bot = iy + item_height;

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
					iy += item_height + item_gap;
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

	float s = Graphics_get_ui_scale();
	float tab_width = TAB_WIDTH * s;
	float tab_height = TAB_HEIGHT * s;
	float tab_gap = TAB_GAP * s;
	float tab_chamf = TAB_CHAMF * s;
	float item_height = ITEM_HEIGHT * s;
	float item_icon_size = ITEM_ICON_SIZE * s;
	float item_gap = ITEM_GAP * s;

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	/* Panel rect */
	float px, py, pw, ph;
	get_panel_rect(screen, &px, &py, &pw, &ph);

	float item_area_x = px + tab_width + tab_gap * 2.0f;
	float item_area_y = py + tab_gap;
	float item_area_w = pw - tab_width - tab_gap * 3.0f;
	float item_area_h = ph - tab_gap * 2.0f;

	/*
	 * Pass 1: All geometry (quads, lines, points).
	 * Batched primitives flush together — must come before text.
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
	for (int t = 0; t < SUB_TYPE_COUNT; t++) {
		if (!tab_has_content(t))
			continue;

		bool selected = (t == selectedTab);
		float tr_col = selected ? 0.15f : 0.1f;
		float tg_col = selected ? 0.15f : 0.1f;
		float tb_col = selected ? 0.25f : 0.15f;

		if (selected) {
			/* Chamfered fill: sharp NW + SE, chamfered NE + SW */
			float c = tab_chamf;
			float tx0 = tab_x, ty0 = tab_y;
			float tx1 = tab_x + tab_width, ty1 = tab_y + tab_height;
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
				tab_x + tab_width, tab_y + tab_height,
				tr_col, tg_col, tb_col, 0.9f);
		}

		/* New item notification dot */
		if (tab_has_new(t)) {
			Position dot_pos = {
				tab_x + tab_width - 8.0f * s,
				tab_y + tab_height * 0.5f
			};
			ColorFloat magenta = {1.0f, 0.0f, 1.0f, 0.9f};
			Render_point(&dot_pos, 6.0f * s, &magenta);
		}

		tab_y += tab_height + tab_gap;
	}

	/* Separator line */
	float sep_x = px + tab_width + tab_gap * 1.5f;
	Render_thick_line(sep_x, py + tab_gap, sep_x, py + ph - tab_gap,
		1.0f * s, 0.25f, 0.25f, 0.3f, 0.6f);

	/* Flush panel/tab geometry before enabling scissor */
	Render_flush(&proj, &ident);

	/* Scissor the item area — GL uses bottom-left origin + physical pixels */
	int draw_w, draw_h;
	Graphics_get_drawable_size(&draw_w, &draw_h);
	float scale_x = (float)draw_w / (float)screen->width;
	float scale_y = (float)draw_h / (float)screen->height;
	int scissor_x = (int)(item_area_x * scale_x);
	int scissor_y = (int)(((float)screen->height - (item_area_y + item_area_h)) * scale_y);
	int scissor_w = (int)(item_area_w * scale_x);
	int scissor_h = (int)(item_area_h * scale_y);
	Render_scissor_begin(scissor_x, scissor_y, scissor_w, scissor_h);

	/* Item backgrounds, highlights, icons, progress bars */
	float iy = item_area_y - scrollOffset;
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (Skillbar_get_sub_type(i) != selectedTab)
			continue;
		if (!Progression_is_discovered(i))
			continue;
		if (iy + item_height < item_area_y || iy > item_area_y + item_area_h) {
			iy += item_height + item_gap;
			continue;
		}

		bool unlocked = Progression_is_unlocked(i);

		float ibg = 0.12f;
		Render_quad_absolute(item_area_x, iy,
			item_area_x + item_area_w, iy + item_height,
			ibg, ibg, ibg + 0.02f, 0.9f);

		int equipped_slot = Skillbar_find_equipped_slot(i);
		if (equipped_slot >= 0) {
			Render_thick_line(item_area_x, iy,
				item_area_x, iy + item_height,
				2.0f * s, 0.3f, 1.0f, 0.3f, 0.8f);
		}

		if (unlocked) {
			float icon_cx = item_area_x + item_icon_size * 0.5f + 10.0f * s;
			float icon_cy = iy + item_height * 0.5f;
			float half = item_icon_size * 0.5f;
			float ich = 8.0f * s;
			ColorFloat bc = TIER_COLORS[Skillbar_get_tier(i)];
			float ix0 = icon_cx - half, iy0 = icon_cy - half;
			float ix1 = icon_cx + half, iy1 = icon_cy + half;
			float ivx[6] = { ix0,       ix1 - ich, ix1,
			                  ix1,       ix0 + ich, ix0 };
			float ivy[6] = { iy0,       iy0,        iy0 + ich,
			                  iy1,       iy1,        iy1 - ich };
			for (int v = 0; v < 6; v++) {
				int nv = (v + 1) % 6;
				Render_thick_line(ivx[v], ivy[v], ivx[nv], ivy[nv],
					1.0f * s, bc.red, bc.green, bc.blue, 0.6f);
			}
			Skillbar_render_icon_at(i, icon_cx, icon_cy, 1.0f);
		}

		if (!unlocked) {
			/* Grey icon border for unknown sub */
			float icon_cx = item_area_x + item_icon_size * 0.5f + 10.0f * s;
			float icon_cy = iy + item_height * 0.5f;
			float half = item_icon_size * 0.5f;
			float ich = 8.0f * s;
			float ix0 = icon_cx - half, iy0 = icon_cy - half;
			float ix1 = icon_cx + half, iy1 = icon_cy + half;
			float ivx[6] = { ix0,       ix1 - ich, ix1,
			                  ix1,       ix0 + ich, ix0 };
			float ivy[6] = { iy0,       iy0,        iy0 + ich,
			                  iy1,       iy1,        iy1 - ich };
			for (int v = 0; v < 6; v++) {
				int nv = (v + 1) % 6;
				Render_thick_line(ivx[v], ivy[v], ivx[nv], ivy[nv],
					1.0f * s, 0.3f, 0.3f, 0.3f, 0.6f);
			}

			float name_x = item_area_x + item_icon_size + 25.0f * s;
			float desc_y = iy + 22.0f * s + 18.0f * s;
			float bar_x = name_x;
			float bar_y = desc_y + 6.0f * s;
			float bar_w = 120.0f * s;
			float bar_h = 4.0f * s;
			float progress = Progression_get_progress(i);
			ColorFloat tc = TIER_COLORS[Skillbar_get_tier(i)];

			Render_quad_absolute(bar_x, bar_y,
				bar_x + bar_w, bar_y + bar_h,
				0.15f, 0.15f, 0.2f, 0.8f);
			Render_quad_absolute(bar_x, bar_y,
				bar_x + bar_w * progress, bar_y + bar_h,
				tc.red, tc.green, tc.blue, 0.7f);
		}

		iy += item_height + item_gap;
	}

	/* Scrollbar — thin track + thumb when content overflows */
	{
		int visible_items = count_discovered_for_type(selectedTab);
		float total_content = visible_items * (item_height + item_gap);
		if (total_content > item_area_h) {
			float track_w = 4.0f * s;
			float track_x = item_area_x + item_area_w - track_w - 2.0f * s;
			float track_y = item_area_y;
			float track_h = item_area_h;

			/* Track background */
			Render_quad_absolute(track_x, track_y,
				track_x + track_w, track_y + track_h,
				0.15f, 0.15f, 0.2f, 0.4f);

			/* Thumb */
			float visible_ratio = item_area_h / total_content;
			float thumb_h = track_h * visible_ratio;
			if (thumb_h < 20.0f * s) thumb_h = 20.0f * s;
			float max_scroll = total_content - item_area_h;
			float scroll_ratio = (max_scroll > 0.0f)
				? scrollOffset / max_scroll : 0.0f;
			float thumb_y = track_y + scroll_ratio * (track_h - thumb_h);

			Render_quad_absolute(track_x, thumb_y,
				track_x + track_w, thumb_y + thumb_h,
				0.4f, 0.4f, 0.6f, 0.7f);
		}
	}

	/* Flush item geometry while scissor is active */
	Render_flush(&proj, &ident);
	Render_scissor_end();

	/* Slot highlights during drag */
	if (drag.active && drag.threshold_met) {
		float ch = 8.0f * s;
		for (int sl = 0; sl < SKILLBAR_SLOTS; sl++) {
			float bx = 10.0f * s + sl * 60.0f * s;
			float by = (float)screen->height - 50.0f * s - 10.0f * s;
			float sz = 50.0f * s;
			float hvx[6] = { bx,       bx + sz - ch, bx + sz,
			                  bx + sz,  bx + ch,      bx };
			float hvy[6] = { by,       by,            by + ch,
			                  by + sz,  by + sz,       by + sz - ch };
			for (int v = 0; v < 6; v++) {
				int nv = (v + 1) % 6;
				Render_thick_line(hvx[v], hvy[v], hvx[nv], hvy[nv],
					2.0f * s, 0.0f, 1.0f, 0.0f, 0.6f);
			}
		}
	}

	/* Drag ghost — chamfered slot border + icon */
	if (drag.active && drag.threshold_met) {
		float gx = drag.current_x - 25.0f * s;
		float gy = drag.current_y - 25.0f * s;
		float gsz = 50.0f * s;
		float ch = 8.0f * s;
		float gvx[6] = { gx,        gx + gsz - ch, gx + gsz,
		                  gx + gsz,  gx + ch,        gx };
		float gvy[6] = { gy,        gy,              gy + ch,
		                  gy + gsz,  gy + gsz,        gy + gsz - ch };

		/* Fill */
		float gcx = gx + gsz * 0.5f;
		float gcy = gy + gsz * 0.5f;
		BatchRenderer *batch = Graphics_get_batch();
		for (int v = 0; v < 6; v++) {
			int nv = (v + 1) % 6;
			Batch_push_triangle_vertices(batch,
				gcx, gcy, gvx[v], gvy[v], gvx[nv], gvy[nv],
				0.1f, 0.1f, 0.1f, 0.4f);
		}
		/* Border — tier colored */
		ColorFloat gc = TIER_COLORS[Skillbar_get_tier(drag.source_id)];
		for (int v = 0; v < 6; v++) {
			int nv = (v + 1) % 6;
			Render_thick_line(gvx[v], gvy[v], gvx[nv], gvy[nv],
				1.0f * s, gc.red, gc.green, gc.blue, 0.4f);
		}

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
		"SUBROUTINE REGISTRY", px + pw * 0.5f - 80.0f * s, py - 5.0f * s,
		0.7f, 0.7f, 1.0f, 0.9f);

	/* Tab labels */
	tab_y = py + tab_gap;
	for (int t = 0; t < SUB_TYPE_COUNT; t++) {
		if (!tab_has_content(t))
			continue;

		bool selected = (t == selectedTab);
		float text_y = tab_y + tab_height * 0.5f + 5.0f * s;
		Text_render(tr, shaders, &proj, &ident,
			tab_names[t], tab_x + 8.0f * s, text_y,
			selected ? 1.0f : 0.5f,
			selected ? 1.0f : 0.5f,
			selected ? 1.0f : 0.5f,
			0.9f);

		tab_y += tab_height + tab_gap;
	}

	/* Item text — scissor to item area */
	Render_scissor_begin(scissor_x, scissor_y, scissor_w, scissor_h);
	iy = item_area_y - scrollOffset;
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (Skillbar_get_sub_type(i) != selectedTab)
			continue;
		if (!Progression_is_discovered(i))
			continue;
		if (iy + item_height < item_area_y || iy > item_area_y + item_area_h) {
			iy += item_height + item_gap;
			continue;
		}

		bool unlocked = Progression_is_unlocked(i);
		int equipped_slot = Skillbar_find_equipped_slot(i);
		float name_x = item_area_x + item_icon_size + 25.0f * s;
		float name_y = iy + 22.0f * s;
		float desc_y = name_y + 18.0f * s;
		float right_edge = px + pw - 10.0f * s;
		float max_text_w = right_edge - name_x;
		char tbuf[128];

		if (unlocked) {
			/* Name — leave room for tier tag and [Slot N] if equipped */
			SubroutineTier tier = Skillbar_get_tier(i);
			const char *tier_label = NULL;
			ColorFloat tier_c = TIER_COLORS[tier];
			if (tier == TIER_ELITE)
				tier_label = " ELITE";
			else if (tier == TIER_RARE)
				tier_label = " RARE";
			float tier_w = tier_label ? Text_measure_width(tr, tier_label) : 0.0f;
			float name_budget = equipped_slot >= 0
				? max_text_w - 90.0f * s - tier_w : max_text_w - tier_w;
			truncate_text(tr, Skillbar_get_sub_name(i),
				name_budget, tbuf, sizeof(tbuf));
			Text_render(tr, shaders, &proj, &ident,
				tbuf, name_x, name_y,
				0.9f, 0.9f, 1.0f, 0.95f);

			if (tier_label) {
				float after_name = name_x + Text_measure_width(tr, tbuf);
				Text_render(tr, shaders, &proj, &ident,
					tier_label, after_name, name_y,
					tier_c.red, tier_c.green, tier_c.blue, 0.95f);
			}

			marquee_text(tr, Skillbar_get_sub_description(i),
				max_text_w, marqueeTimer, tbuf, sizeof(tbuf));
			Text_render(tr, shaders, &proj, &ident,
				tbuf, name_x, desc_y,
				0.5f, 0.5f, 0.6f, 0.8f);

			if (equipped_slot >= 0) {
				char slotbuf[32];
				snprintf(slotbuf, sizeof(slotbuf), "[Slot %d]", equipped_slot + 1);
				float slot_w = Text_measure_width(tr, slotbuf);
				Text_render(tr, shaders, &proj, &ident,
					slotbuf, right_edge - slot_w, name_y,
					0.3f, 1.0f, 0.3f, 0.7f);
			}

			Text_render(tr, shaders, &proj, &ident,
				"drag to equip", name_x, desc_y + 16.0f * s,
				0.3f, 0.3f, 0.4f, 0.5f);
		} else {
			/* "unknown_sub <EnemyType>" with enemy name colored */
			const char *prefix = "unknown_sub ";
			const char *enemy = Progression_get_enemy_name(i);
			const char *suffix = "";
			ColorFloat ec = Progression_get_enemy_color(i);

			float cx = name_x;
			Text_render(tr, shaders, &proj, &ident,
				prefix, cx, name_y,
				0.5f, 0.5f, 0.5f, 0.7f);
			cx += Text_measure_width(tr, prefix);
			Text_render(tr, shaders, &proj, &ident,
				enemy, cx, name_y,
				ec.red, ec.green, ec.blue, 0.9f);
			cx += Text_measure_width(tr, enemy);
			Text_render(tr, shaders, &proj, &ident,
				suffix, cx, name_y,
				0.5f, 0.5f, 0.5f, 0.7f);

			float icon_cx = item_area_x + item_icon_size * 0.5f + 10.0f * s;
			float icon_cy = iy + item_height * 0.5f;
			Text_render(tr, shaders, &proj, &ident,
				"?", icon_cx - 4.0f * s, icon_cy + 5.0f * s,
				0.5f, 0.5f, 0.5f, 0.7f);

			char buf[64];
			snprintf(buf, sizeof(buf), "Fragments: %d/%d",
				Progression_get_current(i),
				Progression_get_threshold(i));
			ColorFloat ftc = TIER_COLORS[Skillbar_get_tier(i)];
			Text_render(tr, shaders, &proj, &ident,
				buf, name_x, desc_y,
				ftc.red, ftc.green, ftc.blue, 0.95f);
		}

		iy += item_height + item_gap;
	}
	Render_scissor_end();

	/* Help text */
	Text_render(tr, shaders, &proj, &ident,
		"[P] Close    [Right-click slot] Unequip    [Drag] Equip/Swap",
		px + 10.0f * s, py + ph + 15.0f * s,
		0.6f, 0.6f, 0.65f, 0.9f);
}
