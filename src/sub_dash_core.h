#ifndef SUB_DASH_CORE_H
#define SUB_DASH_CORE_H

#include <stdbool.h>

typedef struct {
	bool active;
	int dashTimeLeft;
	int cooldownMs;
	double dirX;
	double dirY;
	bool hitThisDash;
} SubDashCore;

typedef struct {
	int duration_ms;
	double speed;
	int cooldown_ms;
	double damage;
} SubDashConfig;

void SubDash_initialize_audio(void);
void SubDash_cleanup_audio(void);

void SubDash_init(SubDashCore *core);
bool SubDash_try_activate(SubDashCore *core, const SubDashConfig *cfg, double dirX, double dirY);
bool SubDash_update(SubDashCore *core, const SubDashConfig *cfg, unsigned int ticks);
void SubDash_end_early(SubDashCore *core, const SubDashConfig *cfg);
bool SubDash_is_active(const SubDashCore *core);
float SubDash_get_cooldown_fraction(const SubDashCore *core, const SubDashConfig *cfg);

#endif
