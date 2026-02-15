#include "savepoint.h"
#include "ship.h"
#include "render.h"
#include "view.h"
#include "text.h"
#include "graphics.h"
#include "mat4.h"
#include "zone.h"
#include "audio.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>

#define DWELL_THRESHOLD_MS 1000
#define FLASH_DURATION_MS 450
#define FLASH_COUNT 3
#define NUM_DOTS 8
#define DOT_RADIUS 100.0f
#define DOT_SIZE 5.0f
#define MAX_SPIN_SPEED (2.0f * 2.0f * M_PI) /* 2 rev/sec */
#define IDLE_SPIN_SPEED (0.15f * 2.0f * M_PI) /* 0.15 rev/sec */

#define NOTIFY_DURATION_MS 3000
#define NOTIFY_FADE_MS 1000

#define SAVEPOINT_CHARGE_CHANNEL 4

typedef enum {
	SAVEPOINT_IDLE,
	SAVEPOINT_CHARGING,
	SAVEPOINT_SAVING,
	SAVEPOINT_DEACTIVATED
} SavepointPhase;

typedef struct {
	bool active;
	Position position;
	char id[32];
	unsigned int dwell_timer;
	unsigned int anim_timer;
	unsigned int flash_timer;
	bool ship_inside;
	SavepointPhase phase;
	float spin_angle;       /* current rotation angle in radians */
	bool charge_sound_playing;
} SavepointState;

static SavepointState savepoints[SAVEPOINT_COUNT];
static PlaceableComponent placeables[SAVEPOINT_COUNT];
static Entity *entityRefs[SAVEPOINT_COUNT];
static int savepointCount = 0;

static RenderableComponent renderable = {Savepoint_render};

/* Global checkpoint */
static SaveCheckpoint checkpoint = {false};

/* Audio */
static Mix_Chunk *chargeLoopSample = 0;
static Mix_Chunk *flashSample = 0;
static bool audioLoaded = false;

/* Save notification */
static bool notifyActive = false;
static unsigned int notifyTimer = 0;

static void boost_sample(Mix_Chunk *chunk, float gain)
{
	Sint16 *samples = (Sint16 *)chunk->abuf;
	int count = chunk->alen / sizeof(Sint16);
	for (int i = 0; i < count; i++) {
		int val = (int)(samples[i] * gain);
		if (val > 32767) val = 32767;
		if (val < -32768) val = -32768;
		samples[i] = (Sint16)val;
	}
}

static void load_audio(void)
{
	if (audioLoaded) return;
	Audio_load_sample(&chargeLoopSample, "resources/sounds/refill_loop.wav");
	Audio_load_sample(&flashSample, "resources/sounds/refill_start.wav");
	boost_sample(chargeLoopSample, 4.0f);
	boost_sample(flashSample, 4.0f);
	Mix_VolumeChunk(chargeLoopSample, MIX_MAX_VOLUME);
	Mix_VolumeChunk(flashSample, MIX_MAX_VOLUME);
	audioLoaded = true;
}

static void do_save(SavepointState *sp)
{
	const Zone *z = Zone_get();

	checkpoint.valid = true;
	strncpy(checkpoint.zone_path, z->filepath, sizeof(checkpoint.zone_path) - 1);
	checkpoint.zone_path[sizeof(checkpoint.zone_path) - 1] = '\0';
	strncpy(checkpoint.savepoint_id, sp->id, sizeof(checkpoint.savepoint_id) - 1);
	checkpoint.savepoint_id[sizeof(checkpoint.savepoint_id) - 1] = '\0';
	checkpoint.position = sp->position;

	for (int i = 0; i < SUB_ID_COUNT; i++) {
		checkpoint.unlocked[i] = Progression_is_unlocked(i);
		checkpoint.discovered[i] = Progression_is_discovered(i);
	}
	for (int i = 0; i < FRAG_TYPE_COUNT; i++)
		checkpoint.fragment_counts[i] = Fragment_get_count(i);
	checkpoint.skillbar = Skillbar_snapshot();

	/* Write to disk */
	/* Create save directory if needed */
	mkdir("./save", 0755);

	FILE *f = fopen(SAVE_FILE_PATH, "w");
	if (!f) {
		printf("Savepoint: failed to write save file\n");
		return;
	}

	fprintf(f, "zone %s\n", checkpoint.zone_path);
	fprintf(f, "savepoint %s\n", checkpoint.savepoint_id);
	fprintf(f, "position %.1f %.1f\n", checkpoint.position.x, checkpoint.position.y);

	fprintf(f, "unlocked");
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (checkpoint.unlocked[i])
			fprintf(f, " %s", Skillbar_get_sub_name(i));
	}
	fprintf(f, "\n");

	fprintf(f, "discovered");
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (checkpoint.discovered[i])
			fprintf(f, " %s", Skillbar_get_sub_name(i));
	}
	fprintf(f, "\n");

	fprintf(f, "fragments");
	{
		static const char *frag_names[] = {"mine", "elite", "hunter", "seeker", "mend", "aegis"};
		for (int i = 0; i < FRAG_TYPE_COUNT; i++) {
			if (checkpoint.fragment_counts[i] > 0)
				fprintf(f, " %s:%d", frag_names[i], checkpoint.fragment_counts[i]);
		}
	}
	fprintf(f, "\n");

	fprintf(f, "skillbar");
	for (int s = 0; s < SKILLBAR_SLOTS; s++) {
		int sub = Skillbar_get_slot_sub(s);
		if (sub != SUB_NONE)
			fprintf(f, " %d:%s", s, Skillbar_get_sub_name(sub));
	}
	fprintf(f, "\n");

	fclose(f);
	printf("Savepoint: saved at '%s' in %s\n", sp->id, checkpoint.zone_path);

	notifyActive = true;
	notifyTimer = 0;
}

static int find_sub_by_name(const char *name)
{
	for (int i = 0; i < SUB_ID_COUNT; i++) {
		if (strcmp(Skillbar_get_sub_name(i), name) == 0)
			return i;
	}
	return -1;
}

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

void Savepoint_initialize(Position position, const char *id)
{
	if (savepointCount >= SAVEPOINT_COUNT) {
		printf("Savepoint_initialize: max savepoints reached\n");
		return;
	}

	load_audio();

	SavepointState *sp = &savepoints[savepointCount];
	sp->active = true;
	sp->position = position;
	strncpy(sp->id, id, sizeof(sp->id) - 1);
	sp->id[sizeof(sp->id) - 1] = '\0';
	sp->dwell_timer = 0;
	sp->anim_timer = 0;
	sp->flash_timer = 0;
	sp->ship_inside = false;
	sp->phase = SAVEPOINT_IDLE;
	sp->spin_angle = 0.0f;
	sp->charge_sound_playing = false;

	placeables[savepointCount].position = position;
	placeables[savepointCount].heading = 0.0;

	Entity entity = Entity_initialize_entity();
	entity.state = sp;
	entity.placeable = &placeables[savepointCount];
	entity.renderable = &renderable;

	entityRefs[savepointCount] = Entity_add_entity(entity);

	savepointCount++;
}

void Savepoint_cleanup(void)
{
	/* Stop charging sound if playing */
	for (int i = 0; i < savepointCount; i++) {
		if (savepoints[i].charge_sound_playing) {
			Mix_HaltChannel(SAVEPOINT_CHARGE_CHANNEL);
			savepoints[i].charge_sound_playing = false;
		}
		if (entityRefs[i]) {
			entityRefs[i]->empty = true;
			entityRefs[i] = NULL;
		}
	}
	savepointCount = 0;
	notifyActive = false;
	notifyTimer = 0;
}

void Savepoint_update_all(unsigned int ticks)
{
	/* Notification timer */
	if (notifyActive) {
		notifyTimer += ticks;
		if (notifyTimer >= NOTIFY_DURATION_MS)
			notifyActive = false;
	}

	Position ship_pos = Ship_get_position();

	for (int i = 0; i < savepointCount; i++) {
		SavepointState *sp = &savepoints[i];
		if (!sp->active) continue;

		sp->anim_timer += ticks;

		/* Point-in-AABB test */
		double dx = ship_pos.x - sp->position.x;
		double dy = ship_pos.y - sp->position.y;
		bool inside = (dx >= -SAVEPOINT_HALF_SIZE && dx <= SAVEPOINT_HALF_SIZE &&
		               dy >= -SAVEPOINT_HALF_SIZE && dy <= SAVEPOINT_HALF_SIZE);

		switch (sp->phase) {
		case SAVEPOINT_IDLE:
			sp->spin_angle += IDLE_SPIN_SPEED * (ticks / 1000.0f);
			if (inside && !Ship_is_destroyed()) {
				sp->ship_inside = true;
				sp->phase = SAVEPOINT_CHARGING;
				sp->dwell_timer = 0;
			}
			break;

		case SAVEPOINT_CHARGING:
			if (!inside || Ship_is_destroyed()) {
				/* Left or died — cancel */
				sp->ship_inside = false;
				sp->phase = SAVEPOINT_IDLE;
				sp->dwell_timer = 0;
				sp->spin_angle = 0.0f;
				if (sp->charge_sound_playing) {
					Audio_fade_out_channel(SAVEPOINT_CHARGE_CHANNEL, 200);
					sp->charge_sound_playing = false;
				}
				break;
			}

			sp->dwell_timer += ticks;

			/* Start charge sound */
			if (!sp->charge_sound_playing) {
				Audio_loop_sample_on_channel(&chargeLoopSample, SAVEPOINT_CHARGE_CHANNEL);
				sp->charge_sound_playing = true;
			}

			/* Pull ship toward center */
			{
				float charge = (float)sp->dwell_timer / DWELL_THRESHOLD_MS;
				if (charge > 1.0f) charge = 1.0f;
				float pull = charge * charge;
				Position pulled;
				pulled.x = ship_pos.x + (sp->position.x - ship_pos.x) * pull;
				pulled.y = ship_pos.y + (sp->position.y - ship_pos.y) * pull;
				Ship_set_position(pulled);
			}

			/* Spin dots */
			{
				float charge = (float)sp->dwell_timer / DWELL_THRESHOLD_MS;
				if (charge > 1.0f) charge = 1.0f;
				float speed = MAX_SPIN_SPEED * charge * charge;
				sp->spin_angle += speed * (ticks / 1000.0f);
			}

			if (sp->dwell_timer >= DWELL_THRESHOLD_MS) {
				/* Transition to saving */
				sp->phase = SAVEPOINT_SAVING;
				sp->flash_timer = 0;

				/* Stop charge sound, play flash sound */
				Mix_HaltChannel(SAVEPOINT_CHARGE_CHANNEL);
				sp->charge_sound_playing = false;
				Audio_play_sample(&flashSample);

				/* Execute save */
				do_save(sp);
			}
			break;

		case SAVEPOINT_SAVING:
			sp->flash_timer += ticks;

			if (sp->flash_timer >= FLASH_DURATION_MS) {
				sp->phase = SAVEPOINT_DEACTIVATED;
				sp->spin_angle = 0.0f;
			}
			break;

		case SAVEPOINT_DEACTIVATED:
			if (!inside) {
				sp->ship_inside = false;
				sp->phase = SAVEPOINT_IDLE;
			}
			break;
		}
	}
}

static void render_dots(const SavepointState *sp, float radius, float dot_size,
	float r, float g, float b, float a)
{
	for (int d = 0; d < NUM_DOTS; d++) {
		float angle = sp->spin_angle + d * (2.0f * M_PI / NUM_DOTS);
		float dx = cosf(angle) * radius;
		float dy = sinf(angle) * radius;
		Position dot_pos = {sp->position.x + dx, sp->position.y + dy};
		Rectangle dot_rect = {-dot_size, dot_size, dot_size, -dot_size};
		ColorFloat dot_color = {r, g, b, a};
		Render_quad(&dot_pos, 0.0, dot_rect, &dot_color);
	}
}

void Savepoint_render(const void *state, const PlaceableComponent *placeable)
{
	const SavepointState *sp = (const SavepointState *)state;
	if (!sp->active) return;

	float alpha_pulse = 0.7f + 0.1f * sinf(sp->anim_timer / 1000.0f * 2.0f);

	switch (sp->phase) {
	case SAVEPOINT_IDLE:
		render_dots(sp, DOT_RADIUS, DOT_SIZE,
			0.0f, 0.9f, 0.9f, alpha_pulse);
		break;

	case SAVEPOINT_CHARGING: {
		float charge = (float)sp->dwell_timer / DWELL_THRESHOLD_MS;
		if (charge > 1.0f) charge = 1.0f;
		/* Lerp cyan -> white */
		float r = charge;
		float g = 0.9f + 0.1f * charge;
		float b = 0.9f + 0.1f * charge;
		float a = 0.7f + 0.3f * charge;
		render_dots(sp, DOT_RADIUS, DOT_SIZE, r, g, b, a);
		break;
	}

	case SAVEPOINT_SAVING: {
		/* 3 flashes over FLASH_DURATION_MS */
		float flash_t = (float)sp->flash_timer / FLASH_DURATION_MS;
		float cycle = flash_t * FLASH_COUNT;
		float frac = cycle - (int)cycle;
		float brightness = (frac < 0.5f) ? 1.0f : 0.3f;
		render_dots(sp, DOT_RADIUS, DOT_SIZE,
			brightness, brightness, brightness, 1.0f);
		break;
	}

	case SAVEPOINT_DEACTIVATED:
		render_dots(sp, DOT_RADIUS, DOT_SIZE,
			0.0f, 0.9f, 0.9f, 0.4f);
		break;
	}
}

void Savepoint_render_bloom_source(void)
{
	for (int i = 0; i < savepointCount; i++) {
		SavepointState *sp = &savepoints[i];
		if (!sp->active) continue;

		float bloom_alpha = 0.4f;
		float bloom_size = DOT_SIZE + 2.0f;

		if (sp->phase == SAVEPOINT_CHARGING) {
			float charge = (float)sp->dwell_timer / DWELL_THRESHOLD_MS;
			if (charge > 1.0f) charge = 1.0f;
			bloom_alpha = 0.4f + 0.6f * charge;
		} else if (sp->phase == SAVEPOINT_SAVING) {
			bloom_alpha = 0.9f;
		}

		for (int d = 0; d < NUM_DOTS; d++) {
			float angle = sp->spin_angle + d * (2.0f * M_PI / NUM_DOTS);
			float dx = cosf(angle) * DOT_RADIUS;
			float dy = sinf(angle) * DOT_RADIUS;
			Position dot_pos = {sp->position.x + dx, sp->position.y + dy};
			Rectangle dot_rect = {-bloom_size, bloom_size, bloom_size, -bloom_size};
			ColorFloat dot_color = {0.0f, 0.9f, 0.9f, bloom_alpha};
			Render_quad(&dot_pos, 0.0, dot_rect, &dot_color);
		}
	}
}

void Savepoint_render_god_labels(void)
{
	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 ui_proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();
	Screen screen = Graphics_get_screen();
	Mat4 view = View_get_transform(&screen);

	for (int i = 0; i < savepointCount; i++) {
		SavepointState *sp = &savepoints[i];
		if (!sp->active) continue;

		/* World to screen coordinates */
		float sx, sy;
		Mat4_transform_point(&view, (float)sp->position.x, (float)sp->position.y, &sx, &sy);
		sy = (float)screen.height - sy;

		char buf[128];
		snprintf(buf, sizeof(buf), "[SAVE:%s]", sp->id);
		Text_render(tr, shaders, &ui_proj, &ident,
			buf, sx - 40.0f, sy - 55.0f,
			0.0f, 1.0f, 0.5f, 0.9f);
	}
}

void Savepoint_render_notification(const Screen *screen)
{
	if (!notifyActive) return;

	TextRenderer *tr = Graphics_get_text_renderer();
	Shaders *shaders = Graphics_get_shaders();
	Mat4 proj = Graphics_get_ui_projection();
	Mat4 ident = Mat4_identity();

	float alpha = 1.0f;
	unsigned int remaining = NOTIFY_DURATION_MS - notifyTimer;
	if (remaining < NOTIFY_FADE_MS)
		alpha = (float)remaining / NOTIFY_FADE_MS;

	const char *msg = ">> State Saved Successfully <<";
	float tw = text_width(tr, msg);
	float nx = (float)screen->width * 0.5f - tw * 0.5f;
	float ny = (float)screen->height * 0.3f;

	Text_render(tr, shaders, &proj, &ident,
		msg, nx, ny,
		1.0f, 1.0f, 1.0f, alpha);
}

const SaveCheckpoint *Savepoint_get_checkpoint(void)
{
	return &checkpoint;
}

bool Savepoint_has_save_file(void)
{
	FILE *f = fopen(SAVE_FILE_PATH, "r");
	if (f) {
		fclose(f);
		return true;
	}
	return false;
}

void Savepoint_delete_save_file(void)
{
	remove(SAVE_FILE_PATH);
	memset(&checkpoint, 0, sizeof(checkpoint));
	checkpoint.valid = false;
}

bool Savepoint_load_from_disk(void)
{
	FILE *f = fopen(SAVE_FILE_PATH, "r");
	if (!f) return false;

	memset(&checkpoint, 0, sizeof(checkpoint));

	/* Initialize skillbar slots to SUB_NONE */
	for (int i = 0; i < SKILLBAR_SLOTS; i++)
		checkpoint.skillbar.slots[i] = SUB_NONE;
	for (int i = 0; i < SUB_TYPE_COUNT; i++)
		checkpoint.skillbar.active_sub[i] = SUB_NONE;

	char line[512];
	while (fgets(line, sizeof(line), f)) {
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
		if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';
		if (len == 0) continue;

		if (strncmp(line, "zone ", 5) == 0) {
			strncpy(checkpoint.zone_path, line + 5, sizeof(checkpoint.zone_path) - 1);
		}
		else if (strncmp(line, "savepoint ", 10) == 0) {
			strncpy(checkpoint.savepoint_id, line + 10, sizeof(checkpoint.savepoint_id) - 1);
		}
		else if (strncmp(line, "position ", 9) == 0) {
			sscanf(line + 9, "%lf %lf", &checkpoint.position.x, &checkpoint.position.y);
		}
		else if (strncmp(line, "unlocked ", 9) == 0) {
			char *tok = strtok(line + 9, " ");
			while (tok) {
				int id = find_sub_by_name(tok);
				if (id >= 0) checkpoint.unlocked[id] = true;
				tok = strtok(NULL, " ");
			}
		}
		else if (strncmp(line, "discovered ", 11) == 0) {
			char *tok = strtok(line + 11, " ");
			while (tok) {
				int id = find_sub_by_name(tok);
				if (id >= 0) checkpoint.discovered[id] = true;
				tok = strtok(NULL, " ");
			}
		}
		else if (strncmp(line, "fragments ", 10) == 0) {
			static const char *frag_names[] = {"mine", "elite", "hunter", "seeker", "mend", "aegis"};
			char *tok = strtok(line + 10, " ");
			while (tok) {
				char name[32];
				int count;
				if (sscanf(tok, "%31[^:]:%d", name, &count) == 2) {
					for (int i = 0; i < FRAG_TYPE_COUNT; i++) {
						if (strcmp(name, frag_names[i]) == 0) {
							checkpoint.fragment_counts[i] = count;
							break;
						}
					}
				}
				tok = strtok(NULL, " ");
			}
		}
		else if (strncmp(line, "skillbar ", 9) == 0) {
			char *tok = strtok(line + 9, " ");
			while (tok) {
				int slot;
				char sub_name[32];
				if (sscanf(tok, "%d:%31s", &slot, sub_name) == 2) {
					int id = find_sub_by_name(sub_name);
					if (id >= 0 && slot >= 0 && slot < SKILLBAR_SLOTS) {
						checkpoint.skillbar.slots[slot] = id;
						SubroutineType type = Skillbar_get_sub_type(id);
						checkpoint.skillbar.active_sub[type] = id;
					}
				}
				tok = strtok(NULL, " ");
			}
		}
	}

	fclose(f);
	checkpoint.valid = true;
	printf("Savepoint: loaded checkpoint from disk (%s @ %s)\n",
		checkpoint.savepoint_id, checkpoint.zone_path);
	return true;
}

void Savepoint_suppress_by_id(const char *savepoint_id)
{
	for (int i = 0; i < savepointCount; i++) {
		if (savepoints[i].active && strcmp(savepoints[i].id, savepoint_id) == 0) {
			savepoints[i].phase = SAVEPOINT_DEACTIVATED;
			savepoints[i].dwell_timer = 0;
			savepoints[i].ship_inside = true;
			savepoints[i].spin_angle = 0.0f;
			if (savepoints[i].charge_sound_playing) {
				Mix_HaltChannel(SAVEPOINT_CHARGE_CHANNEL);
				savepoints[i].charge_sound_playing = false;
			}
			return;
		}
	}
}

void Savepoint_render_minimap(float ship_x, float ship_y,
	float screen_x, float screen_y, float size, float range)
{
	float half_range = range * 0.5f;
	float half_size = size * 0.5f;

	for (int i = 0; i < savepointCount; i++) {
		SavepointState *sp = &savepoints[i];
		if (!sp->active) continue;

		float dx = (float)sp->position.x - ship_x;
		float dy = (float)sp->position.y - ship_y;

		if (dx < -half_range || dx > half_range ||
		    dy < -half_range || dy > half_range)
			continue;

		float mx = screen_x + half_size + (dx / half_range) * half_size;
		float my = screen_y + half_size - (dy / half_range) * half_size;

		/* Cyan dot */
		float s = 2.5f;
		Render_quad_absolute(mx - s, my - s, mx + s, my + s,
			0.0f, 0.9f, 0.9f, 1.0f);
	}
}
