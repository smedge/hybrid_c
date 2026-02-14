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

void Sub_Boost_update(const Input *input, unsigned int ticks)
{
	boosting = input->keyLShift && Skillbar_is_active(SUB_ID_BOOST);
}

bool Sub_Boost_is_boosting(void)
{
	return boosting;
}

float Sub_Boost_get_cooldown_fraction(void)
{
	return 0.0f;
}
