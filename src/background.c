#include "background.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "render.h"
#include "view.h"
#include "graphics.h"

#define BLOBS_PER_CLOUD 3
#define NUM_LAYERS 3
#define MAX_CLOUDS 44
#define IRREG_SEGS 12

typedef struct {
	float x, y;
	float radius;
	float r, g, b, a;
	float drift_dx, drift_dy;
	float pulse_phase, pulse_speed;
	float vert_mult[IRREG_SEGS];
} Blob;

typedef struct {
	Blob blobs[BLOBS_PER_CLOUD];
} Cloud;

/* Directional drift constants */
#define DRIFT_SPEED 60.0f       /* world units per second */
#define DRIFT_CHANGE_MIN 20.0f  /* min seconds before direction change */
#define DRIFT_CHANGE_MAX 45.0f  /* max seconds before direction change */
#define DRIFT_TURN_TIME 3.0f    /* seconds to smoothly transition direction */

typedef struct {
	float accum_x, accum_y;
	float dir_x, dir_y;
	float target_x, target_y;
	float turn_t;
	float timer;
	float next_change;
	unsigned int rng;
} LayerDrift;

typedef struct {
	Cloud clouds[MAX_CLOUDS];
	int cloud_count;
	float tile_size;
	float parallax;
	float alpha_mult;
	LayerDrift drift;
} Layer;

static Layer layers[NUM_LAYERS];
static float bg_time = 0.0f;

/* Default purple hue palette */
static const float default_palette[4][3] = {
	{0.35f, 0.10f, 0.55f},  /* violet */
	{0.20f, 0.10f, 0.50f},  /* blue-purple */
	{0.45f, 0.08f, 0.40f},  /* red-purple */
	{0.15f, 0.05f, 0.35f},  /* dark violet */
};

static float palette[4][3] = {
	{0.35f, 0.10f, 0.55f},
	{0.20f, 0.10f, 0.50f},
	{0.45f, 0.08f, 0.40f},
	{0.15f, 0.05f, 0.35f},
};

void Background_set_palette(const float colors[4][3])
{
	memcpy(palette, colors, sizeof(palette));
}

void Background_reset_palette(void)
{
	memcpy(palette, default_palette, sizeof(palette));
}

static unsigned int bg_xorshift(unsigned int *state)
{
	unsigned int s = *state;
	s ^= s << 13;
	s ^= s >> 17;
	s ^= s << 5;
	*state = s;
	return s;
}

static float bg_rand_range(unsigned int *state, float min, float max)
{
	unsigned int r = bg_xorshift(state);
	return min + (float)(r % 10000) / 10000.0f * (max - min);
}

static void pick_drift_direction(LayerDrift *d)
{
	static const float dirs[8][2] = {
		{ 0.0f,  1.0f}, { 0.707f,  0.707f}, { 1.0f, 0.0f}, { 0.707f, -0.707f},
		{ 0.0f, -1.0f}, {-0.707f, -0.707f}, {-1.0f, 0.0f}, {-0.707f,  0.707f},
	};
	int idx = bg_xorshift(&d->rng) % 8;
	d->target_x = dirs[idx][0];
	d->target_y = dirs[idx][1];
	d->turn_t = 0.0f;
	d->next_change = DRIFT_CHANGE_MIN +
		(float)(bg_xorshift(&d->rng) % 10000) / 10000.0f *
		(DRIFT_CHANGE_MAX - DRIFT_CHANGE_MIN);
	d->timer = 0.0f;
}

void Background_initialize(void)
{
	/* Different tile sizes per layer so repeats never align */
	float tile_sizes[NUM_LAYERS] = {14000.0f, 17000.0f, 21000.0f};
	int cloud_counts[NUM_LAYERS] = {43, 36, 29};
	float parallax_values[NUM_LAYERS] = {0.05f, 0.15f, 0.30f};
	float alpha_mults[NUM_LAYERS] = {0.5f, 0.8f, 1.0f};

	bg_time = 0.0f;

	for (int l = 0; l < NUM_LAYERS; l++) {
		layers[l].tile_size = tile_sizes[l];
		layers[l].cloud_count = cloud_counts[l];
		layers[l].parallax = parallax_values[l];
		layers[l].alpha_mult = alpha_mults[l];

		/* Per-layer directional drift */
		LayerDrift *d = &layers[l].drift;
		d->rng = (unsigned int)rand() * 2654435761u + (unsigned int)(l + 1);
		if (d->rng == 0) d->rng = 1;
		d->accum_x = 0.0f;
		d->accum_y = 0.0f;
		pick_drift_direction(d);
		d->dir_x = d->target_x;
		d->dir_y = d->target_y;
		d->turn_t = 1.0f;

		float ts = tile_sizes[l];

		unsigned int seed = (unsigned int)(rand()) * 2654435761u + (unsigned int)(l + 1);
		if (seed == 0) seed = 1;

		for (int c = 0; c < cloud_counts[l]; c++) {
			float cx = bg_rand_range(&seed, 0.0f, ts);
			float cy = bg_rand_range(&seed, 0.0f, ts);
			int pal = bg_xorshift(&seed) % 4;

			for (int b = 0; b < BLOBS_PER_CLOUD; b++) {
				float ox = bg_rand_range(&seed, -300.0f, 300.0f);
				float oy = bg_rand_range(&seed, -300.0f, 300.0f);
				float rad = bg_rand_range(&seed, 500.0f, 1500.0f);
				float alpha = bg_rand_range(&seed, 0.03f, 0.08f);

				Blob *blob = &layers[l].clouds[c].blobs[b];
				blob->x = cx + ox;
				blob->y = cy + oy;
				blob->radius = rad;
				blob->r = palette[pal][0];
				blob->g = palette[pal][1];
				blob->b = palette[pal][2];
				blob->a = alpha;
				blob->drift_dx = bg_rand_range(&seed, -2.0f, 2.0f);
				blob->drift_dy = bg_rand_range(&seed, -2.0f, 2.0f);
				blob->pulse_phase = bg_rand_range(&seed, 0.0f, 6.283f);
				blob->pulse_speed = bg_rand_range(&seed, 0.10f, 0.25f);

				/* Generate irregular vertex radii and smooth once */
				float raw[IRREG_SEGS];
				for (int v = 0; v < IRREG_SEGS; v++)
					raw[v] = bg_rand_range(&seed, 0.65f, 1.35f);
				for (int v = 0; v < IRREG_SEGS; v++) {
					int prev = (v + IRREG_SEGS - 1) % IRREG_SEGS;
					int next = (v + 1) % IRREG_SEGS;
					blob->vert_mult[v] = 0.25f * raw[prev]
						+ 0.50f * raw[v] + 0.25f * raw[next];
				}
			}
		}
	}
}

void Background_update(unsigned int ticks)
{
	float dt = (float)ticks / 1000.0f;
	bg_time += dt;

	for (int l = 0; l < NUM_LAYERS; l++) {
		LayerDrift *d = &layers[l].drift;

		/* Smooth turn toward target direction */
		if (d->turn_t < 1.0f) {
			d->turn_t += dt / DRIFT_TURN_TIME;
			if (d->turn_t > 1.0f) d->turn_t = 1.0f;
			float t = d->turn_t * d->turn_t * (3.0f - 2.0f * d->turn_t);
			d->dir_x += t * (d->target_x - d->dir_x);
			d->dir_y += t * (d->target_y - d->dir_y);
		}

		/* Accumulate positional drift */
		d->accum_x += d->dir_x * DRIFT_SPEED * dt;
		d->accum_y += d->dir_y * DRIFT_SPEED * dt;

		/* Occasionally change direction */
		d->timer += dt;
		if (d->timer >= d->next_change)
			pick_drift_direction(d);
	}
}

static void render_blob(Blob *blob, float wx, float wy, float rad, float alpha)
{
	BatchRenderer *batch = Graphics_get_batch();
	float step = 2.0f * (float)M_PI / (float)IRREG_SEGS;

	for (int i = 0; i < IRREG_SEGS; i++) {
		int j = (i + 1) % IRREG_SEGS;
		float a0 = (float)i * step;
		float a1 = (float)j * step;
		float r0 = rad * blob->vert_mult[i];
		float r1 = rad * blob->vert_mult[j];

		Batch_push_triangle_vertices(batch,
			wx, wy,
			wx + r0 * cosf(a0), wy + r0 * sinf(a0),
			wx + r1 * cosf(a1), wy + r1 * sinf(a1),
			blob->r, blob->g, blob->b, alpha);
	}
}

void Background_render(void)
{
	View v = View_get_view();
	Screen screen = Graphics_get_screen();
	Mat4 proj = Graphics_get_world_projection();

	/* Slow ambient drift — sinusoidal wander (directional slide applied per-layer) */
	float breath_x = sinf(bg_time * 0.017f) * 400.0f
		+ sinf(bg_time * 0.031f) * 250.0f;
	float breath_y = cosf(bg_time * 0.013f) * 350.0f
		+ cosf(bg_time * 0.029f) * 200.0f;

	/* Dampen zoom for parallax depth — background zooms at half rate */
	float default_zoom = 0.5f;
	float ratio = (float)v.scale / default_zoom;
	float bg_scale = default_zoom * powf(ratio, 0.5f);
	Mat4 s = Mat4_scale(bg_scale, bg_scale, 1.0f);

	for (int l = 0; l < NUM_LAYERS; l++) {
		Layer *layer = &layers[l];
		float p = layer->parallax;
		float am = layer->alpha_mult;
		float ts = layer->tile_size;

		/* Per-layer drift = shared breathing + layer's own directional slide */
		float drift_x = breath_x + layer->drift.accum_x;
		float drift_y = breath_y + layer->drift.accum_y;

		/* Build view matrix with this layer's drift */
		double vx = (screen.width / 2.0) - ((v.position.x + drift_x) * bg_scale);
		double vy = (screen.height / 2.0) - ((v.position.y + drift_y) * bg_scale);
		vx = floor(vx + 0.5);
		vy = floor(vy + 0.5);
		Mat4 t = Mat4_translate((float)vx, (float)vy, 0.0f);
		Mat4 base_view = Mat4_multiply(&t, &s);

		float cam_x = (float)v.position.x + drift_x;
		float cam_y = (float)v.position.y + drift_y;
		float half_vw = (float)(screen.width / 2.0 / bg_scale);
		float half_vh = (float)(screen.height / 2.0 / bg_scale);

		/* Push all blobs once at base tile position (no tile offset) */
		for (int c = 0; c < layer->cloud_count; c++) {
			for (int b = 0; b < BLOBS_PER_CLOUD; b++) {
				Blob *blob = &layer->clouds[c].blobs[b];
				float dx = fmodf(blob->drift_dx * bg_time, ts);
				float dy = fmodf(blob->drift_dy * bg_time, ts);
				float wx = blob->x + cam_x * (1.0f - p) + dx;
				float wy = blob->y + cam_y * (1.0f - p) + dy;

				float pulse = 1.0f + 0.08f * sinf(
					bg_time * blob->pulse_speed + blob->pulse_phase);
				float rad = blob->radius * pulse;
				float alpha = blob->a * am;
				render_blob(blob, wx, wy, rad, alpha);
			}
		}

		/* Compute visible tile range (margin accounts for blob radius + bloom bleed) */
		float margin = 5000.0f;
		float eff_cx = cam_x * p;
		float eff_cy = cam_y * p;
		int tile_min_x = (int)floorf((eff_cx - half_vw - margin) / ts);
		int tile_max_x = (int)floorf((eff_cx + half_vw + margin) / ts);
		int tile_min_y = (int)floorf((eff_cy - half_vh - margin) / ts);
		int tile_max_y = (int)floorf((eff_cy + half_vh + margin) / ts);

		/* Upload geometry once, redraw per tile with offset view */
		int first = 1;
		for (int tx = tile_min_x; tx <= tile_max_x; tx++) {
			for (int ty = tile_min_y; ty <= tile_max_y; ty++) {
				Mat4 offset = Mat4_translate(
					(float)tx * ts, (float)ty * ts, 0.0f);
				Mat4 tile_view = Mat4_multiply(&base_view, &offset);

				if (first) {
					Render_flush_keep(&proj, &tile_view);
					first = 0;
				} else {
					Render_redraw(&proj, &tile_view);
				}
			}
		}
		Render_clear();
	}
}
