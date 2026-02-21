#ifndef FOG_OF_WAR_H
#define FOG_OF_WAR_H

#include <stdbool.h>
#include "entity.h"

void FogOfWar_initialize(void);
void FogOfWar_cleanup(void);
void FogOfWar_reset(void);
void FogOfWar_update(Position player_pos);
void FogOfWar_reveal_all(void);
bool FogOfWar_is_revealed(int gx, int gy);
void FogOfWar_save_to_file(void);
void FogOfWar_load_from_file(void);
void FogOfWar_set_enabled(bool enabled);
bool FogOfWar_get_enabled(void);

#endif
