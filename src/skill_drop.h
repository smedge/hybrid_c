#ifndef SKILL_DROP_H
#define SKILL_DROP_H

#include "position.h"
#include "progression.h"
#include <stdbool.h>

void SkillDrop_initialize(void);
void SkillDrop_cleanup(void);
void SkillDrop_spawn(Position position, SubroutineId sub_id);
void SkillDrop_spawn_persistent(Position position, SubroutineId sub_id);
void SkillDrop_update(unsigned int ticks);
void SkillDrop_render(void);
void SkillDrop_render_bloom_source(void);
void SkillDrop_deactivate_all(void);
bool SkillDrop_any_persistent_active(void);

#endif
