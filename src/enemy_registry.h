#ifndef ENEMY_REGISTRY_H
#define ENEMY_REGISTRY_H

#include <stdbool.h>
#include "position.h"

#define MAX_ENEMY_TYPES 16

typedef struct {
	bool (*find_wounded)(Position from, double range, double hp_threshold,
						 Position *out_pos, int *out_index);
	bool (*find_aggro)(Position from, double range, Position *out_pos);
	void (*heal)(int index, double amount);
	void (*alert_nearby)(Position origin, double radius, Position threat);
} EnemyTypeCallbacks;

void EnemyRegistry_clear(void);
int EnemyRegistry_register(EnemyTypeCallbacks callbacks);
int EnemyRegistry_type_count(void);
const EnemyTypeCallbacks *EnemyRegistry_get_type(int type_id);
void EnemyRegistry_heal(int type_id, int index, double amount);
void EnemyRegistry_alert_nearby(Position origin, double radius, Position threat);

#endif
