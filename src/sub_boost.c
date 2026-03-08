#include "sub_boost.h"

#include "skillbar.h"
#include "progression.h"

static bool boosting;

void Sub_Boost_initialize(void)
{
	boosting = false;
}

void Sub_Boost_cleanup(void)
{
}

void Sub_Boost_try_activate(void)
{
	boosting = !boosting;
}

void Sub_Boost_update(const Input *input, unsigned int ticks)
{
	(void)input;
	(void)ticks;

	/* Deactivate if skill is unequipped */
	if (boosting && !Skillbar_is_active(SUB_ID_BOOST))
		boosting = false;
}

bool Sub_Boost_is_boosting(void)
{
	return boosting;
}

float Sub_Boost_get_cooldown_fraction(void)
{
	return 0.0f;
}
