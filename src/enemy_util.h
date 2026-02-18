#ifndef ENEMY_UTIL_H
#define ENEMY_UTIL_H

#include <stdbool.h>
#include "entity.h"
#include "collision.h"

#define PI 3.14159265358979323846

typedef struct {
	bool hit;
	bool mine_hit;
	bool ambush;
	double damage;
	double mine_damage;
} PlayerDamageResult;

/* Check all player weapons against hitBox, apply ambush multiplier based on
   distance from enemyPos to ship. Returns damage split: non-mine vs mine. */
PlayerDamageResult Enemy_check_player_damage(Rectangle hitBox, Position enemyPos);

/* Call when an enemy is killed by the player. Handles ambush kill rewards. */
void Enemy_on_player_kill(const PlayerDamageResult *dmg);

/* Break stealth if player is within 100 units and inside the enemy's 90° vision cone. */
void Enemy_check_stealth_proximity(Position enemyPos, double facingDegrees);

double Enemy_distance_between(Position a, Position b);
bool Enemy_has_line_of_sight(Position from, Position to);
void Enemy_move_toward(PlaceableComponent *pl, Position target, double speed, double dt, double wallCheckDist);
void Enemy_move_away_from(PlaceableComponent *pl, Position threat, double speed, double dt, double wallCheckDist);
void Enemy_pick_wander_target(Position spawnPoint, double radius, int baseInterval,
	Position *out_target, int *out_timer);

void Enemy_render_death_flash(const PlaceableComponent *pl, float deathTimer, float deathDuration);
void Enemy_render_spark(Position sparkPosition, int sparkTicksLeft, int sparkDuration, float sparkSize, bool sparkShielded, float normalR, float normalG, float normalB);

/* Apply gravity well pull to an enemy. Returns speed multiplier (1.0 if no effect). */
double Enemy_apply_gravity(PlaceableComponent *pl, double dt);

/* Centralized weapon proximity/hit checks — add new weapons here, not in enemy files */
bool Enemy_check_any_nearby(Position pos, double radius);
bool Enemy_check_any_hit(Rectangle hitBox);

#endif
