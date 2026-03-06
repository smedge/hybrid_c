#include "sub_ember_core.h"
#include "audio.h"

/* Tuning: single source of truth */
static const SubEmberConfig emberConfig = {
	.proj = {
		.fire_cooldown_ms = 300,
		.velocity = 3500.0,
		.ttl_ms = 600,
		.pool_size = 16,
		.damage = 15.0,
		/* Orange-red fire visuals */
		.color_r = 1.0f, .color_g = 0.45f, .color_b = 0.1f,
		.trail_thickness = 3.0f,
		.trail_alpha = 0.8f,
		.point_size = 7.0,
		.min_point_size = 2.0,
		.spark_duration_ms = 120,
		.spark_size = 14.0f,
		/* Orange light */
		.light_proj_radius = 60.0f,
		.light_proj_r = 1.0f, .light_proj_g = 0.45f, .light_proj_b = 0.1f, .light_proj_a = 0.5f,
		.light_spark_radius = 80.0f,
		.light_spark_r = 1.0f, .light_spark_g = 0.4f, .light_spark_b = 0.0f, .light_spark_a = 0.4f,
	},
	.fire_cooldown_ms = 300,
	.feedback_cost = 3.0f,
	.ignite_radius = 100.0f,
};

const SubEmberConfig *SubEmber_get_config(void)
{
	return &emberConfig;
}

/* --- AOE ignite burst buffer --- */

#define BURST_MAX 16
static Position burstPositions[BURST_MAX];
static int burstCount = 0;

void SubEmber_add_burst(Position impact)
{
	if (burstCount < BURST_MAX)
		burstPositions[burstCount++] = impact;
}

int SubEmber_check_burst(Rectangle target)
{
	float radius = emberConfig.ignite_radius;
	float r2 = radius * radius;
	int hits = 0;

	for (int i = 0; i < burstCount; i++) {
		/* Closest point on AABB to burst center */
		double cx = burstPositions[i].x;
		double cy = burstPositions[i].y;

		if (cx < target.aX) cx = target.aX;
		else if (cx > target.bX) cx = target.bX;

		if (cy < target.bY) cy = target.bY;
		else if (cy > target.aY) cy = target.aY;

		double dx = burstPositions[i].x - cx;
		double dy = burstPositions[i].y - cy;
		if (dx * dx + dy * dy <= (double)r2)
			hits++;
	}

	return hits;
}

void SubEmber_clear_bursts(void)
{
	burstCount = 0;
}

/* --- Audio --- */

static Mix_Chunk *sampleImpact = NULL;

void SubEmber_initialize_audio(void)
{
	Audio_load_sample(&sampleImpact, "resources/sounds/flak.wav");
}

void SubEmber_cleanup_audio(void)
{
	Audio_unload_sample(&sampleImpact);
}
