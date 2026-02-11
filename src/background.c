#include "background.h"

#include <math.h>
#include <stdlib.h>
#include "render.h"
#include "view.h"
#include "graphics.h"

#define BLOBS_PER_CLOUD 3
#define NUM_LAYERS 3
#define MAX_CLOUDS 20
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

typedef struct {
	Cloud clouds[MAX_CLOUDS];
	int cloud_count;
	float tile_size;
	float parallax;
	float alpha_mult;
} Layer;

static Layer layers[NUM_LAYERS];
static float bg_time = 0.0f;

/* Purple hue palette */
static const float palette[][3] = {
	{0.35f, 0.10f, 0.55f},  /* violet */
	{0.20f, 0.10f, 0.50f},  /* blue-purple */
	{0.45f, 0.08f, 0.40f},  /* red-purple */
	{0.15f, 0.05f, 0.35f},  /* dark violet */
};

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

void Background_initialize(void)
{
	/* Different tile sizes per layer so repeats never align */
	float tile_sizes[NUM_LAYERS] = {14000.0f, 17000.0f, 21000.0f};
	int cloud_counts[NUM_LAYERS] = {18, 15, 12};
	float parallax_values[NUM_LAYERS] = {0.05f, 0.15f, 0.30f};
	float alpha_mults[NUM_LAYERS] = {0.5f, 0.8f, 1.0f};

	bg_time = 0.0f;

	for (int l = 0; l < NUM_LAYERS; l++) {
		layers[l].tile_size = tile_sizes[l];
		layers[l].cloud_count = cloud_counts[l];
		layers[l].parallax = parallax_values[l];
		layers[l].alpha_mult = alpha_mults[l];

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
	bg_time += (float)ticks / 1000.0f;
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

static void render_layer_clouds(int layer_idx, float cam_x, float cam_y,
	float half_vw, float half_vh)
{
	Layer *layer = &layers[layer_idx];
	float p = layer->parallax;
	float am = layer->alpha_mult;
	float ts = layer->tile_size;

	float eff_cx = cam_x * p;
	float eff_cy = cam_y * p;

	float view_min_x = eff_cx - half_vw;
	float view_max_x = eff_cx + half_vw;
	float view_min_y = eff_cy - half_vh;
	float view_max_y = eff_cy + half_vh;

	int tile_min_x = (int)floorf(view_min_x / ts);
	int tile_max_x = (int)floorf(view_max_x / ts);
	int tile_min_y = (int)floorf(view_min_y / ts);
	int tile_max_y = (int)floorf(view_max_y / ts);

	for (int tx = tile_min_x; tx <= tile_max_x; tx++) {
		for (int ty = tile_min_y; ty <= tile_max_y; ty++) {
			float tile_ox = (float)tx * ts;
			float tile_oy = (float)ty * ts;

			for (int c = 0; c < layer->cloud_count; c++) {
				for (int b = 0; b < BLOBS_PER_CLOUD; b++) {
					Blob *blob = &layer->clouds[c].blobs[b];

					float dx = fmodf(blob->drift_dx * bg_time, ts);
					float dy = fmodf(blob->drift_dy * bg_time, ts);
					float wx = blob->x + tile_ox + cam_x * (1.0f - p) + dx;
					float wy = blob->y + tile_oy + cam_y * (1.0f - p) + dy;

					float pulse = 1.0f + 0.08f * sinf(
						bg_time * blob->pulse_speed + blob->pulse_phase);
					float rad = blob->radius * pulse;

					float alpha = blob->a * am;
					render_blob(blob, wx, wy, rad, alpha);
				}
			}
		}
	}
}

void Background_render(void)
{
	View view = View_get_view();
	Screen screen = Graphics_get_screen();

	float cam_x = (float)view.position.x;
	float cam_y = (float)view.position.y;
	float half_vw = (float)(screen.width / 2.0 / view.scale);
	float half_vh = (float)(screen.height / 2.0 / view.scale);

	for (int l = 0; l < NUM_LAYERS; l++) {
		render_layer_clouds(l, cam_x, cam_y, half_vw, half_vh);
	}
}
