#include "data_node.h"
#include "ship.h"
#include "render.h"
#include "view.h"
#include "color.h"
#include "text.h"
#include "graphics.h"
#include "mat4.h"
#include "narrative.h"
#include "audio.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#define DWELL_THRESHOLD_MS 800
#define FLASH_DURATION_MS 300
#define DISMISS_DELAY_MS 500
#define DATANODE_HALF_SIZE 50.0
#define DISK_RADIUS 25.0
#define HOLE_RATIO 0.4
#define SPIN_DEG_PER_SEC 180.0
#define NOTIFY_DURATION_MS 3000
#define NOTIFY_FADE_MS 1000
#define DISK_SEGMENTS 16
#define PI 3.14159265358979323846

#define MAX_COLLECTED_NODES 256

typedef enum {
	DATANODE_IDLE,
	DATANODE_CHARGING,
	DATANODE_COLLECTED,
	DATANODE_READING,
	DATANODE_DONE
} DataNodePhase;

typedef struct {
	bool active;
	Position position;
	char node_id[32];
	DataNodePhase phase;
	unsigned int anim_timer;
	unsigned int charge_timer;
	unsigned int flash_timer;
	unsigned int reading_timer;
	bool ship_inside;
} DataNodeState;

static DataNodeState nodes[DATANODE_COUNT];
static int nodeCount = 0;

/* Collection tracking (persists across zone transitions until saved) */
static char collectedIds[MAX_COLLECTED_NODES][32];
static int collectedCount = 0;

/* Active reading state */
static bool reading = false;
static const NarrativeEntry *readingEntry = NULL;
static float readingScroll = 0.0f;

/* Music ducking during voice playback */
static float duckLevel = 1.0f;   /* current music volume multiplier (0..1) */
#define DUCK_TARGET 0.30f         /* 30% music volume while voice plays */
#define DUCK_RAMP_SPEED 1.5f      /* units per second (~330ms ramp) */

/* SFX ducking during voice playback */
static float sfxDuckLevel = 1.0f;
static bool sfxDucking = false;
#define SFX_DUCK_TARGET 0.50f     /* 50% SFX volume while voice plays */

/* Notification */
static bool notifyActive = false;
static unsigned int notifyTimer = 0;

/* Audio */
static Mix_Chunk *sampleCollect = 0;

/* Voice playback */
#define VOICE_CHANNEL 2
static Mix_Chunk *voiceChunk = NULL;

void DataNode_initialize(Position position, const char *node_id)
{
	if (nodeCount >= DATANODE_COUNT) {
		printf("DataNode: max nodes (%d) reached\n", DATANODE_COUNT);
		return;
	}

	if (!sampleCollect)
		Audio_load_sample(&sampleCollect, "resources/sounds/data_collect.wav");

	DataNodeState *n = &nodes[nodeCount];
	memset(n, 0, sizeof(*n));
	n->active = true;
	n->position = position;
	strncpy(n->node_id, node_id, sizeof(n->node_id) - 1);
	n->phase = DataNode_is_collected(node_id) ? DATANODE_DONE : DATANODE_IDLE;

	nodeCount++;
}

void DataNode_cleanup(void)
{
	nodeCount = 0;
	reading = false;
	readingEntry = NULL;
	readingScroll = 0.0f;
	notifyActive = false;

	/* Stop and free any playing voice clip */
	Mix_HaltChannel(VOICE_CHANNEL);
	if (voiceChunk) {
		Mix_FreeChunk(voiceChunk);
		voiceChunk = NULL;
	}

	if (duckLevel < 1.0f) {
		duckLevel = 1.0f;
		Mix_VolumeMusic(MIX_MAX_VOLUME);
	}

	if (sfxDuckLevel < 1.0f || sfxDucking) {
		sfxDuckLevel = 1.0f;
		sfxDucking = false;
		for (int ch = 0; ch < 64; ch++) {
			if (ch == VOICE_CHANNEL) continue;
			Mix_Volume(ch, MIX_MAX_VOLUME);
		}
	}
}

void DataNode_refresh_phases(void)
{
	/* Sync node phases with collected state (e.g. after death rollback) */
	for (int i = 0; i < nodeCount; i++) {
		DataNodeState *n = &nodes[i];
		if (!n->active) continue;
		bool collected = DataNode_is_collected(n->node_id);
		if (collected && n->phase != DATANODE_DONE) {
			n->phase = DATANODE_DONE;
		} else if (!collected && n->phase == DATANODE_DONE) {
			n->phase = DATANODE_IDLE;
			n->anim_timer = 0;
			n->charge_timer = 0;
		}
	}
	/* Cancel any active reading overlay and voice */
	if (reading || voiceChunk) {
		reading = false;
		readingEntry = NULL;
		readingScroll = 0.0f;
		Mix_HaltChannel(VOICE_CHANNEL);
		if (voiceChunk) {
			Mix_FreeChunk(voiceChunk);
			voiceChunk = NULL;
		}
	}
}

static void collect_node(DataNodeState *n)
{
	if (collectedCount < MAX_COLLECTED_NODES) {
		strncpy(collectedIds[collectedCount], n->node_id, 31);
		collectedIds[collectedCount][31] = '\0';
		collectedCount++;
	}
}

static void begin_reading(const char *node_id)
{
	readingEntry = Narrative_get(node_id);
	if (readingEntry) {
		reading = true;
		readingScroll = 0.0f;

		/* Stop any still-playing voice from a previous node */
		if (voiceChunk) {
			Mix_HaltChannel(VOICE_CHANNEL);
			Mix_FreeChunk(voiceChunk);
			voiceChunk = NULL;
		}

		/* Play voice clip if entry has one */
		if (readingEntry->voice_path[0] != '\0') {
			voiceChunk = Mix_LoadWAV(readingEntry->voice_path);
			if (voiceChunk) {
				if (readingEntry->voice_gain != 1.0f)
					Audio_boost_sample(voiceChunk, readingEntry->voice_gain);
				Mix_PlayChannel(VOICE_CHANNEL, voiceChunk, 0);
			} else
				printf("DataNode: could not load voice %s\n", readingEntry->voice_path);
		}
	}
}

static void end_reading(void)
{
	reading = false;
	readingEntry = NULL;
	readingScroll = 0.0f;
	notifyActive = true;
	notifyTimer = 0;

	/* Voice clip keeps playing — music stays ducked until clip finishes.
	 * Cleanup happens in DataNode_update_all when Mix_Playing returns 0. */
}

void DataNode_update_all(unsigned int ticks)
{
	Position ship_pos = Ship_get_position();

	/* Notification timer */
	if (notifyActive) {
		notifyTimer += ticks;
		if (notifyTimer >= NOTIFY_DURATION_MS)
			notifyActive = false;
	}

	for (int i = 0; i < nodeCount; i++) {
		DataNodeState *n = &nodes[i];
		if (!n->active) continue;

		n->anim_timer += ticks;

		switch (n->phase) {
		case DATANODE_IDLE: {
			double dx = ship_pos.x - n->position.x;
			double dy = ship_pos.y - n->position.y;
			bool inside = (dx >= -DATANODE_HALF_SIZE && dx <= DATANODE_HALF_SIZE &&
			               dy >= -DATANODE_HALF_SIZE && dy <= DATANODE_HALF_SIZE);

			if (inside && !Ship_is_destroyed()) {
				n->phase = DATANODE_CHARGING;
				n->charge_timer = 0;
				n->ship_inside = true;
			}
			break;
		}
		case DATANODE_CHARGING: {
			double dx = ship_pos.x - n->position.x;
			double dy = ship_pos.y - n->position.y;
			bool inside = (dx >= -DATANODE_HALF_SIZE && dx <= DATANODE_HALF_SIZE &&
			               dy >= -DATANODE_HALF_SIZE && dy <= DATANODE_HALF_SIZE);

			if (!inside || Ship_is_destroyed()) {
				n->phase = DATANODE_IDLE;
				n->charge_timer = 0;
				n->ship_inside = false;
				break;
			}

			n->charge_timer += ticks;

			/* Pull ship toward center */
			float charge = (float)n->charge_timer / DWELL_THRESHOLD_MS;
			if (charge > 1.0f) charge = 1.0f;
			float pull = charge * charge;
			Position pulled;
			pulled.x = ship_pos.x + (n->position.x - ship_pos.x) * pull;
			pulled.y = ship_pos.y + (n->position.y - ship_pos.y) * pull;
			Ship_set_position(pulled);

			if (n->charge_timer >= DWELL_THRESHOLD_MS) {
				n->phase = DATANODE_COLLECTED;
				n->flash_timer = 0;
				collect_node(n);
				if (sampleCollect)
				Audio_play_sample(&sampleCollect);
			}
			break;
		}
		case DATANODE_COLLECTED: {
			n->flash_timer += ticks;
			if (n->flash_timer >= FLASH_DURATION_MS) {
				n->phase = DATANODE_READING;
				n->reading_timer = 0;
				begin_reading(n->node_id);
			}
			break;
		}
		case DATANODE_READING:
			n->reading_timer += ticks;
			break;
		case DATANODE_DONE:
			break;
		}
	}

	/* Free voice clip once it finishes playing (after overlay dismissed) */
	if (voiceChunk && !Mix_Playing(VOICE_CHANNEL)) {
		Mix_FreeChunk(voiceChunk);
		voiceChunk = NULL;
	}

	/* Duck music while reading overlay is open OR voice clip is still playing */
	float target = (reading || voiceChunk) ? DUCK_TARGET : 1.0f;
	if (duckLevel != target) {
		float dt = ticks / 1000.0f;
		float step = DUCK_RAMP_SPEED * dt;
		if (duckLevel > target) {
			duckLevel -= step;
			if (duckLevel < target) duckLevel = target;
		} else {
			duckLevel += step;
			if (duckLevel > target) duckLevel = target;
		}
		Mix_VolumeMusic((int)(duckLevel * MIX_MAX_VOLUME));
	}

	/* Duck SFX channels while voice is playing */
	float sfx_target = (reading || voiceChunk) ? SFX_DUCK_TARGET : 1.0f;
	if (sfxDuckLevel != sfx_target) {
		float dt = ticks / 1000.0f;
		float step = DUCK_RAMP_SPEED * dt;
		if (sfxDuckLevel > sfx_target) {
			sfxDuckLevel -= step;
			if (sfxDuckLevel < sfx_target) sfxDuckLevel = sfx_target;
		} else {
			sfxDuckLevel += step;
			if (sfxDuckLevel > sfx_target) sfxDuckLevel = sfx_target;
		}
	}
	if (sfxDuckLevel < 1.0f) {
		/* Apply to all channels except the voice channel */
		int sfx_vol = (int)(sfxDuckLevel * MIX_MAX_VOLUME);
		for (int ch = 0; ch < 64; ch++) {
			if (ch == VOICE_CHANNEL) continue;
			Mix_Volume(ch, sfx_vol);
		}
		sfxDucking = true;
	} else if (sfxDucking) {
		/* One final restore pass when ducking ends */
		for (int ch = 0; ch < 64; ch++) {
			if (ch == VOICE_CHANNEL) continue;
			Mix_Volume(ch, MIX_MAX_VOLUME);
		}
		sfxDucking = false;
	}
}

/* --- Rendering --- */

static void render_disk(Position pos, float spin_deg, float radius, float alpha)
{
	/* Outer white disk as ellipse — horizontal radius oscillates with spin */
	float spin_rad = (float)(spin_deg * PI / 180.0);
	float h_scale = (float)fabs(cos(spin_rad));
	if (h_scale < 0.15f) h_scale = 0.15f; /* clamp so disk never vanishes */
	float hr = radius * h_scale;
	float vr = radius;

	float cx = (float)pos.x;
	float cy = (float)pos.y;

	/* Outer disk: white with slight cyan tint */
	Render_filled_ellipse(cx, cy, hr, vr, DISK_SEGMENTS,
		0.9f, 0.95f, 1.0f, alpha);

	/* Inner black hole */
	float ihr = radius * HOLE_RATIO * h_scale;
	float ivr = radius * HOLE_RATIO;
	Render_filled_ellipse(cx, cy, ihr, ivr, DISK_SEGMENTS,
		0.0f, 0.0f, 0.0f, alpha);
}

void DataNode_render_all(void)
{
	for (int i = 0; i < nodeCount; i++) {
		DataNodeState *n = &nodes[i];
		if (!n->active) continue;

		float t = n->anim_timer / 1000.0f;

		switch (n->phase) {
		case DATANODE_IDLE: {
			float spin = (float)fmod(t * SPIN_DEG_PER_SEC, 360.0);
			float pulse = 0.7f + 0.3f * (0.5f + 0.5f * sinf(t * 2.0f));
			render_disk(n->position, spin, DISK_RADIUS, pulse);
			break;
		}
		case DATANODE_CHARGING: {
			/* Spin accelerates with charge */
			float charge = (float)n->charge_timer / DWELL_THRESHOLD_MS;
			if (charge > 1.0f) charge = 1.0f;
			float speed = SPIN_DEG_PER_SEC * (1.0f + charge * 3.0f);
			float spin = (float)fmod(t * speed, 360.0);
			render_disk(n->position, spin, DISK_RADIUS, 1.0f);
			break;
		}
		case DATANODE_COLLECTED: {
			/* White flash expanding ring */
			float progress = (float)n->flash_timer / FLASH_DURATION_MS;
			float fade = 1.0f - progress;
			float ring_radius = DISK_RADIUS * (1.0f + progress * 3.0f);
			float thickness = 4.0f * fade;
			ColorFloat flash = {1.0f, 1.0f, 1.0f, fade};
			float cx = (float)n->position.x;
			float cy = (float)n->position.y;
			for (int s = 0; s < DISK_SEGMENTS; s++) {
				float a0 = (float)(2.0 * PI * s / DISK_SEGMENTS);
				float a1 = (float)(2.0 * PI * (s + 1) / DISK_SEGMENTS);
				float x0 = cx + ring_radius * cosf(a0);
				float y0 = cy + ring_radius * sinf(a0);
				float x1 = cx + ring_radius * cosf(a1);
				float y1 = cy + ring_radius * sinf(a1);
				Render_thick_line(x0, y0, x1, y1, thickness,
					flash.red, flash.green, flash.blue, flash.alpha);
			}
			break;
		}
		case DATANODE_READING:
		case DATANODE_DONE:
			break;
		}
	}
}

void DataNode_render_bloom_source(void)
{
	for (int i = 0; i < nodeCount; i++) {
		DataNodeState *n = &nodes[i];
		if (!n->active || n->phase == DATANODE_DONE || n->phase == DATANODE_READING)
			continue;

		if (n->phase == DATANODE_IDLE || n->phase == DATANODE_CHARGING) {
			Render_point(&n->position, 8.0f,
				&(ColorFloat){0.9f, 0.95f, 1.0f, 0.8f});
		}
		else if (n->phase == DATANODE_COLLECTED) {
			float fade = 1.0f - (float)n->flash_timer / FLASH_DURATION_MS;
			Render_point(&n->position, 12.0f,
				&(ColorFloat){1.0f, 1.0f, 1.0f, fade});
		}
	}
}

void DataNode_render_notification(const Screen *screen)
{
	if (!notifyActive) return;

	unsigned int remaining = NOTIFY_DURATION_MS - notifyTimer;
	float alpha = 1.0f;
	if (remaining < NOTIFY_FADE_MS)
		alpha = (float)remaining / NOTIFY_FADE_MS;

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();

	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	Render_flush(&proj, &ident);

	const char *msg = ">> Data Transfer Complete <<";
	float tw = Text_measure_width(tr, msg);
	float nx = screen->width * 0.5f - tw * 0.5f;
	float ny = screen->height * 0.3f;
	Text_render(tr, shaders, &proj, &ident, msg, nx, ny,
		1.0f, 1.0f, 1.0f, alpha);
}

/* --- Message overlay --- */

#define OVERLAY_MAX_WIDTH 550.0f
#define OVERLAY_PADDING 20.0f
#define OVERLAY_MAX_HEIGHT_RATIO 0.6f
#define SCROLLBAR_WIDTH 8.0f

/* Spacing: title sits at top padding, separator tight below it,
 * then breathing room before body text begins */
#define OV_TITLE_H 18.0f    /* title baseline to separator */
#define OV_SEP_GAP  4.0f    /* separator line to breathing room */
#define OV_BODY_GAP 14.0f   /* breathing room before body text */

void DataNode_render_overlay(const Screen *screen)
{
	if (!reading || !readingEntry) return;

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();

	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	float max_h = screen->height * OVERLAY_MAX_HEIGHT_RATIO;
	float content_w = OVERLAY_MAX_WIDTH - OVERLAY_PADDING * 2.0f;
	float line_h = 18.0f;
	float header_h = OV_TITLE_H + OV_SEP_GAP + OV_BODY_GAP;

	/* Measure body text height */
	float body_h = 0.0f;
	{
		const char *p = readingEntry->body;
		char linebuf[512];
		int linelen = 0;
		while (*p) {
			if (*p == '\n') {
				body_h += line_h;
				linelen = 0;
				p++;
				continue;
			}
			linebuf[linelen++] = *p++;
			linebuf[linelen] = '\0';
			if (linelen >= 510 || Text_measure_width(tr, linebuf) > content_w) {
				int wrap = linelen - 1;
				while (wrap > 0 && linebuf[wrap] != ' ') wrap--;
				if (wrap > 0) {
					char overflow[512];
					strncpy(overflow, linebuf + wrap + 1, 511);
					overflow[511] = '\0';
					body_h += line_h;
					strcpy(linebuf, overflow);
					linelen = (int)strlen(linebuf);
				} else {
					body_h += line_h;
					linelen = 0;
				}
			}
		}
		if (linelen > 0) body_h += line_h;
	}

	float total_content_h = header_h + body_h + OVERLAY_PADDING;
	float overlay_h = total_content_h + OVERLAY_PADDING * 2.0f;
	if (overlay_h > max_h) overlay_h = max_h;

	float overlay_w = OVERLAY_MAX_WIDTH;
	float ox = (screen->width - overlay_w) * 0.5f;
	float oy = (screen->height - overlay_h) * 0.5f - 20.0f;

	/* Background (matched to catalog/data logs) */
	Render_quad_absolute(ox, oy, ox + overlay_w, oy + overlay_h,
		0.08f, 0.08f, 0.12f, 0.95f);

	/* Border (matched to catalog) */
	Render_thick_line(ox, oy, ox + overlay_w, oy,
		1.0f, 0.3f, 0.3f, 0.3f, 0.8f);
	Render_thick_line(ox + overlay_w, oy, ox + overlay_w, oy + overlay_h,
		1.0f, 0.3f, 0.3f, 0.3f, 0.8f);
	Render_thick_line(ox + overlay_w, oy + overlay_h, ox, oy + overlay_h,
		1.0f, 0.3f, 0.3f, 0.3f, 0.8f);
	Render_thick_line(ox, oy + overlay_h, ox, oy,
		1.0f, 0.3f, 0.3f, 0.3f, 0.8f);

	/* Separator line (tight below title) */
	float tx = ox + OVERLAY_PADDING;
	float sep_y = oy + OVERLAY_PADDING + OV_TITLE_H;
	Render_thick_line(tx, sep_y, tx + content_w, sep_y,
		1.0f, 0.25f, 0.25f, 0.3f, 0.6f);

	Render_flush(&proj, &ident);

	/* Title (floating above border, centered, catalog style) */
	{
		float tw = Text_measure_width(tr, readingEntry->title);
		Text_render(tr, shaders, &proj, &ident, readingEntry->title,
			ox + overlay_w * 0.5f - tw * 0.5f, oy - 5.0f,
			0.7f, 0.7f, 1.0f, 0.9f);
	}

	/* Body text with breathing room after separator */
	float body_start_y = oy + OVERLAY_PADDING + header_h;
	float visible_h = overlay_h - OVERLAY_PADDING * 2.0f - header_h;
	{
		const char *p = readingEntry->body;
		char linebuf[512];
		int linelen = 0;
		float cur_y = body_start_y - readingScroll;

		while (*p) {
			if (*p == '\n') {
				linebuf[linelen] = '\0';
				if (cur_y >= body_start_y - line_h && cur_y <= body_start_y + visible_h) {
					Text_render(tr, shaders, &proj, &ident, linebuf, tx, cur_y,
						0.75f, 0.75f, 0.8f, 0.75f);
				}
				cur_y += line_h;
				linelen = 0;
				p++;
				continue;
			}

			linebuf[linelen++] = *p++;
			linebuf[linelen] = '\0';

			if (linelen >= 510 || Text_measure_width(tr, linebuf) > content_w) {
				int wrap = linelen - 1;
				while (wrap > 0 && linebuf[wrap] != ' ') wrap--;
				if (wrap > 0) {
					char overflow[512];
					strncpy(overflow, linebuf + wrap + 1, 511);
					overflow[511] = '\0';
					linebuf[wrap] = '\0';

					if (cur_y >= body_start_y - line_h && cur_y <= body_start_y + visible_h) {
						Text_render(tr, shaders, &proj, &ident, linebuf, tx, cur_y,
							0.75f, 0.75f, 0.8f, 0.75f);
					}
					cur_y += line_h;
					strcpy(linebuf, overflow);
					linelen = (int)strlen(linebuf);
				} else {
					if (cur_y >= body_start_y - line_h && cur_y <= body_start_y + visible_h) {
						Text_render(tr, shaders, &proj, &ident, linebuf, tx, cur_y,
							0.75f, 0.75f, 0.8f, 0.75f);
					}
					cur_y += line_h;
					linelen = 0;
				}
			}
		}
		if (linelen > 0) {
			linebuf[linelen] = '\0';
			if (cur_y >= body_start_y - line_h && cur_y <= body_start_y + visible_h) {
				Text_render(tr, shaders, &proj, &ident, linebuf, tx, cur_y,
					0.75f, 0.75f, 0.8f, 0.75f);
			}
		}

		/* Scrollbar */
		if (total_content_h > overlay_h) {
			float max_scroll = total_content_h - overlay_h + OVERLAY_PADDING * 2.0f;
			float scroll_ratio = readingScroll / max_scroll;
			float track_h = overlay_h - OVERLAY_PADDING * 2.0f;
			float thumb_h = (visible_h / total_content_h) * track_h;
			if (thumb_h < 20.0f) thumb_h = 20.0f;
			float thumb_y = oy + OVERLAY_PADDING + scroll_ratio * (track_h - thumb_h);
			float sbx = ox + overlay_w - SCROLLBAR_WIDTH - 4.0f;

			Render_quad_absolute(sbx, oy + OVERLAY_PADDING,
				sbx + SCROLLBAR_WIDTH, oy + overlay_h - OVERLAY_PADDING,
				0.2f, 0.2f, 0.2f, 0.3f);
			Render_quad_absolute(sbx, thumb_y,
				sbx + SCROLLBAR_WIDTH, thumb_y + thumb_h,
				0.7f, 0.7f, 0.7f, 0.4f);
			Render_flush(&proj, &ident);
		}
	}

	/* Dismiss hint (floating below window, matching catalog key hints) */
	float dismiss_alpha = 0.0f;
	for (int i = 0; i < nodeCount; i++) {
		if (nodes[i].phase == DATANODE_READING && nodes[i].reading_timer > DISMISS_DELAY_MS) {
			float fade_in = (float)(nodes[i].reading_timer - DISMISS_DELAY_MS) / 500.0f;
			if (fade_in > 1.0f) fade_in = 1.0f;
			dismiss_alpha = fade_in * 0.9f;
			break;
		}
	}
	if (reading && dismiss_alpha == 0.0f)
		dismiss_alpha = 0.9f;

	if (dismiss_alpha > 0.0f) {
		const char *hint = "[Any key to continue]";
		float hw = Text_measure_width(tr, hint);
		Text_render(tr, shaders, &proj, &ident, hint,
			ox + overlay_w - hw - 10.0f,
			oy + overlay_h + 15.0f,
			0.6f, 0.6f, 0.65f, dismiss_alpha);
	}
}

bool DataNode_is_reading(void)
{
	return reading;
}

bool DataNode_dismiss_reading(void)
{
	if (!reading) return false;

	/* Find the node in READING phase and advance to DONE */
	for (int i = 0; i < nodeCount; i++) {
		if (nodes[i].phase == DATANODE_READING) {
			/* Require dismiss delay before accepting input */
			if (nodes[i].reading_timer < DISMISS_DELAY_MS)
				return false;
			nodes[i].phase = DATANODE_DONE;
			break;
		}
	}

	end_reading();
	return true;
}

void DataNode_trigger_transfer(const char *node_id)
{
	if (DataNode_is_collected(node_id))
		return;

	/* Mark as collected */
	if (collectedCount < MAX_COLLECTED_NODES) {
		strncpy(collectedIds[collectedCount], node_id, 31);
		collectedIds[collectedCount][31] = '\0';
		collectedCount++;
	}

	/* Begin reading immediately */
	begin_reading(node_id);
	if (sampleCollect)
		Audio_play_sample(&sampleCollect);
}

/* --- Collection tracking --- */

bool DataNode_is_collected(const char *node_id)
{
	for (int i = 0; i < collectedCount; i++) {
		if (strcmp(collectedIds[i], node_id) == 0)
			return true;
	}
	return false;
}

int DataNode_collected_count(void)
{
	return collectedCount;
}

const char *DataNode_collected_id(int index)
{
	if (index < 0 || index >= collectedCount)
		return NULL;
	return collectedIds[index];
}

void DataNode_mark_collected(const char *node_id)
{
	if (DataNode_is_collected(node_id))
		return;
	if (collectedCount < MAX_COLLECTED_NODES) {
		strncpy(collectedIds[collectedCount], node_id, 31);
		collectedIds[collectedCount][31] = '\0';
		collectedCount++;
	}
}

void DataNode_clear_collected(void)
{
	collectedCount = 0;
}

/* --- God mode labels --- */

void DataNode_render_god_labels(void)
{
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 ui_proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();
	Screen screen = Graphics_get_screen();
	Mat4 view = View_get_transform(&screen);

	for (int i = 0; i < nodeCount; i++) {
		DataNodeState *n = &nodes[i];
		if (!n->active) continue;

		float sx, sy;
		Mat4_transform_point(&view, (float)n->position.x, (float)n->position.y, &sx, &sy);
		sx = sx * ((float)screen.width / screen.norm_w);
		sy = sy * ((float)screen.height / screen.norm_h);
		sy = (float)screen.height - sy;

		char buf[64];
		snprintf(buf, sizeof(buf), "[DN:%s]", n->node_id);
		Text_render(tr, shaders, &ui_proj, &ident,
			buf, sx - 40.0f, sy - 55.0f,
			1.0f, 0.9f, 0.2f, 0.9f);
	}
}

/* --- Minimap --- */

void DataNode_render_minimap(float ship_x, float ship_y,
	float screen_x, float screen_y, float size, float range)
{
	float half_range = range * 0.5f;
	float half_size = size * 0.5f;

	for (int i = 0; i < nodeCount; i++) {
		DataNodeState *n = &nodes[i];
		if (!n->active) continue;

		float dx = (float)n->position.x - ship_x;
		float dy = (float)n->position.y - ship_y;

		if (dx < -half_range || dx > half_range ||
		    dy < -half_range || dy > half_range)
			continue;

		float mx = screen_x + half_size + (dx / half_range) * half_size;
		float my = screen_y + half_size - (dy / half_range) * half_size;

		float s = 2.5f;
		if (n->phase == DATANODE_DONE) {
			/* Grey dot — collected */
			Render_quad_absolute(mx - s, my - s, mx + s, my + s,
				0.4f, 0.4f, 0.4f, 0.7f);
		} else {
			/* Yellow dot — uncollected */
			Render_quad_absolute(mx - s, my - s, mx + s, my + s,
				1.0f, 0.9f, 0.2f, 1.0f);
		}
	}
}
