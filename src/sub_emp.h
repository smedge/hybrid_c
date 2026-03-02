#ifndef SUB_EMP_H
#define SUB_EMP_H

#include <stdbool.h>
#include "input.h"

void Sub_Emp_initialize(void);
void Sub_Emp_cleanup(void);
void Sub_Emp_update(const Input *input, unsigned int ticks);
void Sub_Emp_render(void);
void Sub_Emp_render_bloom_source(void);
void Sub_Emp_render_light_source(void);
float Sub_Emp_get_cooldown_fraction(void);
void Sub_Emp_try_activate(void);
bool Sub_Emp_is_active(void);

#endif
