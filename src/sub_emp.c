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
	if (SubEmp_try_activate(&empCore, SubEmp_get_config(), center)) {
		PlayerStats_add_feedback(EMP_FEEDBACK_COST);
		EnemyRegistry_apply_emp(center, SubEmp_get_config()->half_size, EMP_DURATION_MS);
	}
}

void Sub_Emp_update(const Input *input, unsigned int ticks)
{
	if (Ship_is_destroyed()) {
		empCore.visualActive = false;
		return;
	}

	SubEmp_update(&empCore, SubEmp_get_config(), ticks);

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
	SubEmp_render_ring(&empCore, SubEmp_get_config());
}

void Sub_Emp_render_bloom_source(void)
{
	SubEmp_render_bloom(&empCore, SubEmp_get_config());
}

void Sub_Emp_render_light_source(void)
{
	SubEmp_render_light(&empCore, SubEmp_get_config());
}

float Sub_Emp_get_cooldown_fraction(void)
{
	return SubEmp_get_cooldown_fraction(&empCore, SubEmp_get_config());
}
