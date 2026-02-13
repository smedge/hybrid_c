#ifndef SUB_BOOST_H
#define SUB_BOOST_H

#include <stdbool.h>
#include "input.h"

void Sub_Boost_initialize(void);
void Sub_Boost_cleanup(void);
void Sub_Boost_update(const Input *input, unsigned int ticks);
bool Sub_Boost_is_boosting(void);
float Sub_Boost_get_cooldown_fraction(void);

#endif
