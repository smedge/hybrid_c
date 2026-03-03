#ifndef SUB_EMP_CORE_H
#define SUB_EMP_CORE_H

#include <stdbool.h>
#include "position.h"

typedef struct {
	int cooldown_ms;
	int visual_ms;
	float half_size;
	float ring_max_radius;
	float inner_ring_ratio;
	int segments;
	float outer_r, outer_g, outer_b;
	float outer_thickness;
	float inner_r, inner_g, inner_b;
	float inner_thickness;
	float inner_alpha_mult;
} SubEmpConfig;

typedef struct {
	int cooldownMs;
	bool visualActive;
	int visualTimer;
	Position visualCenter;
} SubEmpCore;

void SubEmp_init(SubEmpCore *core);
void SubEmp_update(SubEmpCore *core, const SubEmpConfig *cfg, unsigned int ticks);
bool SubEmp_try_activate(SubEmpCore *core, const SubEmpConfig *cfg, Position center);
bool SubEmp_is_active(const SubEmpCore *core);
float SubEmp_get_cooldown_fraction(const SubEmpCore *core, const SubEmpConfig *cfg);

void SubEmp_initialize_audio(void);
void SubEmp_cleanup_audio(void);

void SubEmp_render_ring(const SubEmpCore *core, const SubEmpConfig *cfg);
void SubEmp_render_bloom(const SubEmpCore *core, const SubEmpConfig *cfg);
void SubEmp_render_light(const SubEmpCore *core, const SubEmpConfig *cfg);

/* Single shared config — same skill, same tuning for player and enemies */
const SubEmpConfig *SubEmp_get_config(void);

#endif
