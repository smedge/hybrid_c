#include "skill_drop.h"

#include "graphics.h"
#include "render.h"
#include "view.h"
#include "ship.h"
#include "mat4.h"
#include "audio.h"
#include "progression.h"
#include "skillbar.h"
#include "fragment.h"
#include "batch.h"

#include <math.h>
#include <stdlib.h>

#define MAX_SKILL_DROPS   16
#define DROP_SIZE         80.0f          /* 20% smaller than MAP_CELL_SIZE (100) */
#define DROP_HALF         (DROP_SIZE * 0.5f)
#define DROP_CHAMFER      (DROP_SIZE * 8.0f / 50.0f)  /* same ratio as skillbar */
#define ICON_SCALE        (DROP_SIZE / 50.0f)          /* 1.6 — matches skillbar proportions */
#define BORDER_THICK      3.0f

#define SPIN_DEG_PER_SEC  180.0f
#define MIN_H_SCALE       0.15f

#define LIFETIME_MS       15000
#define FADE_MS           2000
#define ATTRACT_RADIUS    300.0
#define COLLECT_RADIUS    30.0
#define ATTRACT_DUR_MS    800.0
#define ATTRACT_MAX_SPEED 1500.0
#define DRIFT_DAMPEN      0.97

#define FLASH_DURATION_MS 300
#define FLASH_SEGMENTS    16

#define SD_PI 3.14159265358979323846

typedef enum {
	SD_IDLE,
	SD_ATTRACTING,
	SD_COLLECTED
} SkillDropState;

typedef struct {
	bool active;
	Position position;
	double vel_x, vel_y;
	SubroutineId sub_id;
	SkillDropState state;
	unsigned int timer;
	unsigned int attractTimer;
	unsigned int flashTimer;
	float alpha;
	bool persistent;
} SkillDrop;

static SkillDrop drops[MAX_SKILL_DROPS];
static Mix_Chunk *collectSample = NULL;

void SkillDrop_initialize(void)
{
	for (int i = 0; i < MAX_SKILL_DROPS; i++)
		drops[i].active = false;

	Audio_load_sample(&collectSample, "resources/sounds/samus_pickup2.wav");
}

void SkillDrop_cleanup(void)
{
	for (int i = 0; i < MAX_SKILL_DROPS; i++)
		drops[i].active = false;

	Audio_unload_sample(&collectSample);
}

void SkillDrop_spawn(Position position, SubroutineId sub_id)
{
	int slot = -1;
	for (int i = 0; i < MAX_SKILL_DROPS; i++) {
		if (!drops[i].active) { slot = i; break; }
	}
	if (slot < 0) slot = 0;

	SkillDrop *d = &drops[slot];
	d->active = true;
	d->position = position;
	d->vel_x = (rand() % 60) - 30;
	d->vel_y = 15.0 + ((rand() % 20) - 10);
	d->sub_id = sub_id;
	d->state = SD_IDLE;
	d->timer = 0;
	d->attractTimer = 0;
	d->flashTimer = 0;
	d->alpha = 1.0f;
	d->persistent = false;
}

void SkillDrop_spawn_persistent(Position position, SubroutineId sub_id)
{
	SkillDrop_spawn(position, sub_id);

	/* Find the just-spawned drop and mark persistent + no drift */
	for (int i = 0; i < MAX_SKILL_DROPS; i++) {
		SkillDrop *d = &drops[i];
		if (d->active && d->sub_id == sub_id && d->timer == 0) {
			d->persistent = true;
			d->vel_x = 0;
			d->vel_y = 0;
			break;
		}
	}
}

void SkillDrop_update(unsigned int ticks)
{
	Position shipPos = Ship_get_position();
	double dt = ticks / 1000.0;

	for (int i = 0; i < MAX_SKILL_DROPS; i++) {
		SkillDrop *d = &drops[i];
		if (!d->active) continue;

		d->timer += ticks;

		/* Collected flash animation */
		if (d->state == SD_COLLECTED) {
			d->flashTimer += ticks;
			if (d->flashTimer >= FLASH_DURATION_MS)
				d->active = false;
			continue;
		}

		/* Lifetime expiry (persistent drops never expire) */
		if (!d->persistent && d->timer >= LIFETIME_MS) {
			d->active = false;
			continue;
		}

		/* Fade in last 2 seconds */
		if (!d->persistent) {
			unsigned int remaining = LIFETIME_MS - d->timer;
			d->alpha = (remaining < FADE_MS) ? (float)remaining / FADE_MS : 1.0f;
		} else {
			d->alpha = 1.0f;
		}

		/* Distance to ship */
		double dx = shipPos.x - d->position.x;
		double dy = shipPos.y - d->position.y;
		double dist = sqrt(dx * dx + dy * dy);

		if (d->state == SD_IDLE) {
			d->vel_x *= DRIFT_DAMPEN;
			d->vel_y *= DRIFT_DAMPEN;
			d->position.x += d->vel_x * dt;
			d->position.y += d->vel_y * dt;

			if (dist < ATTRACT_RADIUS) {
				d->state = SD_ATTRACTING;
				d->attractTimer = 0;
			}
		} else {
			/* SD_ATTRACTING */
			d->attractTimer += ticks;
			double t = d->attractTimer / ATTRACT_DUR_MS;
			if (t > 1.0) t = 1.0;

			double speed = ATTRACT_MAX_SPEED * t * t;
			if (dist > 0.001) {
				d->vel_x = (dx / dist) * speed;
				d->vel_y = (dy / dist) * speed;
			}

			d->position.x += d->vel_x * dt;
			d->position.y += d->vel_y * dt;

			/* Recompute distance after movement */
			dx = shipPos.x - d->position.x;
			dy = shipPos.y - d->position.y;
			dist = sqrt(dx * dx + dy * dy);

			if (dist < COLLECT_RADIUS) {
				/* Immediately unlock the skill via fragment count */
				FragmentType ft = Progression_get_frag_type(d->sub_id);
				int threshold = Progression_get_threshold(d->sub_id);
				Fragment_set_count(ft, threshold);

				Audio_play_sample(&collectSample);

				d->state = SD_COLLECTED;
				d->flashTimer = 0;
			}
		}
	}
}

/* Render hex border + fill at (0,0) in UI convention (y-down) */
static void render_drop_shape(float alpha, SubroutineTier tier, bool bloom_only)
{
	float hs = DROP_HALF;
	float ch = DROP_CHAMFER;
	ColorFloat tc = TIER_COLORS[tier];

	/* Chamfered hexagon — sharp NW + SE, chamfered NE + SW (matches skillbar) */
	float vx[6] = { -hs,      hs - ch,  hs,   hs,      -hs + ch, -hs };
	float vy[6] = { -hs,      -hs,      -hs + ch, hs,  hs,       hs - ch };

	/* Background fill */
	if (!bloom_only) {
		BatchRenderer *batch = Graphics_get_batch();
		for (int v = 0; v < 6; v++) {
			int nv = (v + 1) % 6;
			Batch_push_triangle_vertices(batch,
				0.0f, 0.0f, vx[v], vy[v], vx[nv], vy[nv],
				0.05f, 0.05f, 0.08f, alpha * 0.85f);
		}
	}

	/* Border */
	float thick = bloom_only ? BORDER_THICK * 1.5f : BORDER_THICK;
	float ba = bloom_only ? alpha : alpha * 0.95f;
	for (int v = 0; v < 6; v++) {
		int nv = (v + 1) % 6;
		Render_thick_line(vx[v], vy[v], vx[nv], vy[nv],
			thick, tc.red, tc.green, tc.blue, ba);
	}
}

/* Render spinning drops — shared by main and bloom passes */
static void render_drops(bool bloom_only)
{
	/* Quick check for any active spinning drops */
	bool any = false;
	for (int i = 0; i < MAX_SKILL_DROPS; i++) {
		if (drops[i].active && drops[i].state != SD_COLLECTED) {
			any = true;
			break;
		}
	}
	if (!any) return;

	Screen screen = Graphics_get_screen();
	Mat4 world_proj = Graphics_get_world_projection();
	Mat4 camera_view = View_get_transform(&screen);

	for (int i = 0; i < MAX_SKILL_DROPS; i++) {
		SkillDrop *d = &drops[i];
		if (!d->active || d->state == SD_COLLECTED) continue;

		/* Spin — cosine-based horizontal squash (same trick as data node disk) */
		float t = d->timer / 1000.0f;
		float spin_rad = t * SPIN_DEG_PER_SEC * (float)(SD_PI / 180.0);
		float h_scale = fabsf(cosf(spin_rad));
		if (h_scale < MIN_H_SCALE) h_scale = MIN_H_SCALE;

		/* Flush any pending geometry with standard view */
		Render_flush(&world_proj, &camera_view);

		/* Render hex shape + icon at origin in UI convention */
		SubroutineTier tier = Skillbar_get_tier(d->sub_id);
		render_drop_shape(d->alpha, tier, bloom_only);

		if (!bloom_only) {
			/* Render icon at origin with world-appropriate scale */
			Skillbar_render_icon_at_scale(d->sub_id, 0.0f, 0.0f, d->alpha, ICON_SCALE);
		}

		/* Build custom view: camera * translate(pos) * scale(h_scale, -1, 1)
		   - translate places the drop in the world
		   - scale.x applies the spin squash
		   - scale.y = -1 flips Y from UI convention to world convention */
		Mat4 t_mat = Mat4_translate((float)d->position.x, (float)d->position.y, 0.0f);
		Mat4 s_mat = Mat4_scale(h_scale, -1.0f, 1.0f);
		Mat4 ts = Mat4_multiply(&t_mat, &s_mat);
		Mat4 custom_view = Mat4_multiply(&camera_view, &ts);

		Render_flush(&world_proj, &custom_view);
	}
}

/* Render collection flash — expanding ring in tier color (world space, no transform) */
static void render_collected_flash(void)
{
	for (int i = 0; i < MAX_SKILL_DROPS; i++) {
		SkillDrop *d = &drops[i];
		if (!d->active || d->state != SD_COLLECTED) continue;

		float progress = (float)d->flashTimer / (float)FLASH_DURATION_MS;
		float fade = 1.0f - progress;
		float ring_r = DROP_HALF * (1.0f + progress * 2.0f);

		SubroutineTier tier = Skillbar_get_tier(d->sub_id);
		ColorFloat tc = TIER_COLORS[tier];

		float cx = (float)d->position.x;
		float cy = (float)d->position.y;

		for (int s = 0; s < FLASH_SEGMENTS; s++) {
			float a0 = (float)(2.0 * SD_PI * s / FLASH_SEGMENTS);
			float a1 = (float)(2.0 * SD_PI * (s + 1) / FLASH_SEGMENTS);
			Render_thick_line(
				cx + ring_r * cosf(a0), cy + ring_r * sinf(a0),
				cx + ring_r * cosf(a1), cy + ring_r * sinf(a1),
				3.0f * fade, tc.red, tc.green, tc.blue, fade);
		}
	}
}

void SkillDrop_render(void)
{
	render_drops(false);
	render_collected_flash();
}

void SkillDrop_render_bloom_source(void)
{
	render_drops(true);
}

void SkillDrop_deactivate_all(void)
{
	for (int i = 0; i < MAX_SKILL_DROPS; i++)
		drops[i].active = false;
}

bool SkillDrop_any_persistent_active(void)
{
	for (int i = 0; i < MAX_SKILL_DROPS; i++) {
		if (drops[i].active && drops[i].persistent)
			return true;
	}
	return false;
}
