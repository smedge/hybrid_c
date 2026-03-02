#ifndef SUB_SPRINT_CORE_H
#define SUB_SPRINT_CORE_H

#include <stdbool.h>

typedef struct {
	int duration_ms;
	int cooldown_ms;
} SubSprintConfig;

typedef struct {
	bool active;
	int sprintTimer;
	int cooldownMs;
} SubSprintCore;

void SubSprint_init(SubSprintCore *core);
bool SubSprint_try_activate(SubSprintCore *core, const SubSprintConfig *cfg);
bool SubSprint_update(SubSprintCore *core, const SubSprintConfig *cfg, unsigned int ticks);
bool SubSprint_is_active(const SubSprintCore *core);
float SubSprint_get_cooldown_fraction(const SubSprintCore *core, const SubSprintConfig *cfg);

#endif
