#include "sub_emp.h"

#include "sub_emp_core.h"
#include "skillbar.h"
#include "progression.h"
#include "ship.h"
#include "player_stats.h"
#include "enemy_registry.h"
#include "position.h"

#define EMP_FEEDBACK_COST 30.0
#define EMP_DURATION_MS 10000

static const SubEmpConfig playerEmpCfg = {
	.cooldown_ms = 30000,
	.visual_ms = 167,
	.half_size = 400.0f,
	.ring_max_radius = 400.0f,
	.inner_ring_ratio = 0.6f,
	.segments = 24,
	.outer_r = 0.5f, .outer_g = 0.8f, .outer_b = 1.0f,
	.outer_thickness = 3.0f,
	.inner_r = 0.8f, .inner_g = 0.95f, .inner_b = 1.0f,
	.inner_thickness = 2.0f,
	.inner_alpha_mult = 0.8f,
};

static SubEmpCore empCore;
static bool keyWasDown;

void Sub_Emp_initialize(void)
{
	SubEmp_init(&empCore);
	keyWasDown = false;
	SubEmp_initialize_audio();
}

void Sub_Emp_cleanup(void)
{
	SubEmp_cleanup_audio();
}

void Sub_Emp_try_activate(void)
{
	if (Ship_is_destroyed())
		return;
	if (empCore.cooldownMs > 0)
		return;
	if (Skillbar_find_equipped_slot(SUB_ID_EMP) < 0)
		return;

	Position center = Ship_get_position();
	if (SubEmp_try_activate(&empCore, &playerEmpCfg, center)) {
		PlayerStats_add_feedback(EMP_FEEDBACK_COST);
		EnemyRegistry_apply_emp(center, playerEmpCfg.half_size, EMP_DURATION_MS);
	}
}

void Sub_Emp_update(const Input *input, unsigned int ticks)
{
	if (Ship_is_destroyed()) {
		empCore.visualActive = false;
		return;
	}

	SubEmp_update(&empCore, &playerEmpCfg, ticks);

	/* V key activation (edge-detect) */
	bool keyDown = input->keyV && Skillbar_find_equipped_slot(SUB_ID_EMP) >= 0;
	if (keyDown && !keyWasDown && empCore.cooldownMs <= 0)
		Sub_Emp_try_activate();
	keyWasDown = keyDown;
}

bool Sub_Emp_is_active(void)
{
	return SubEmp_is_active(&empCore);
}

void Sub_Emp_render(void)
{
	SubEmp_render_ring(&empCore, &playerEmpCfg);
}

void Sub_Emp_render_bloom_source(void)
{
	SubEmp_render_bloom(&empCore, &playerEmpCfg);
}

void Sub_Emp_render_light_source(void)
{
	SubEmp_render_light(&empCore, &playerEmpCfg);
}

float Sub_Emp_get_cooldown_fraction(void)
{
	return SubEmp_get_cooldown_fraction(&empCore, &playerEmpCfg);
}
