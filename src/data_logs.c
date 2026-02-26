#include "data_logs.h"

#include "render.h"
#include "text.h"
#include "graphics.h"
#include "mat4.h"
#include "narrative.h"
#include "data_node.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

/* Layout constants — matched to catalog.c styling */
#define DLOG_WIDTH 600.0f
#define DLOG_HEIGHT 400.0f
#define TAB_WIDTH 140.0f
#define TAB_HEIGHT 35.0f
#define TAB_GAP 5.0f
#define PADDING 12.0f
#define TITLE_ROW_HEIGHT 24.0f
#define BODY_LINE_HEIGHT 16.0f
#define SCROLLBAR_WIDTH 8.0f
#define INDICATOR_WIDTH 16.0f

/* Catalog-matched colors */
#define BG_R 0.08f
#define BG_G 0.08f
#define BG_B 0.12f
#define BG_A 0.95f

#define BORDER_R 0.3f
#define BORDER_G 0.3f
#define BORDER_B 0.3f
#define BORDER_A 0.8f
#define BORDER_W 1.0f

#define TAB_SEL_BG_R 0.15f
#define TAB_SEL_BG_G 0.15f
#define TAB_SEL_BG_B 0.25f

#define TAB_UNSEL_BG_R 0.1f
#define TAB_UNSEL_BG_G 0.1f
#define TAB_UNSEL_BG_B 0.15f

#define TAB_BORDER_R 0.5f
#define TAB_BORDER_G 0.5f
#define TAB_BORDER_B 1.0f
#define TAB_BORDER_A 0.8f

#define SEP_R 0.25f
#define SEP_G 0.25f
#define SEP_B 0.3f
#define SEP_A 0.6f

#define MAX_ZONE_TABS 32

/* State */
static bool logsOpen = false;
static int selectedTab = 0;
static float scrollOffset = 0.0f;
static int expandedEntry = -1; /* index within current tab's entries, -1 = none */
static bool mouseWasDown = false;

/* Cached zone tab list — rebuilt when window opens */
static char zoneTabs[MAX_ZONE_TABS][64];
static int zoneTabCount = 0;

/* Panel position helpers (computed each frame from screen size) */
static float panel_x(const Screen *s) { return (s->width - DLOG_WIDTH) * 0.5f; }
static float panel_y(const Screen *s) { return (s->height - DLOG_HEIGHT) * 0.5f - 30.0f; }

static void rebuild_tabs(void)
{
	zoneTabCount = 0;

	/* Walk all collected nodes, build unique zone name list in collection order */
	for (int i = 0; i < DataNode_collected_count(); i++) {
		const char *nid = DataNode_collected_id(i);
		const NarrativeEntry *e = Narrative_get(nid);
		if (!e) continue;

		/* Check if zone already in tabs */
		bool found = false;
		for (int t = 0; t < zoneTabCount; t++) {
			if (strcmp(zoneTabs[t], e->zone_name) == 0) {
				found = true;
				break;
			}
		}
		if (!found && zoneTabCount < MAX_ZONE_TABS) {
			strncpy(zoneTabs[zoneTabCount], e->zone_name, 63);
			zoneTabs[zoneTabCount][63] = '\0';
			zoneTabCount++;
		}
	}
}

/* Count entries for a given zone tab */
static int count_entries_for_tab(int tab)
{
	if (tab < 0 || tab >= zoneTabCount) return 0;
	int count = 0;
	for (int i = 0; i < DataNode_collected_count(); i++) {
		const NarrativeEntry *e = Narrative_get(DataNode_collected_id(i));
		if (e && strcmp(e->zone_name, zoneTabs[tab]) == 0)
			count++;
	}
	return count;
}

/* Get the nth entry for a given zone tab */
static const NarrativeEntry *get_entry_for_tab(int tab, int index)
{
	if (tab < 0 || tab >= zoneTabCount) return NULL;
	int count = 0;
	for (int i = 0; i < DataNode_collected_count(); i++) {
		const NarrativeEntry *e = Narrative_get(DataNode_collected_id(i));
		if (e && strcmp(e->zone_name, zoneTabs[tab]) == 0) {
			if (count == index) return e;
			count++;
		}
	}
	return NULL;
}

void DataLogs_initialize(void)
{
	logsOpen = false;
	selectedTab = 0;
	scrollOffset = 0.0f;
	expandedEntry = -1;
}

void DataLogs_cleanup(void)
{
	logsOpen = false;
}

void DataLogs_toggle(void)
{
	logsOpen = !logsOpen;
	if (logsOpen) {
		rebuild_tabs();
		scrollOffset = 0.0f;
		expandedEntry = -1;
		if (selectedTab >= zoneTabCount)
			selectedTab = 0;
	}
}

bool DataLogs_is_open(void)
{
	return logsOpen;
}

/* Measure wrapped body height for an entry */
static float measure_body_height(TextRenderer *tr, const NarrativeEntry *e, float max_w)
{
	float h = 0.0f;
	const char *p = e->body;
	char linebuf[512];
	int linelen = 0;

	while (*p) {
		if (*p == '\n') {
			h += BODY_LINE_HEIGHT;
			linelen = 0;
			p++;
			continue;
		}
		linebuf[linelen++] = *p++;
		linebuf[linelen] = '\0';
		if (linelen >= 510 || Text_measure_width(tr, linebuf) > max_w) {
			int wrap = linelen - 1;
			while (wrap > 0 && linebuf[wrap] != ' ') wrap--;
			if (wrap > 0) {
				char overflow[512];
				strncpy(overflow, linebuf + wrap + 1, 511);
				overflow[511] = '\0';
				h += BODY_LINE_HEIGHT;
				strcpy(linebuf, overflow);
				linelen = (int)strlen(linebuf);
			} else {
				h += BODY_LINE_HEIGHT;
				linelen = 0;
			}
		}
	}
	if (linelen > 0)
		h += BODY_LINE_HEIGHT;
	return h + PADDING; /* bottom padding for expanded body */
}

void DataLogs_update(Input *input, unsigned int ticks)
{
	(void)ticks;
	if (!logsOpen) return;

	if (input->keyEsc) {
		logsOpen = false;
		return;
	}

	/* Scroll with mouse wheel */
	if (input->mouseWheelUp)
		scrollOffset -= 30.0f;
	if (input->mouseWheelDown)
		scrollOffset += 30.0f;
	if (scrollOffset < 0.0f) scrollOffset = 0.0f;

	/* Click handling */
	bool mouseDown = input->mouseLeft;
	bool clicked = mouseDown && !mouseWasDown;
	mouseWasDown = mouseDown;

	if (!clicked) return;

	float mx = (float)input->mouseX;
	float my = (float)input->mouseY;

	/* Window position */
	Screen screen = Graphics_get_screen();
	float px = panel_x(&screen);
	float py = panel_y(&screen);

	/* Check if click is outside window → close */
	if (mx < px || mx > px + DLOG_WIDTH || my < py || my > py + DLOG_HEIGHT) {
		logsOpen = false;
		return;
	}

	/* Tab clicks (left column) */
	float tab_x = px + TAB_GAP;
	float tab_y = py + TAB_GAP;
	for (int t = 0; t < zoneTabCount; t++) {
		float ty = tab_y + t * (TAB_HEIGHT + TAB_GAP);
		if (mx >= tab_x && mx <= tab_x + TAB_WIDTH &&
		    my >= ty && my <= ty + TAB_HEIGHT) {
			selectedTab = t;
			scrollOffset = 0.0f;
			expandedEntry = -1;
			return;
		}
	}

	/* Content area clicks (accordion titles) */
	float content_x = px + TAB_WIDTH + TAB_GAP * 2.0f;
	float content_top = py + TAB_GAP;
	float content_w = DLOG_WIDTH - TAB_WIDTH - TAB_GAP * 3.0f - SCROLLBAR_WIDTH;

	TextRenderer *tr = Graphics_get_text_renderer();
	int entry_count = count_entries_for_tab(selectedTab);
	float cursor_y = content_top - scrollOffset;

	for (int i = 0; i < entry_count; i++) {
		/* Title row */
		if (my >= cursor_y && my <= cursor_y + TITLE_ROW_HEIGHT &&
		    mx >= content_x && mx <= content_x + content_w + SCROLLBAR_WIDTH) {
			if (expandedEntry == i)
				expandedEntry = -1; /* collapse */
			else
				expandedEntry = i; /* expand (auto-collapses previous) */
			return;
		}
		cursor_y += TITLE_ROW_HEIGHT;

		/* If expanded, skip over body height */
		if (expandedEntry == i) {
			const NarrativeEntry *e = get_entry_for_tab(selectedTab, i);
			if (e)
				cursor_y += measure_body_height(tr, e, content_w - INDICATOR_WIDTH - 4.0f);
		}
	}
}

/* --- Render helpers --- */

static void render_panel_border(float px, float py, float pw, float ph)
{
	Render_thick_line(px, py, px + pw, py, BORDER_W, BORDER_R, BORDER_G, BORDER_B, BORDER_A);
	Render_thick_line(px + pw, py, px + pw, py + ph, BORDER_W, BORDER_R, BORDER_G, BORDER_B, BORDER_A);
	Render_thick_line(px + pw, py + ph, px, py + ph, BORDER_W, BORDER_R, BORDER_G, BORDER_B, BORDER_A);
	Render_thick_line(px, py + ph, px, py, BORDER_W, BORDER_R, BORDER_G, BORDER_B, BORDER_A);
}

static void render_tab_border(float x, float y, float w, float h)
{
	Render_thick_line(x, y, x + w, y, BORDER_W, TAB_BORDER_R, TAB_BORDER_G, TAB_BORDER_B, TAB_BORDER_A);
	Render_thick_line(x + w, y, x + w, y + h, BORDER_W, TAB_BORDER_R, TAB_BORDER_G, TAB_BORDER_B, TAB_BORDER_A);
	Render_thick_line(x + w, y + h, x, y + h, BORDER_W, TAB_BORDER_R, TAB_BORDER_G, TAB_BORDER_B, TAB_BORDER_A);
	Render_thick_line(x, y + h, x, y, BORDER_W, TAB_BORDER_R, TAB_BORDER_G, TAB_BORDER_B, TAB_BORDER_A);
}

static void render_body_text(TextRenderer *tr, Shaders *shaders, Mat4 *proj, Mat4 *ident,
	const NarrativeEntry *e, float x, float max_w,
	float clip_top, float clip_bottom, float *cursor_y)
{
	const char *p = e->body;
	char linebuf[512];
	int linelen = 0;

	while (*p) {
		if (*p == '\n') {
			linebuf[linelen] = '\0';
			if (*cursor_y > clip_top - BODY_LINE_HEIGHT && *cursor_y < clip_bottom) {
				Text_render(tr, shaders, proj, ident, linebuf, x, *cursor_y,
					0.75f, 0.75f, 0.8f, 0.75f);
			}
			*cursor_y += BODY_LINE_HEIGHT;
			linelen = 0;
			p++;
			continue;
		}
		linebuf[linelen++] = *p++;
		linebuf[linelen] = '\0';
		if (linelen >= 510 || Text_measure_width(tr, linebuf) > max_w) {
			int wrap = linelen - 1;
			while (wrap > 0 && linebuf[wrap] != ' ') wrap--;
			if (wrap > 0) {
				char overflow[512];
				strncpy(overflow, linebuf + wrap + 1, 511);
				overflow[511] = '\0';
				linebuf[wrap] = '\0';
				if (*cursor_y > clip_top - BODY_LINE_HEIGHT && *cursor_y < clip_bottom) {
					Text_render(tr, shaders, proj, ident, linebuf, x, *cursor_y,
						0.75f, 0.75f, 0.8f, 0.75f);
				}
				*cursor_y += BODY_LINE_HEIGHT;
				strcpy(linebuf, overflow);
				linelen = (int)strlen(linebuf);
			} else {
				if (*cursor_y > clip_top - BODY_LINE_HEIGHT && *cursor_y < clip_bottom) {
					Text_render(tr, shaders, proj, ident, linebuf, x, *cursor_y,
						0.75f, 0.75f, 0.8f, 0.75f);
				}
				*cursor_y += BODY_LINE_HEIGHT;
				linelen = 0;
			}
		}
	}
	if (linelen > 0) {
		linebuf[linelen] = '\0';
		if (*cursor_y > clip_top - BODY_LINE_HEIGHT && *cursor_y < clip_bottom) {
			Text_render(tr, shaders, proj, ident, linebuf, x, *cursor_y,
				0.75f, 0.75f, 0.8f, 0.75f);
		}
		*cursor_y += BODY_LINE_HEIGHT;
	}
	*cursor_y += PADDING;
}

void DataLogs_render(const Screen *screen)
{
	if (!logsOpen) return;

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();

	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	float px = panel_x(screen);
	float py = panel_y(screen);

	/* ── Panel background ── */
	Render_quad_absolute(px, py, px + DLOG_WIDTH, py + DLOG_HEIGHT,
		BG_R, BG_G, BG_B, BG_A);

	/* ── Panel border ── */
	render_panel_border(px, py, DLOG_WIDTH, DLOG_HEIGHT);

	/* ── Separator between tabs and content ── */
	float sep_x = px + TAB_WIDTH + TAB_GAP * 1.5f;
	Render_thick_line(sep_x, py + TAB_GAP, sep_x, py + DLOG_HEIGHT - TAB_GAP,
		1.0f, SEP_R, SEP_G, SEP_B, SEP_A);

	/* ── Empty state ── */
	if (zoneTabCount == 0) {
		Render_flush(&proj, &ident);

		/* Title (floating above border) */
		const char *title = "DATA LOGS";
		float tw = Text_measure_width(tr, title);
		Text_render(tr, shaders, &proj, &ident, title,
			px + DLOG_WIDTH * 0.5f - tw * 0.5f, py - 5.0f,
			0.7f, 0.7f, 1.0f, 0.9f);

		const char *empty = "No data transfers recorded.";
		float ew = Text_measure_width(tr, empty);
		Text_render(tr, shaders, &proj, &ident, empty,
			px + DLOG_WIDTH * 0.5f - ew * 0.5f,
			py + DLOG_HEIGHT * 0.5f,
			0.5f, 0.5f, 0.5f, 0.6f);

		/* Key hints */
		Text_render(tr, shaders, &proj, &ident,
			"[L] Close",
			px + 10.0f, py + DLOG_HEIGHT + 15.0f,
			0.6f, 0.6f, 0.65f, 0.9f);
		return;
	}

	/* ── Tab backgrounds ── */
	float tab_x = px + TAB_GAP;
	float tab_y = py + TAB_GAP;
	for (int t = 0; t < zoneTabCount; t++) {
		float ty = tab_y + t * (TAB_HEIGHT + TAB_GAP);
		if (t == selectedTab) {
			Render_quad_absolute(tab_x, ty, tab_x + TAB_WIDTH, ty + TAB_HEIGHT,
				TAB_SEL_BG_R, TAB_SEL_BG_G, TAB_SEL_BG_B, 0.9f);
			render_tab_border(tab_x, ty, TAB_WIDTH, TAB_HEIGHT);
		} else {
			Render_quad_absolute(tab_x, ty, tab_x + TAB_WIDTH, ty + TAB_HEIGHT,
				TAB_UNSEL_BG_R, TAB_UNSEL_BG_G, TAB_UNSEL_BG_B, 0.9f);
		}
	}

	/* ── Accordion title row backgrounds ── */
	float content_x = px + TAB_WIDTH + TAB_GAP * 2.0f;
	float content_top = py + TAB_GAP;
	float content_w = DLOG_WIDTH - TAB_WIDTH - TAB_GAP * 3.0f - SCROLLBAR_WIDTH;
	float content_h = DLOG_HEIGHT - TAB_GAP * 2.0f;

	int entry_count = count_entries_for_tab(selectedTab);
	float total_h = 0.0f;

	/* Calculate total content height */
	for (int i = 0; i < entry_count; i++) {
		total_h += TITLE_ROW_HEIGHT;
		if (expandedEntry == i) {
			const NarrativeEntry *e = get_entry_for_tab(selectedTab, i);
			if (e) total_h += measure_body_height(tr, e, content_w - INDICATOR_WIDTH - 4.0f);
		}
	}

	/* Clamp scroll */
	float max_scroll = total_h - content_h;
	if (max_scroll < 0.0f) max_scroll = 0.0f;
	if (scrollOffset > max_scroll) scrollOffset = max_scroll;

	/* Render title row quads */
	{
		float cursor_y = content_top - scrollOffset;
		for (int i = 0; i < entry_count; i++) {
			if (cursor_y + TITLE_ROW_HEIGHT > content_top && cursor_y < content_top + content_h) {
				/* Item background (subtle alternating) */
				Render_quad_absolute(content_x, cursor_y,
					content_x + content_w + SCROLLBAR_WIDTH, cursor_y + TITLE_ROW_HEIGHT,
					0.12f, 0.12f, 0.14f, (i % 2 == 0) ? 0.9f : 0.6f);
			}
			cursor_y += TITLE_ROW_HEIGHT;
			if (expandedEntry == i) {
				const NarrativeEntry *e = get_entry_for_tab(selectedTab, i);
				if (e) cursor_y += measure_body_height(tr, e, content_w - INDICATOR_WIDTH - 4.0f);
			}
		}
	}

	/* Scrollbar */
	if (total_h > content_h && max_scroll > 0.0f) {
		float sb_x = px + DLOG_WIDTH - SCROLLBAR_WIDTH - TAB_GAP;
		float track_y = content_top;
		float track_h = content_h;
		float thumb_h = (content_h / total_h) * track_h;
		if (thumb_h < 20.0f) thumb_h = 20.0f;
		float scroll_ratio = scrollOffset / max_scroll;
		float thumb_y = track_y + scroll_ratio * (track_h - thumb_h);

		Render_quad_absolute(sb_x, track_y, sb_x + SCROLLBAR_WIDTH, track_y + track_h,
			0.2f, 0.2f, 0.2f, 0.3f);
		Render_quad_absolute(sb_x, thumb_y, sb_x + SCROLLBAR_WIDTH, thumb_y + thumb_h,
			0.7f, 0.7f, 0.7f, 0.4f);
	}

	/* ── Flush geometry, switch to text ── */
	Render_flush(&proj, &ident);

	/* ── Title (floating above panel border, centered) ── */
	{
		const char *title = "DATA LOGS";
		float tw = Text_measure_width(tr, title);
		Text_render(tr, shaders, &proj, &ident, title,
			px + DLOG_WIDTH * 0.5f - tw * 0.5f, py - 5.0f,
			0.7f, 0.7f, 1.0f, 0.9f);
	}

	/* ── Tab labels ── */
	for (int t = 0; t < zoneTabCount; t++) {
		float ty = tab_y + t * (TAB_HEIGHT + TAB_GAP);
		float alpha = (t == selectedTab) ? 0.9f : 0.5f;
		/* Truncate long zone names */
		char label[32];
		strncpy(label, zoneTabs[t], 31);
		label[31] = '\0';
		float lw = Text_measure_width(tr, label);
		while (lw > TAB_WIDTH - 16.0f && strlen(label) > 4) {
			label[strlen(label) - 1] = '\0';
			lw = Text_measure_width(tr, label);
		}
		Text_render(tr, shaders, &proj, &ident, label,
			tab_x + 8.0f, ty + TAB_HEIGHT * 0.5f - 3.0f,
			1.0f, 1.0f, 1.0f, alpha);
	}

	/* ── Accordion entries (text) ── */
	{
		float cursor_y = content_top - scrollOffset;
		float text_x = content_x + INDICATOR_WIDTH;
		float body_x = content_x + INDICATOR_WIDTH + 4.0f;
		float body_w = content_w - INDICATOR_WIDTH - 4.0f;

		for (int i = 0; i < entry_count; i++) {
			const NarrativeEntry *e = get_entry_for_tab(selectedTab, i);
			if (!e) { cursor_y += TITLE_ROW_HEIGHT; continue; }

			/* Title row */
			if (cursor_y + TITLE_ROW_HEIGHT > content_top && cursor_y < content_top + content_h) {
				/* Expand/collapse indicator — Y aligned to tab text baseline */
				float text_y = cursor_y + TAB_HEIGHT * 0.5f - 3.0f;
				const char *indicator = (expandedEntry == i) ? "v" : ">";
				Text_render(tr, shaders, &proj, &ident, indicator,
					content_x + 4.0f, text_y,
					0.4f, 0.8f, 0.9f, 0.8f);

				/* Title text */
				Text_render(tr, shaders, &proj, &ident, e->title,
					text_x, text_y,
					0.9f, 0.9f, 1.0f, 0.95f);
			}
			cursor_y += TITLE_ROW_HEIGHT;

			/* Expanded body */
			if (expandedEntry == i) {
				/* Separator line under title */
				if (cursor_y > content_top && cursor_y < content_top + content_h) {
					Render_thick_line(body_x, cursor_y - 2.0f,
						body_x + body_w - 20.0f, cursor_y - 2.0f,
						1.0f, SEP_R, SEP_G, SEP_B, SEP_A);
					Render_flush(&proj, &ident);
				}
				cursor_y += 10.0f;

				render_body_text(tr, shaders, &proj, &ident, e,
					body_x, body_w,
					content_top, content_top + content_h,
					&cursor_y);
			}
		}
	}

	/* ── Key hints (below panel, matching catalog style) ── */
	Text_render(tr, shaders, &proj, &ident,
		"[L] Close    [Click] Expand/Collapse    [Scroll] Navigate",
		px + 10.0f, py + DLOG_HEIGHT + 15.0f,
		0.6f, 0.6f, 0.65f, 0.9f);
}
