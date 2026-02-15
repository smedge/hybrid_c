#ifndef SUB_MEND_H
#define SUB_MEND_H

#include "input.h"

void Sub_Mend_initialize(void);
void Sub_Mend_cleanup(void);
void Sub_Mend_update(const Input *input, unsigned int ticks);
float Sub_Mend_get_cooldown_fraction(void);

#endif
