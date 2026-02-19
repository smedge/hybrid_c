#ifndef SUB_PROJECTILE_CORE_H
#define SUB_PROJECTILE_CORE_H

#include <stdbool.h>
#include "position.h"
#include "collision.h"

#include <SDL2/SDL_mixer.h>

#define SUB_PROJ_MAX_POOL 64

typedef struct {
	bool active;
	Position position;
	Position prevPosition;
	double headingSin;
	double headingCos;
	int ticksLived;
} SubProjectile;

typedef struct {
	SubProjectile projectiles[SUB_PROJ_MAX_POOL];
	int poolSize;
	int cooldownTimer;
	bool sparkActive;
	Position sparkPosition;
	int sparkTicksLeft;
} SubProjectilePool;

typedef struct {
	int fire_cooldown_ms;
	double velocity;
	int ttl_ms;
	int pool_size;
	double damage;
	/* Visuals */
	float color_r, color_g, color_b;
	float trail_thickness;
	float trail_alpha;
	double point_size;
	double min_point_size;
	/* Spark */
	int spark_duration_ms;
	float spark_size;
	/* Light */
	float light_proj_radius;
	float light_proj_r, light_proj_g, light_proj_b, light_proj_a;
	float light_spark_radius;
	float light_spark_r, light_spark_g, light_spark_b, light_spark_a;
	/* Audio */
	Mix_Chunk **sample_fire;
	Mix_Chunk **sample_hit;
} SubProjectileConfig;

void SubProjectile_pool_init(SubProjectilePool *pool, int poolSize);
bool SubProjectile_try_fire(SubProjectilePool *pool, const SubProjectileConfig *cfg,
	Position origin, Position target);
void SubProjectile_update(SubProjectilePool *pool, const SubProjectileConfig *cfg, unsigned int ticks);
double SubProjectile_check_hit(SubProjectilePool *pool, const SubProjectileConfig *cfg, Rectangle target);
bool SubProjectile_check_nearby(const SubProjectilePool *pool, Position pos, double radius);
void SubProjectile_deactivate_all(SubProjectilePool *pool);
float SubProjectile_get_cooldown_fraction(const SubProjectilePool *pool, const SubProjectileConfig *cfg);

void SubProjectile_render(const SubProjectilePool *pool, const SubProjectileConfig *cfg);
void SubProjectile_render_bloom(const SubProjectilePool *pool, const SubProjectileConfig *cfg);
void SubProjectile_render_light(const SubProjectilePool *pool, const SubProjectileConfig *cfg);

#endif
