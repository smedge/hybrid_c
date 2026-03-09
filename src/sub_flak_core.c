#include "sub_flak_core.h"
#include "position.h"
#include "audio.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Tuning: 9 pellets, 15° cone, 250ms cooldown, 667ms range (2/3 of tgun) */
static const SubFlakConfig flakConfig = {
	.pellets_per_shot = 9,
	.spread_half_angle_deg = 7.5,
	.fire_cooldown_ms = 250,
	.feedback_cost = 4.0f,
	.proj = {
		.fire_cooldown_ms = 250, /* unused by flak — cooldown managed externally */
		.velocity = 3500.0,
		.ttl_ms = 667,
		.pool_size = 48,
		.damage = 25.0,
		/* Orange fire visuals */
		.color_r = 1.0f, .color_g = 0.5f, .color_b = 0.1f,
		.trail_thickness = 2.5f,
		.trail_alpha = 0.7f,
		.point_size = 6.0,
		.min_point_size = 2.0,
		.spark_duration_ms = 100,
		.spark_size = 12.0f,
		/* Orange light */
		.light_proj_radius = 50.0f,
		.light_proj_r = 1.0f, .light_proj_g = 0.5f, .light_proj_b = 0.1f, .light_proj_a = 0.5f,
		.light_spark_radius = 70.0f,
		.light_spark_r = 1.0f, .light_spark_g = 0.4f, .light_spark_b = 0.0f, .light_spark_a = 0.4f,
	},
};

const SubFlakConfig *SubFlak_get_config(void)
{
	return &flakConfig;
}

/* Audio — distinct from pea/mgun fire sound */
static Mix_Chunk *sampleFire = NULL;

void SubFlak_initialize_audio(void)
{
	Audio_load_sample(&sampleFire, "resources/sounds/flak.wav");
}

void SubFlak_cleanup_audio(void)
{
	Audio_unload_sample(&sampleFire);
}

bool SubFlak_try_fire(SubProjectilePool *pool, const SubFlakConfig *cfg,
	Position origin, Position target)
{
	if (pool->cooldownTimer > 0)
		return false;

	pool->cooldownTimer = cfg->fire_cooldown_ms;

	/* Base heading toward target */
	double heading_deg = Position_get_heading(origin, target);
	double base_rad = heading_deg * M_PI / 180.0;
	double spread_rad = cfg->spread_half_angle_deg * M_PI / 180.0;

	/* Spawn pellets with random offsets within the cone + slight speed variation */
	for (int i = 0; i < cfg->pellets_per_shot; i++) {
		double offset = ((double)(rand() % 10000) / 10000.0 - 0.5) * 2.0 * spread_rad;
		double pellet_rad = base_rad + offset;
		SubProjectile_spawn_pellet(pool, origin, pellet_rad);
	}
	/* Apply per-pellet speed variation to freshly spawned pellets (ticksLived == 0) */
	for (int i = 0; i < pool->poolSize; i++) {
		SubProjectile *p = &pool->projectiles[i];
		if (p->active && p->ticksLived == 0) {
			/* +/-10% speed variation for a cloud-of-shot feel */
			p->speedMult = 0.9 + (double)(rand() % 200) / 1000.0;
		}
	}

	Audio_play_sample_at(&sampleFire, origin);
	return true;
}
